#include "uart.h"
#include "command.h"
#define  SERVO_01 1
#define  SERVO_02 2

static void receive_uart_thread(int *pfd);

int uart_control_servo (int servo_id, char direction, int degree)
{
		if (direction == '+')
			control_servo (rpi_hardware.uart.uart_fd, servo_id, degree);
		else
			control_servo (rpi_hardware.uart.uart_fd, servo_id, degree*(-1));


	printf ("send servo done!\n");
}

void uart_get_temnperature ()
{
	/* Send command */
	get_temperature(rpi_hardware.uart.uart_fd);
	printf ("send to get temp done!\n");
}

