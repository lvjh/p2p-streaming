Rpi_Setup: Check_Hardware.o utils.o 
	ld -r Check_Hardware.o utils.o -o Rpi_Setup.o
	rm -rf Check_Hardware.o utils.o

Check_Hardware.o:
	gcc -Wall -c Check_Hardware.c

utils.o:
	gcc -Wall -c utils.c

