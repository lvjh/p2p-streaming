#include "stream.h"
#include "../Login/login.h"

void*  _video_receive_main(CustomData *data)
{
	// Create the nice agent
	data->agent = libnice_create_NiceAgent_with_gstreamer ( video_receive_gathering_done,
															data->context);
	// Create a new stream with one component
	data->stream_id = libnice_create_stream_id (data->agent);

	// Init Gstreamer
	_video_receive_init_gstreamer (data->agent,
								   data->stream_id,
								   data);

	// Start gathering candidates
	libnice_start_gather_candidate (data->agent,
									data->stream_id,
									data->context);

	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[video] agent = %d", data->agent);
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

	/* Video is ready to play, so display it on surfaceview */
	if (data->pipeline->current_state == GST_STATE_PLAYING)
	{
		__android_log_print (ANDROID_LOG_DEBUG, "tutorial-3", "Video is ready!");
		JNIEnv *env = get_jni_env ();
		jclass cls = (*env)->GetObjectClass(env, data->app);
		jfieldID video_available_field_id = (*env)->GetFieldID (env, cls, "isVideoAvailable", "Z");
		(*env)->SetBooleanField(env, data->app, video_available_field_id, JNI_TRUE);
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
	  __android_log_print (ANDROID_LOG_INFO, "tutorial-3", "=========================================\n");
	  message_string = g_strdup_printf ("Error received from element %s: %s", GST_OBJECT_NAME (message->src), err->message);
	  __android_log_print (ANDROID_LOG_INFO, "tutorial-3", "debug_info = %s \n\n message_string = %s\n", debug_info, message_string);
	  __android_log_print (ANDROID_LOG_INFO, "tutorial-3", "=========================================\n");
	  g_clear_error (&err);
	  g_free (debug_info);
	  g_free (message_string);
}

static void on_pad_added (GstElement* object, GstPad* pad, gpointer data)
{
	gchar *pad_name = gst_pad_get_name(pad);
	__android_log_print (ANDROID_LOG_DEBUG, "tutorial-3", "on_pad_added = %s", pad_name);
	GstPad *sinkpad;
	GstElement *autovideosink = (GstElement *) data;
	sinkpad = gst_element_get_static_pad (autovideosink, "sink");
	gst_pad_link (pad, sinkpad);
	gst_object_unref (sinkpad);
}

void  _video_receive_init_gstreamer (NiceAgent *magent, guint streamID, CustomData *data)
{
	GstElement *pipeline, *nicesrc, *capsfilter, *gstrtpjitterbuffer,
		*rtph264depay, *h264parse, *decodebin2, *video_view;

	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	GSource *bus_source;

	// Initialize GStreamer
  	gst_init (NULL, NULL);

  	//Register gstreamer plugin libnice
	gst_plugin_register_static (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"nice",
		"Interactive UDP connectivity establishment",
		plugin_init, "0.1.4", "LGPL", "libnice",
		"http://telepathy.freedesktop.org/wiki/", "");

	// Create elements
	nicesrc = gst_element_factory_make ("nicesrc", NULL);
	capsfilter = gst_element_factory_make ("capsfilter", NULL);
	gstrtpjitterbuffer = gst_element_factory_make ("gstrtpjitterbuffer", NULL);
	rtph264depay = gst_element_factory_make ("rtph264depay", NULL);
	h264parse = gst_element_factory_make ("h264parse", NULL);
	decodebin2 = gst_element_factory_make ("decodebin2", NULL);
	video_view = gst_element_factory_make ("autovideosink", NULL);

	// Set element's properties
	g_object_set (nicesrc, "agent", magent, NULL);
	g_object_set (nicesrc, "stream", streamID, NULL);
	g_object_set (nicesrc, "component", 1, NULL);
	g_object_set (capsfilter, "caps", gst_caps_from_string("application/x-rtp, payload=(int)96"), NULL);
	g_object_set (video_view, "sync", FALSE, NULL);

	pipeline = gst_pipeline_new("Receive Video Pipeline");

	if (!pipeline || ! nicesrc || !capsfilter || !gstrtpjitterbuffer ||
			!rtph264depay|| !video_view || !decodebin2 || !h264parse)
	{
		g_printerr ("Not all elements could be created.\n");
		return;
	}

	gst_bin_add_many (GST_BIN (pipeline), nicesrc, capsfilter,
			 rtph264depay, h264parse, decodebin2, video_view, NULL);

	if (gst_element_link_many (nicesrc, capsfilter, rtph264depay, h264parse, decodebin2, NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.[01]\n");
		gst_object_unref (pipeline);
		return;
	}

	g_signal_connect (decodebin2, "pad_added", G_CALLBACK (on_pad_added), video_view);

	data->pipeline = pipeline;
	gst_element_set_state(data->pipeline, GST_STATE_READY);
	
	data->video_sink = gst_bin_get_by_interface(GST_BIN(data->pipeline), GST_TYPE_X_OVERLAY);
	if (!data->video_sink)
	{
		GST_ERROR ("Could not retrieve video sink");
		return;
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
	check_initialization_complete (data);
}
