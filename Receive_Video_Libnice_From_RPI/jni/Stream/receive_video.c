#include "stream.h"
#include "../Login/login.h"

static GMutex gather_mutex, negotiate_mutex;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};

static GThread* connectThread;
//GMutex *mutex;
//GCond *cond;
static gchar *mInfo_SendVideo;
static gchar *RpiInfo_SendVideo;
static gboolean stopThread;
static int flag_trans=0;
static gboolean hasdata = FALSE;

static gchar data_buf[512] = {0};
static int data_len = 0;
struct sockaddr_in addr;
static int sConnect;
static int _video_receive_ClientThread();

void*  _video_receive_main(CustomData *data)
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
	G_CALLBACK( _video_receive_cb_candidate_gathering_done), NULL);
	
	//g_signal_connect(G_OBJECT(agent), "new-selected-pair",
	//G_CALLBACK( _video_receive_cb_new_selected_pair), NULL);

	streamID = nice_agent_add_stream(agent, 1);
	if (streamID == 0)
		g_error("Failed to add stream");
	
	/* Init Gstreamer */
	_video_receive_init_gstreamer(agent, streamID, data);

	/* Start gathering local candidates */
  	if (!nice_agent_gather_candidates(agent, streamID))
    		g_error("Failed to start candidate gathering");
  	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[VIDEO] nice_agent_gather_candidates");

}

/* Listen for element's state change */
static void on_state_changed (GstBus *bus, GstMessage *msg, CustomData *data)
{
	GstState old_state, new_state, pending_state;
	gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

	if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline))
	{
		 gchar *message = g_strdup_printf("%s changed to state %s",  GST_MESSAGE_SRC_NAME(msg), gst_element_state_get_name(new_state));
		__android_log_print (ANDROID_LOG_ERROR, "tutorial-3", "[receive video]%s\n", message);
		g_free (message);
	}
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

void  _video_receive_init_gstreamer(NiceAgent *magent, guint streamID, CustomData *data)
{
	GstElement *pipeline, *nicesrc, *capsfilter, *gstrtpjitterbuffer, *rtph264depay, *ff_decodeh264, *video_view;
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
	gstrtpjitterbuffer = gst_element_factory_make ("gstrtpjitterbuffer", NULL);
	rtph264depay = gst_element_factory_make ("rtph264depay", NULL);
	ff_decodeh264 = gst_element_factory_make ("ffdec_h264", NULL);
	video_view = gst_element_factory_make ("autovideosink", NULL);

	g_object_set (nicesrc, "agent", magent, NULL);
	g_object_set (nicesrc, "stream", streamID, NULL);
	g_object_set (nicesrc, "component", 1, NULL);
	g_object_set (capsfilter, "caps", gst_caps_from_string("application/x-rtp, payload=(int)96"), NULL);

	pipeline = gst_pipeline_new ("test-pipeline");
	if (!pipeline || ! nicesrc || !capsfilter || !gstrtpjitterbuffer || !rtph264depay|| !ff_decodeh264|| !video_view)
	{
		g_printerr ("Not all elements could be created.\n");
		return;
	}

	gst_bin_add_many (GST_BIN (pipeline), nicesrc, capsfilter, gstrtpjitterbuffer, rtph264depay, ff_decodeh264, video_view, NULL);
	if (gst_element_link_many (nicesrc, capsfilter, gstrtpjitterbuffer, rtph264depay, ff_decodeh264, video_view, NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (pipeline);
		return;
	}

	data->pipeline = pipeline;
	gst_element_set_state(data->pipeline, GST_STATE_READY);
	
	data->video_sink = gst_bin_get_by_interface(GST_BIN(data->pipeline), GST_TYPE_X_OVERLAY);
	if (!data->video_sink) {
	GST_ERROR ("Could not retrieve video sink");
	return NULL;
	}

	//Instruct the bus to emit signals for each received message, and connect to the interesting signals
	bus = gst_element_get_bus (data->pipeline);
	bus_source = gst_bus_create_watch (bus);
	g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func, NULL, NULL);
	g_source_attach (bus_source, data->context);
	g_source_unref (bus_source);
	g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)on_error, data);
	g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)on_state_changed, data);
	gst_object_unref (bus);

	//Create a GLib Main Loop and set it to run
	GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
	//data->main_loop = g_main_loop_new (data->context, FALSE);
	check_initialization_complete (data);
}

void  _video_receive_cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data)
{
	gchar *line = NULL;
	int rval;
	int RetVal = 0;
	gboolean ret = TRUE;

	_video_receive_print_local_data(agent, stream_id, 1);

	// Connect to server
//	do
//	{
//		__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[VIDEO] Connecting to server ...");
//		sConnect = connect_with_timeout(SERVER, SERVER_PORT, 5, 0,"ceslab$1234");
//
//	}while(sConnect == -1); // 1 = connect without timeout

	// Wait until another client connect to server
	connectThread = g_thread_create(_video_receive_ClientThread, NULL, FALSE, NULL);
	while (flag_trans != 1)
	{
		usleep(10000);
	}

	rval = _video_receive_parse_remote_data(agent, stream_id, 1, RpiInfo_SendVideo);
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

	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[VIDEO] Gathering done");
	video_receive_gathering_done = TRUE;
}

int  _video_receive_print_local_data(NiceAgent *agent, guint stream_id, guint component_id)
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

	mInfo_SendVideo = (gchar*)malloc(181*sizeof(gchar));

	printf("%s %s", local_ufrag, local_password);
	sprintf(mInfo_SendVideo, "%s %s", local_ufrag, local_password);
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

		sprintf(mInfo_SendVideo + strlen(mInfo_SendVideo), " %s,%u,%s,%u,%s",
		c->foundation,
		c->priority,
		ipaddr,
		nice_address_get_port(&c->addr),
		candidate_type_name[c->type]);
	}
	printf("\n");

	//printf("\nmInfo_SendVideo:\n");
	//printf("%s\n", mInfo_SendVideo);
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

int  _video_receive_parse_remote_data(NiceAgent *agent, guint streamID,
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
			NiceCandidate *c = _video_receive_parse_candidate(line_argv[i], streamID);

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

static NiceCandidate* _video_receive_parse_candidate(char *scand, guint streamID)
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

static int _video_receive_ClientThread()
{
	char *header, *init, *dest, *data;
		char buffer[181] = { 0 };
		char temp[181] = { 0 };
		char combine[181] = { 0 };
		int flag = 0;

		char receiver[181] = {0};
		char sender[181] = {0};
		int rc = 0;

		memcpy(temp, mInfo_SendVideo, sizeof(temp));

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

				__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
										"[VIDEO] receive - decode = %s", receiver);

				header = strtok(receiver, "$");
				init = strtok(NULL, "$");
				dest = strtok(NULL, "$");
				data = strtok(NULL, "$");
				//__android_log_print(ANDROID_LOG_INFO, "tutorial-3",
				//					"[VIDEO] data = %s", data);
				/*if(buffer[0]!='2')
				 {
				 memcpy(mInfo_SendVideo,buffer,sizeof(buffer));
				 flag_trans = 1;
				 break;
				 }
				 else send(sConnect,temp,181,NULL);*/

				/* Receive rpi's info -> send its'info */
				if (!strcmp(header, "002") && flag < 1) {
					//cout<<"002";
					RpiInfo_SendVideo = (gchar*)malloc(sizeof(gchar)*181);
					memcpy(RpiInfo_SendVideo,data, strlen(data));
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
