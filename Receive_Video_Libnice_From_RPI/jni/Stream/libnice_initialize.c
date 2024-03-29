#include "libnice_initialize.h"


NiceAgent *libnice_create_NiceAgent_without_gstreamer ( gboolean *signal_type,
											    		GMainContext *context)
{
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "libnice_create_NiceAgent_without_gstreamer");
	NiceAgent *agent;

	// Create the nice agent
	agent = nice_agent_new( context,
							NICE_COMPATIBILITY_RFC5245);
	if (agent == NULL)
		return NULL;

	// Set the STUN settings and controlling mode
	g_object_set(G_OBJECT(agent),
				 "stun-server",
				 STUNSR_ADDR,
				 NULL);

	g_object_set(G_OBJECT(agent),
				 "stun-server-port",
				 STUNSR_PORT, NULL);

	g_object_set(G_OBJECT(agent),
				 "controlling-mode",
				 0,
				 NULL);

	// Connect to the signals
	g_signal_connect(G_OBJECT(agent),
					 "candidate-gathering-done",
					 G_CALLBACK(libnice_candidate_gathering_done),
					 signal_type);

	g_signal_connect(G_OBJECT(agent),
					 "new-selected-pair",
					 G_CALLBACK(libnice_new_selected_pair),
					 signal_type);

	g_signal_connect(G_OBJECT(agent),
					 "component-state-changed",
					 G_CALLBACK(libnice_component_state_changed),
					 signal_type);

	return agent;
}

NiceAgent *libnice_create_NiceAgent_with_gstreamer ( gboolean *signal_type,
											    	 GMainContext *context)
{
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "libnice_create_NiceAgent_with_gstreamer");
	NiceAgent *agent;

	// Create the nice agent
	agent = nice_agent_new( context,
							NICE_COMPATIBILITY_RFC5245);
	if (agent == NULL)
		return NULL;

	// Set the STUN settings and controlling mode
	g_object_set(G_OBJECT(agent),
				 "stun-server",
				 STUNSR_ADDR,
				 NULL);

	g_object_set(G_OBJECT(agent),
				 "stun-server-port",
				 STUNSR_PORT, NULL);

	g_object_set(G_OBJECT(agent),
				 "controlling-mode",
				 0,
				 NULL);

	// Connect to the signals
	g_signal_connect(G_OBJECT(agent),
					 "candidate-gathering-done",
					 G_CALLBACK(libnice_candidate_gathering_done),
					 signal_type);

	return agent;
}

guint libnice_create_stream_id (NiceAgent *agent)
{
	return nice_agent_add_stream(agent, 1);
}

int libnice_start_gather_candidate (NiceAgent *agent, guint stream_id, GMainContext *context)
{
	if (!nice_agent_gather_candidates(agent, stream_id))
		return 1;

	return 0;
}

int connect_to_rpi()
{
	char sender[181] = {0};
	char tmp[181] = {0};
	int rc;

	sprintf(tmp, "001$%s$%s", client_name, rpi_name);
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "tmp = %s", tmp);
	rc = Base64Encode(tmp, sender, BUFFFERLEN);

	int ret = send(global_socket, sender, 181, NULL);
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "ret = %d", ret);

	is_connect_to_rpi = TRUE;
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "connect to rpi done");
	return 0;
}

void  libnice_candidate_gathering_done (NiceAgent *agent, guint stream_id, gboolean *signal_type)
{
	gchar *local_info = (gchar*)malloc(181);
	gchar *remote_info = (gchar*)malloc(181);

	memset(local_info, '\0', 181);
	memset(remote_info, '\0', 181);

	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "is_connect_to_rpi = %d", is_connect_to_rpi);

	if (!is_connect_to_rpi)
		connect_to_rpi();

	libnice_print_local_data (agent, stream_id, 1, local_info);
	libnice_receive_information_from_client_remote (local_info,
												    remote_info);
	libnice_parse_remote_data (agent,
							   stream_id,
							   1,
							   remote_info);

	(*signal_type) = TRUE;
	__android_log_print (ANDROID_LOG_INFO, "CONTROLLER", "parse remote done");
}

void libnice_print_local_data (NiceAgent *agent, guint stream_id, guint component_id, gchar* local_info)
{
	gchar *local_ufrag = NULL;
	gchar *local_password = NULL;
	gchar ipaddr[INET6_ADDRSTRLEN];
	GSList *cands = NULL, *item;

	if (!nice_agent_get_local_credentials(	agent,
											stream_id,
											&local_ufrag,
											&local_password ))
	{
		goto end;
	}

	cands = nice_agent_get_local_candidates(agent,
											stream_id,
											component_id);
	if (cands == NULL)
		goto end;

	printf("%s %s", local_ufrag, local_password);
	sprintf(local_info, "%s %s", local_ufrag, local_password);

	for (item = cands; item; item = item->next)
	{
		NiceCandidate *c = (NiceCandidate *)item->data;

		nice_address_to_string(&c->addr, ipaddr);

		printf(" %s,%u,%s,%u,%s",
		c->foundation,
		c->priority,
		ipaddr,
		nice_address_get_port(&c->addr),
		candidate_type_name[c->type]);

		sprintf(local_info + strlen(local_info), " %s,%u,%s,%u,%s",
		c->foundation,
		c->priority,
		ipaddr,
		nice_address_get_port(&c->addr),
		candidate_type_name[c->type]);
	}

	printf("\n");

	end:
		if (local_ufrag)
			g_free(local_ufrag);
		if (local_password)
			g_free(local_password);
		if (cands)
			g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);
}

int libnice_receive_information_from_client_remote (gchar *local_info,
													gchar *remote_info)
{
	char *header, *init, *dest, *data;
	char buffer[181] = {0};
	char combine[181] = {0};
	char receiver[181] = {0};
	char sender[181] = {0};

	while (1)
	{
		if (recv(global_socket, buffer, 181, NULL))
		{
			/* Decode received data */
			Base64Decode(buffer, receiver, BUFFFERLEN);

			header = strtok(receiver, "$");
			init   = strtok(NULL, "$");
			dest   = strtok(NULL, "$");
			data   = strtok(NULL, "$");

			__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "data = %s", data);
			/* Infomation packet */
			if (!strcmp(header, "002"))
			{
				/* Get remote information */
				memcpy(remote_info, data, strlen(data));

				/* Send local infor to remote */
				sprintf(combine, "002$%s$%s$%s", dest, init, local_info);
				Base64Encode(combine, sender, BUFFFERLEN);
				send(global_socket, sender, 181, NULL);
				break;
			}
		}

		return 0;
	}
}

void libnice_parse_remote_data (NiceAgent *agent,
								guint stream_id,
								guint component_id,
								gchar* remote_info)
{
	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "remote info = %s", remote_info);
	GSList *remote_candidates = NULL;
	gchar **line_argv = NULL;
	const gchar *ufrag = NULL;
	const gchar *passwd = NULL;
	int result = EXIT_FAILURE;
	int i;

	line_argv = g_strsplit_set (remote_info, " \t\n", 0);
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
	NiceCandidate *c = libnice_parse_candidate(line_argv[i], stream_id);

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

	if (!nice_agent_set_remote_credentials(agent, stream_id, ufrag, passwd)) {
	g_message("failed to set remote credentials");
	goto end;
	}

	// Note: this will trigger the start of negotiation.
	if (nice_agent_set_remote_candidates(agent, stream_id, component_id,
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
}

NiceCandidate* libnice_parse_candidate (char *scand, guint stream_id)
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
	cand->stream_id = stream_id;
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

void libnice_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state, gboolean signal_type)
{
	if (state == NICE_COMPONENT_STATE_READY)
	{
		NiceCandidate *local, *remote;

		// Get current selected candidate pair and print IP address used
		if (nice_agent_get_selected_pair (	agent,
											stream_id,
											component_id,
											&local, &remote))
		{
			gchar ipaddr[INET6_ADDRSTRLEN];

			nice_address_to_string(&local->addr, ipaddr);
			__android_log_print (ANDROID_LOG_ERROR, "tutorial-3","\nNegotiation complete: ([%s]:%d,",
			ipaddr, nice_address_get_port(&local->addr));
			nice_address_to_string(&remote->addr, ipaddr);
			__android_log_print (ANDROID_LOG_ERROR, "tutorial-3"," [%s]:%d)\n", ipaddr, nice_address_get_port(&remote->addr));
		}
	}
	else if (state == NICE_COMPONENT_STATE_FAILED)
	{
		g_main_loop_quit (gloop);
	}
}

void libnice_new_selected_pair(	NiceAgent *agent, guint stream_id,
								guint component_id,
								gchar *lfoundation,
								gchar *rfoundation,
								gboolean signal_type)
{
	return;
}
