#include "stream.h"
#include "../Login/login.h"

static GMutex gather_mutex, negotiate_mutex;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};

GThread* connectThread;
GMutex *mutex;
GCond *cond;
gchar *mInfo;
gchar rpiInfo[181] = {'\0'};
gboolean stopThread;
static int flag_trans=0;
static gboolean hasdata = FALSE;

static gchar data_buf[512] = {0};
static int data_len = 0;
struct sockaddr_in addr;
static int sConnect;
static int _send_audio_ClientThread();
static void
on_error (GstBus     *bus,
          GstMessage *message,
          gpointer    user_data);

void*  _send_audio_main(CustomData *data)
{
	NiceAgent *agent;
	guint streamID = 0;

	/* Init agent */
	agent = nice_agent_new(data->context,
	NICE_COMPATIBILITY_RFC5245);
	if (agent == NULL)
		g_error("Failed to create agent");

	g_object_set(G_OBJECT(agent), "stun-server", STUNSR_ADDR, NULL);
	g_object_set(G_OBJECT(agent), "stun-server-port", STUNSR_PORT, NULL);
	g_object_set(G_OBJECT(agent), "controlling-mode", 0, NULL);

	g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",
	G_CALLBACK( _send_audio_cb_candidate_gathering_done), NULL);
	//g_signal_connect(G_OBJECT(agent), "new-selected-pair",
	//		G_CALLBACK(_send_audio_cb_new_selected_pair), NULL);

	//g_signal_connect(G_OBJECT(agent), "new-selected-pair",
	//G_CALLBACK( _send_audio_cb_new_selected_pair), NULL);

	streamID = nice_agent_add_stream(agent, 1);
	if (streamID == 0)
		g_error("Failed to add stream");

	while(receive_audio_gathering_done == FALSE)
			usleep(100);
	/* Init Gstreamer */
	_send_audio_init_gstreamer(agent, streamID, data);

	nice_agent_attach_recv(agent, streamID, 1,
				data->context, _send_audio_cb_nice_recv, NULL);

	/* Start gathering local candidates */
  	if (!nice_agent_gather_candidates(agent, streamID))
    		g_error("Failed to start candidate gathering");
  	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[SEND_AUDIO] nice_agent_gather_candidates");

}

static void
on_error (GstBus     *bus,
          GstMessage *message,
          gpointer    user_data)
{
	  GError *err;
	  gchar *debug_info;
	  gchar *message_string;

	  gst_message_parse_error (message, &err, &debug_info);
	  message_string = g_strdup_printf ("Error received from element %s: %s", GST_OBJECT_NAME (message->src), err->message);

	  __android_log_print (ANDROID_LOG_INFO, "tutorial-3", "=========================================\n");
	  __android_log_print (ANDROID_LOG_INFO, "tutorial-3", "debug_info = %s \n\n message_string = %s\n", debug_info, message_string);
	  g_clear_error (&err);
	  g_free (debug_info);
	  g_free (message_string);
}

void  _send_audio_init_gstreamer(NiceAgent *magent, guint stream_id, CustomData *data)
{
	GstElement *pipeline, *audiotestsrc, *openslessrc, *audioconvert, *caps, *rtpL16pay, *nicesink;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	GSource *bus_source;

	/* Initialize GStreamer */
  	gst_init (NULL, NULL);

  	//Register gstreamer plugin libnice
	gst_plugin_register_static (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"nice",
		"Interactive UDP connectivity establishment",
		plugin_init, "0.1.4", "LGPL", "libnice",
		"http://telepathy.freedesktop.org/wiki/", "");

	openslessrc = gst_element_factory_make ("openslessrc", NULL);
	audioconvert = gst_element_factory_make ("audioconvert", NULL);
	caps = gst_element_factory_make ("capsfilter", NULL);
	rtpL16pay = gst_element_factory_make ("rtpL16pay", NULL);
	nicesink = gst_element_factory_make ("nicesink", NULL);

	g_object_set (caps, "caps", gst_caps_from_string("audio/x-raw-int, channels=1, rate=16000, payload=96"), NULL);
	//Set properties
	g_object_set (nicesink, "agent", magent, NULL);
	g_object_set (nicesink, "stream", stream_id, NULL);
	g_object_set (nicesink, "component", 1, NULL);


	pipeline = gst_pipeline_new ("Audio pipeline");
	if (!pipeline || !openslessrc || !audioconvert || !caps || !rtpL16pay || !nicesink)
	{
		g_printerr ("Not all elements could be created.\n");
		return;
	}

	gst_bin_add_many (GST_BIN (pipeline), openslessrc, audioconvert, caps, rtpL16pay, nicesink, NULL);
	if (gst_element_link_many (openslessrc, audioconvert, caps, rtpL16pay, nicesink, NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (pipeline);
		return;
	}

	bus = gst_element_get_bus (pipeline);
	gst_bus_enable_sync_message_emission (bus);
	gst_bus_add_signal_watch (bus);

	g_signal_connect (bus, "message::error",
			(GCallback) on_error, NULL);

	data->pipeline = pipeline;
	gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}

void  _send_audio_cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data)
{
	gchar *line = NULL;
		int rval;
		int RetVal = 0;
		gboolean ret = TRUE;

		_send_audio_print_local_data (agent, stream_id, 1);

		// Connect to server
	//	do
	//	{
	//		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[VIDEO] Connecting to server ...");
	//		sConnect = connect_with_timeout(SERVER, SERVER_PORT, 5, 0,"ceslab$1234");
	//
	//	}while(sConnect == -1); // 1 = connect without timeout

		// Wait until another client connect to server
		connectThread = g_thread_create(_send_audio_ClientThread, NULL, FALSE, NULL);
		while (flag_trans != 1)
		{
			usleep(10000);
		}

		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[SEND_AUDIO] rpiInfo = %s", rpiInfo);
		rval = _send_audio_parse_remote_data (agent, stream_id, 1, rpiInfo);
		if (rval == EXIT_SUCCESS) {
			// Return FALSE so we stop listening to stdin since we parsed the
			// candidates correctly
			ret = FALSE;
			g_debug("waiting for state READY or FAILED signal...");
		} else {
			fprintf(stderr, "ERROR: failed to parse remote data\n");
			printf("Enter remote data (single line, no wrapping):\n");
			printf("> ");
			fflush (stdout);
		}

		//int retVal = nice_agent_send(agent, stream_id, 1, 3, "ab");
		//__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[SEND_AUDIO] retVal = %d", retVal);
		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[SEND_AUDIO] Gathering done");
		send_audio_gathering_done = TRUE;
}

int  _send_audio_print_local_data(NiceAgent *agent, guint stream_id, guint component_id)
{
		int result = EXIT_FAILURE;
		gchar *local_ufrag = NULL;
		gchar *local_password = NULL;
		gchar ipaddr[INET6_ADDRSTRLEN];
		GSList *cands = NULL, *item;

		if (!nice_agent_get_local_credentials(agent, stream_id,
			&local_ufrag, &local_password))
		goto end;

		cands = nice_agent_get_local_candidates(agent, stream_id, component_id);
		if (cands == NULL)
			goto end;

		mInfo = (gchar*)malloc(181*sizeof(gchar));

		printf("%s %s", local_ufrag, local_password);
		sprintf(mInfo, "%s %s", local_ufrag, local_password);
		for (item = cands; item; item = item->next) {
			NiceCandidate *c = (NiceCandidate *)item->data;

			nice_address_to_string(&c->addr, ipaddr);

			// (foundation),(prio),(addr),(port),(type)
			printf(" %s,%u,%s,%u,%s",
			c->foundation,
			c->priority,
			ipaddr,
			nice_address_get_port(&c->addr),
			candidate_type_name[c->type]);

//			__android_log_print (ANDROID_LOG_INFO, "tutorial-3", " %s,%u,%s,%u,%s",
//							c->foundation,
//							c->priority,
//							ipaddr,
//							nice_address_get_port(&c->addr),
//							candidate_type_name[c->type]);

			sprintf(mInfo + strlen(mInfo), " %s,%u,%s,%u,%s",
			c->foundation,
			c->priority,
			ipaddr,
			nice_address_get_port(&c->addr),
			candidate_type_name[c->type]);
		}
		printf("\n");

		//printf("\nmInfo:\n");
		//printf("%s\n", mInfo);
		result = EXIT_SUCCESS;

		end:
		if (local_ufrag)
			g_free(local_ufrag);
		if (local_password)
			g_free(local_password);
		if (cands)
			g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);

		return result;
}

int  _send_audio_parse_remote_data(NiceAgent *agent, guint streamID,
    guint component_id, char *line)
{
	GSList *remote_candidates = NULL;
	gchar **line_argv = NULL;
	const gchar *ufrag = NULL;
	const gchar *passwd = NULL;
	int result = EXIT_FAILURE;
	int i;

	line_argv = g_strsplit_set (line, " \t\n", 0);
	for (i = 0; line_argv && line_argv[i]; i++) {
		if (strlen (line_argv[i]) == 0)
			continue;

		// first two args are remote ufrag and password
		if (!ufrag) {
			ufrag = line_argv[i];
		} else if (!passwd) {
			passwd = line_argv[i];
		} else {
			// Remaining args are serialized canidates (at least one is required)
			NiceCandidate *c = _send_audio_parse_candidate(line_argv[i], streamID);

			if (c == NULL) {
				g_message("failed to parse candidate: %s", line_argv[i]);
				goto end;
			}
		remote_candidates = g_slist_prepend(remote_candidates, c);
		}
	}
	if (ufrag == NULL || passwd == NULL || remote_candidates == NULL) {
		g_message("line must have at least ufrag, password, and one candidate");
		goto end;
	}

	if (!nice_agent_set_remote_credentials(agent, streamID, ufrag, passwd)) {
		g_message("failed to set remote credentials");
		goto end;
	}

	// Note: this will trigger the start of negotiation.
	if (nice_agent_set_remote_candidates(agent, streamID, component_id,
	remote_candidates) < 1) {
		g_message("failed to set remote candidates");
		goto end;
	}

	result = EXIT_SUCCESS;

	end:
	if (line_argv != NULL)
	g_strfreev(line_argv);
	if (remote_candidates != NULL)
	g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);

	return result;
}

NiceCandidate* _send_audio_parse_candidate(char *scand, guint streamID)
{
	NiceCandidate *cand = NULL;
	NiceCandidateType ntype;
	gchar **tokens = NULL;
	guint i;

	tokens = g_strsplit (scand, ",", 5);
	for (i = 0; tokens && tokens[i]; i++);
		if (i != 5)
	goto end;

	for (i = 0; i < G_N_ELEMENTS (candidate_type_name); i++) {
		if (strcmp(tokens[4], candidate_type_name[i]) == 0) {
			ntype = i;
			break;
		}
	}
	if (i == G_N_ELEMENTS (candidate_type_name))
		goto end;

	cand = nice_candidate_new(ntype);
	cand->component_id = 1;
	cand->stream_id = streamID;
	cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;
	strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
	cand->priority = atoi (tokens[1]);

	if (!nice_address_set_from_string(&cand->addr, tokens[2])) {
		g_message("failed to parse addr: %s", tokens[2]);
		nice_candidate_free(cand);
		cand = NULL;
		goto end;
	}

	nice_address_set_port(&cand->addr, atoi (tokens[3]));

	end:
	g_strfreev(tokens);

	return cand;
}

void _send_audio_cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
	if (len == 1 && buf[0] == '\0')
	g_main_loop_quit (gloop);
	printf("%.*s", len, buf);
	fflush(stdout);
}

void _send_audio_cb_new_selected_pair(NiceAgent *agent, guint stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data)
{
	g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

static int _send_audio_ClientThread()
{
	char *header, *init, *dest, *data;
			char buffer[181] = { 0 };
			char temp[181] = { 0 };
			char combine[181] = { 0 };
			int flag = 0;
			char receiver[181] = {0};
			char sender[181] = {0};
			int rc = 0;

			memcpy(temp, mInfo, sizeof(temp));

			// Request to connect Rpi
			//send(global_socket, "001$ceslab$khtn", 181, NULL);

			while (1) {
				// The server will send a struct to the client
				// containing message and ID
				// But send only accepts a char as buffer parameter
				// so here we need to recv a char buffer and then
				// we copy the content of this buffer to our struct
				if (recv(global_socket, buffer, 181, NULL)) {
					__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
							"[VIDEO] receive = %s", buffer);
					rc = Base64Decode(buffer, receiver, BUFFFERLEN);
					header = strtok(receiver, "$");
					init = strtok(NULL, "$");
					dest = strtok(NULL, "$");
					data = strtok(NULL, "$");
					__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
										"[VIDEO] data = %s", data);
					/*if(buffer[0]!='2')
					 {
					 memcpy(mInfo,buffer,sizeof(buffer));
					 flag_trans = 1;
					 break;
					 }
					 else send(sConnect,temp,181,NULL);*/

					/* Receive rpi's info -> send its'info */
					if (!strcmp(header, "002") && flag < 1) {
						//cout<<"002";
						//memcpy(mInfo,data,sizeof(data));
						memcpy(rpiInfo,data, strlen(data));
						sprintf(combine, "002$%s$%s$%s", dest, init, temp);
						rc = Base64Encode(combine, sender, BUFFFERLEN);
						send(global_socket, sender, 181, NULL);
						//cout<<data<<endl<<"Trade completed!"<<endl;
						flag_trans = 1;
						flag++;
						break;
					}
				}

				usleep(10000);
			}

			return 0;
}
