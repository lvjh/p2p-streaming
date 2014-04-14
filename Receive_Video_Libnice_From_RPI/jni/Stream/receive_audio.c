/*
 * receive_audio.c
 *
 *  Created on: Mar 16, 2014
 *      Author: cxphong
 */

#include "stream.h"
#include "../Login/login.h"

static GMutex gather_mutex, negotiate_mutex;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};

static GThread* connectThread;
static GMutex *mutex;
static GCond *cond;
static gchar *mInfo_ReceiveAudio;
static gchar *RpiInfo_ReceiveAudio;
static gboolean stopThread;
static int flag_trans=0;
static gboolean hasdata = FALSE;

static gchar data_buf[512] = {0};
static int data_len = 0;
struct sockaddr_in addr;
static int sConnect;
static int _receive_audio_ClientThread();

void*  _receive_audio_main(CustomData *data)
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
	g_object_set(G_OBJECT(agent), "controlling-mode", CONTROLLING_MODE, NULL);

	g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",
	G_CALLBACK( _receive_audio_cb_candidate_gathering_done), NULL);

	//g_signal_connect(G_OBJECT(agent), "new-selected-pair",
	//G_CALLBACK( _receive_audio_cb_new_selected_pair), NULL);

	streamID = nice_agent_add_stream(agent, 1);
	if (streamID == 0)
		g_error("Failed to add stream");

	while(video_receive_gathering_done == FALSE)
				usleep(100);

	/* Init Gstreamer */
	_receive_audio_init_gstreamer(agent, streamID, data);

	/* Start gathering local candidates */
  	if (!nice_agent_gather_candidates(agent, streamID))
    		g_error("Failed to start candidate gathering");
  	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[AUDIO_RECEIVE] nice_agent_gather_candidates");

}

void  _receive_audio_init_gstreamer(NiceAgent *magent, guint streamID, CustomData *data)
{
	GstElement *pipeline, *nicesrc, *capsfilter, *rtpspeexdepay, *speexdec, *autoaudiosink;
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

	nicesrc = gst_element_factory_make ("nicesrc", NULL);
	capsfilter = gst_element_factory_make ("capsfilter", NULL);
	rtpspeexdepay = gst_element_factory_make ("rtpspeexdepay", NULL);
	speexdec = gst_element_factory_make ("speexdec", NULL);
	autoaudiosink = gst_element_factory_make ("autoaudiosink", NULL);

	g_object_set (nicesrc, "agent", magent, NULL);
	g_object_set (nicesrc, "stream", streamID, NULL);
	g_object_set (nicesrc, "component", 1, NULL);
	g_object_set (capsfilter, "caps", gst_caps_from_string("application/x-rtp, media=(string)audio, clock-rate=(int)44100, encoding-name=(string)SPEEX, encoding-params=(string)1, payload=(int)110, ssrc=(uint)1647313534, timestamp-offset=(uint)2918479805, seqnum-offset=(uint)26294"), NULL);

	pipeline = gst_pipeline_new ("test-pipeline");
	if (!pipeline || !nicesrc || !capsfilter || !rtpspeexdepay|| !speexdec|| !autoaudiosink)
	{
		g_printerr ("Not all elements could be created.\n");
		return;
	}

	gst_bin_add_many (GST_BIN (pipeline), nicesrc, capsfilter, rtpspeexdepay, speexdec, autoaudiosink, NULL);
	if (gst_element_link_many ( nicesrc, capsfilter, rtpspeexdepay, speexdec, autoaudiosink, NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (pipeline);
		return;
	}

	data->pipeline = pipeline;
	gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

//	data->video_sink = gst_bin_get_by_interface(GST_BIN(data->pipeline), GST_TYPE_X_OVERLAY);
//	if (!data->video_sink) {
//	GST_ERROR ("Could not retrieve video sink");
//	return NULL;
//	}

	//Instruct the bus to emit signals for each received message, and connect to the interesting signals
	bus = gst_element_get_bus (data->pipeline);
	bus_source = gst_bus_create_watch (bus);
	g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func, NULL, NULL);
	g_source_attach (bus_source, data->context);
	g_source_unref (bus_source);
	//g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, data);
	//g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)state_changed_cb, data);
	gst_object_unref (bus);

	//Create a GLib Main Loop and set it to run
	GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
	//data->main_loop = g_main_loop_new (data->context, FALSE);
	//check_initialization_complete (data);
}

void  _receive_audio_cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data)
{
	gchar *line = NULL;
	int rval;
	int RetVal = 0;
	gboolean ret = TRUE;

	_receive_audio_print_local_data(agent, stream_id, 1);

	// Connect to server
//	do
//	{
//		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[VIDEO] Connecting to server ...");
//		sConnect = connect_with_timeout(SERVER, SERVER_PORT, 5, 0); // Timeout = 5s
//
//	}while(sConnect == -1); // 1 = connect without timeout

	// Wait until another client connect to server
	connectThread = g_thread_create(_receive_audio_ClientThread, NULL, FALSE, NULL);
	while (flag_trans != 1)
	{
		usleep(10000);
	}

	rval = _receive_audio_parse_remote_data(agent, stream_id, 1, RpiInfo_ReceiveAudio);
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

	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[AUDIO_RECEIVE] Gathering done");
	receive_audio_gathering_done = TRUE;
}

int  _receive_audio_print_local_data(NiceAgent *agent, guint stream_id, guint component_id)
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

	mInfo_ReceiveAudio = (gchar*)malloc(181*sizeof(gchar));

	printf("%s %s", local_ufrag, local_password);
	sprintf(mInfo_ReceiveAudio, "%s %s", local_ufrag, local_password);
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

//		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", " %s,%u,%s,%u,%s",
//						c->foundation,
//						c->priority,
//						ipaddr,
//						nice_address_get_port(&c->addr),
//						candidate_type_name[c->type]);

		sprintf(mInfo_ReceiveAudio + strlen(mInfo_ReceiveAudio), " %s,%u,%s,%u,%s",
		c->foundation,
		c->priority,
		ipaddr,
		nice_address_get_port(&c->addr),
		candidate_type_name[c->type]);
	}
	printf("\n");

	//printf("\nmInfo_ReceiveAudio:\n");
	//printf("%s\n", mInfo_ReceiveAudio);
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

int  _receive_audio_parse_remote_data(NiceAgent *agent, guint streamID,
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
			NiceCandidate *c = _receive_audio_parse_candidate(line_argv[i], streamID);

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

static NiceCandidate* _receive_audio_parse_candidate(char *scand, guint streamID)
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

static int _receive_audio_ClientThread()
{
	char *header, *init, *dest, *data;
			char buffer[181] = { 0 };
			char temp[181] = { 0 };
			char combine[181] = { 0 };
			int flag = 0;
			char receiver[181] = {0};
			char sender[181] = {0};
			int rc = 0;
			memcpy(temp, mInfo_ReceiveAudio, sizeof(temp));

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
							"[RECEIVE AUDIO] Receive = %s", buffer);
					rc = Base64Decode(buffer, receiver, BUFFFERLEN);
					header = strtok(receiver, "$");
					init = strtok(NULL, "$");
					dest = strtok(NULL, "$");
					data = strtok(NULL, "$");
					//__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
					//					"[VIDEO] data = %s", data);
					/*if(buffer[0]!='2')
					 {
					 memcpy(mInfo_ReceiveAudio,buffer,sizeof(buffer));
					 flag_trans = 1;
					 break;
					 }
					 else send(sConnect,temp,181,NULL);*/

					__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
												"[RECEIVE AUDIO] header = %s, flag = %d", header, flag);
					/* Receive rpi's info -> send its'info */
					if (!strcmp(header, "002") && flag < 1) {
						//cout<<"002";
						RpiInfo_ReceiveAudio = (gchar*)malloc(sizeof(gchar)*181);
						memcpy(RpiInfo_ReceiveAudio, data, strlen(data));
						sprintf(combine, "002$%s$%s$%s", dest, init, temp);
						rc = Base64Encode(combine, sender, BUFFFERLEN);
						send(global_socket, sender, 181, NULL);
						//cout<<data<<endl<<"Trade completed!"<<endl;
						flag_trans = 1;
						flag++;
					}
				}

				usleep(10000);
				break;
			}

			return 0;
}




