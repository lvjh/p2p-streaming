#include "uart.h"
#include "command.h"
#include "../Rpi_Setup/Check_Hardware.h"
#define  SERVO_01 1
#define  SERVO_02 2

void receive_uart_thread(int *pfd);

int uart_control_servo (int servo_id, char direction, int degree)
{
		if (direction == '+')
			control_servo (rpi_hardware.uart.uart_fd, servo_id, degree);
		else
			control_servo (rpi_hardware.uart.uart_fd, servo_id, degree*(-1));


	printf ("send servo done!\n");
}

//int uart_controller (char controller_id, char direction, char degree)
//{
//	pthread_t rx_thread;
//	int *pfd;
//
//	//----- Uart receive -----
//	if (uart0_filestream != -1)
//	{
//		pfd = (int*)malloc(sizeof(int));
//		*pfd = uart0_filestream;
//		pthread_create( &rx_thread, NULL, &receive_uart_thread, pfd);
//	}
//
//	free(pfd);
//	close(uart0_filestream);
//}
//
//void receive_uart_thread(int *pfd)
//{
//	char *rx_buffer;
//	rx_buffer = (char*)malloc(sizeof(char)*256);
//
//	while (1)
//	{
//		rx_uart (*pfd, rx_buffer, 6);
//		fflush (stdout) ;
//	}
//}
//
