DEPENDENCES=gstreamer_log.o  send_audio.o  stream.o  text.o receive_audio.o  send_video.o  stun.o
SOURCES=gstreamer_log.c receive_audio.c send_audio.c send_video.c stun.c text.c stream.c

P2P_Stream: 
	gcc -Wall -c $(SOURCES) `pkg-config --cflags --libs nice gstreamer-1.0`
	ld -r  $(DEPENDENCES) -o P2P_Stream.o
	rm -rf $(DEPENDENCES)
