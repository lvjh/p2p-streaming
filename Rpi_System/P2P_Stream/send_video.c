#include "stream.h"

static GMutex gather_mutex, negotiate_mutex;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};
static GThread* connectThread;
static GMutex *mutex;
static GCond *cond;
static gchar *mInfo_SendVideo;
static gchar *AndroidInfo_SendVideo;
static gboolean stopThread;
static int flag_trans=0;
static gboolean hasdata = FALSE;

static gchar data_buf[512] = {0};
static int data_len = 0;
struct sockaddr_in addr;
//int sConnect;
static int _video_send_ClientThread();
static void
on_error_video (GstBus     *bus,
          GstMessage *message,
          gpointer    user_data);

void*  _video_send_main()
{
	printf("[send video]\n");
	//NiceAgent *agent;
	//guint streamID = 0;

	//nice_debug_enable(TRUE);
	/* Init agent */
	RpiData_SendVideo->agent = nice_agent_new(g_main_loop_get_context (gloop),
	NICE_COMPATIBILITY_RFC5245);

	if (RpiData_SendVideo->agent == NULL)
		g_error("Failed to create agent");

	g_object_set(G_OBJECT(RpiData_SendVideo->agent), "stun-server", STUNSR_ADDR, NULL);
	g_object_set(G_OBJECT(RpiData_SendVideo->agent), "stun-server-port", STUNSR_PORT, NULL);
	g_object_set(G_OBJECT(RpiData_SendVideo->agent), "controlling-mode", CONTROLLING_MODE, NULL);

	g_signal_connect(G_OBJECT(RpiData_SendVideo->agent), "candidate-gathering-done",
	G_CALLBACK( _video_send_cb_candidate_gathering_done), NULL);

	//g_signal_connect(G_OBJECT(agent), "new-selected-pair",
	//G_CALLBACK( _video_receive_cb_new_selected_pair), NULL);

	RpiData_SendVideo->streamID = nice_agent_add_stream(RpiData_SendVideo->agent, 1);
	if (RpiData_SendVideo->streamID == 0)
		g_error("Failed to add stream");

	/* Init Gstreamer */
	_video_send_init_gstreamer(RpiData_SendVideo->agent, RpiData_SendVideo->streamID);

	nice_agent_attach_recv(RpiData_SendVideo->agent, RpiData_SendVideo->streamID, 1,
      	g_main_loop_get_context (gloop), _video_send_cb_nice_recv, NULL);

	/* Start gathering local candidates */
  	if (!nice_agent_gather_candidates(RpiData_SendVideo->agent, RpiData_SendVideo->streamID))
		g_error("Failed to start candidate gathering");

  	printf("[send video] Start Gathering!\n");
  	printf("[send video] Agent = %d!\n", RpiData_SendVideo->agent);

}

static void
on_error_video (GstBus     *bus,
          GstMessage *message,
          gpointer    user_data)
{
	  GError *err;
	  gchar *debug_info;
	  gchar *message_string;

	  gst_message_parse_error (message, &err, &debug_info);
	  message_string = g_strdup_printf ("Error received from element %s: %s", GST_OBJECT_NAME (message->src), err->message);

	  g_message ("debug_info = %s \n message_string = %s\n", debug_info, message_string);
	  g_clear_error (&err);
	  g_free (debug_info);
	  g_free (message_string);
}

void  _video_send_init_gstreamer(NiceAgent *magent, guint stream_id)
{
	//GstElement *pipeline, *rpicamsrc, *capsfilter, *h264parse, *rtph264pay, *nicesink;
	GstElement *rpicamsrc, *capsfilter, *h264parse, *rtph264pay, *nicesink;
	//GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;

	/* Initialize GStreamer */
  	gst_init (NULL, NULL);

	rpicamsrc = gst_element_factory_make ("rpicamsrc", NULL);
	capsfilter = gst_element_factory_make ("capsfilter", NULL);
	h264parse = gst_element_factory_make ("h264parse", NULL);
	rtph264pay = gst_element_factory_make ("rtph264pay", NULL);
	nicesink = gst_element_factory_make ("nicesink", NULL);

	//rpicamsrc
	g_object_set (rpicamsrc, "bitrate", 5000000, NULL);
	g_object_set (rpicamsrc, "rotation", 180, NULL);
	g_object_set (rpicamsrc, "exposure-mode", 9, NULL);
	g_object_set (rpicamsrc, "video-stabilisation", TRUE, NULL);
	g_object_set (capsfilter, "caps", gst_caps_from_string("video/x-h264,width=1280,height=720,framerate=25/1"), NULL);
	//rtph264pay
	g_object_set (rtph264pay, "pt", 96, NULL);
	g_object_set (rtph264pay, "config-interval", 1, NULL);

	//Set properties
	g_object_set (nicesink, "agent", magent, NULL);
	g_object_set (nicesink, "stream", stream_id, NULL);
	g_object_set (nicesink, "component", 1, NULL);

	/// Create the empty pipeline
	//pipeline = gst_pipeline_new ("test-pipeline");
	RpiData_SendVideo->pipeline = gst_pipeline_new ("send-video-pipeline");

	if (!RpiData_SendVideo->pipeline || !rpicamsrc ||!capsfilter || !h264parse || !rtph264pay|| !nicesink)
	{
		g_printerr ("Not all elements could be created.\n");
		return -1;
	}

	/// Build the pipeline
	gst_bin_add_many (GST_BIN (RpiData_SendVideo->pipeline), rpicamsrc, capsfilter, h264parse, rtph264pay, nicesink, NULL);
	if (gst_element_link_many ( rpicamsrc, capsfilter, h264parse, rtph264pay, nicesink,  NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (RpiData_SendVideo->pipeline);
		return -1;
	}

	RpiData_SendVideo->bus = gst_element_get_bus (RpiData_SendVideo->pipeline);
  	gst_bus_enable_sync_message_emission (RpiData_SendVideo->bus);
  	gst_bus_add_signal_watch (RpiData_SendVideo->bus);

	g_signal_connect (RpiData_SendVideo->bus, "message::error",
      		(GCallback) on_error_video, NULL);

	// Start playing
	ret = gst_element_set_state (RpiData_SendVideo->pipeline, GST_STATE_PLAYING);

	if (ret == GST_STATE_CHANGE_FAILURE)
	{
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		gst_object_unref (RpiData_SendVideo->pipeline);
		return -1;
	}

}

void  _video_send_cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data)
{
	gchar *line = NULL;
	int rval;
	int RetVal = 0;
	gboolean ret = TRUE;
	flag_trans = 0;

	// Candidate gathering is done. Send our local candidates on stdout
	//printf("Copy this line to remote client[Video Send]:\n");
	//printf("\n  ");
	_video_send_print_local_data(agent, stream_id, 1);
	// Connect to server
//	do
//	{
//		sConnect = connect_with_timeout(SERVER, SERVER_PORT, 5, 0); // Timeout = 5s
//
//	}while(sConnect == -1); // 1 = connect without timeout

	// Wait until another client connect to server
	connectThread = g_thread_create(_video_send_ClientThread, NULL, FALSE, NULL);
	while (flag_trans != 1)
	{
		usleep(10000);
	}

	rval = _video_send_parse_remote_data(agent, stream_id, 1, AndroidInfo_SendVideo);
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

	video_send_gathering_done = TRUE;
	printf("[send video] Gathering done!\n");
}

int  _video_send_print_local_data(NiceAgent *agent, guint stream_id, guint component_id)
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

	//printf("%s %s", local_ufrag, local_password);
	sprintf(mInfo_SendVideo, "%s %s", local_ufrag, local_password);
	for (item = cands; item; item = item->next) {
	NiceCandidate *c = (NiceCandidate *)item->data;

		nice_address_to_string(&c->addr, ipaddr);

		// (foundation),(prio),(addr),(port),(type)
		/*printf(" %s,%u,%s,%u,%s",
		c->foundation,
		c->priority,
		ipaddr,
		nice_address_get_port(&c->addr),
		candidate_type_name[c->type]);*/
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

int  _video_send_parse_remote_data (NiceAgent *agent, guint streamID,
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
	NiceCandidate *c = _video_send_parse_candidate(line_argv[i], streamID);

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

static NiceCandidate* _video_send_parse_candidate(char *scand, guint streamID)
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

static void _video_send_cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
	if (len == 1 && buf[0] == '\0')
	g_main_loop_quit (gloop);
	printf("%.*s", len, buf);
	fflush(stdout);
}

static int _video_send_ClientThread()
{
	char *header, *init , *dest, *data;
	char buffer[181] = {0};
	char temp[181]={0};
	char temp1[181]={0};
	char combine[181]={0};

	char receiver[181] = {0};
	char sender[181] = {0};
	int rc = 0;

	// Send ice ifo to android
	memcpy(temp,mInfo_SendVideo,sizeof(temp));
	sprintf(combine,"002$%s$%s$%s",destBuf,originBuf,temp);
	rc = Base64Encode(combine, sender, BUFFFERLEN);
	send(global_socket,sender,181,NULL);

	printf ("=============== Send Video ===============\n");
	printf("[send video] send = %s\n", combine);
	printf("[send video] send[Encode] = %s\n", sender);

	//Receive ice info from android
	if(recv(global_socket, buffer, 181, NULL))
	{
		rc = Base64Decode(buffer, receiver, BUFFFERLEN);

		printf("[send video] receive = %s\n", buffer);
		printf("[send video] receive[Decode] = %s\n", receiver);

		header = strtok (receiver,"$");
		init = strtok (NULL,"$");
		dest = strtok (NULL,"$");
		data = strtok (NULL,"$");
		//printf("%s - %s\n",header ,data);

		int j=0;
		if (data!=NULL)
			while(j<strlen(data))
			{
				temp1[j] = *(data+j);
				j++;
			}

		if(!strcmp(header,"002"))
		{
			AndroidInfo_SendVideo = (gchar*)malloc(sizeof(gchar)*181);
			memcpy(AndroidInfo_SendVideo, temp1, sizeof(temp1));
			flag_trans = 1;
			return 0;
		}
	}
}
