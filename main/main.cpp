/*
 * 1. Open up the project properties
 * 2. Visit C/C++ General > Preprocessor Include Paths, Macros, etc
 * 3. Select the Providers tab
 * 4. Check the box for "CDT GCC Built-in Compiler Settings"
 * 5. Set the compiler spec command to "xtensa-esp32-elf-gcc ${FLAGS} -E -P -v -dD "${INPUTS}""
 * 6. Rebuild the index
*/

#include <esp_log.h>
#include <string>
#include "sdkconfig.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"


static char tag[]="collision-detection";

#define ECHO_TEST_TXD  (GPIO_NUM_4)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE (256) // frankly, just so it's bigger than the hardware buffer size (UART_FIFO_LEN)

extern "C" {
	void app_main(void);
}


uint8_t* data = (uint8_t*) malloc(1);

static uint8_t read_one_byte(){
	uart_read_bytes(UART_NUM_1, data, 1, 100 / portTICK_RATE_MS);
	return data[0];
}

static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);

    printf("LET'S GOOOOO ALREADY!\n");

    uint8_t frames [9];
    uint8_t* data = (uint8_t*) malloc(1);
    int check;
    int dist;
    int strength;

	while (1) {

			//JUST DO THE SHIT VERSION FOR NOW
			if (read_one_byte()==0x59) {
				frames[0] = 0x59;
				if (read_one_byte()==0x59) {
					frames[1] = 0x59;
					for(int i=2;i<9;i++)//save data in array
					{
						frames[i]=read_one_byte();
					}
					check=frames[0]+frames[1]+frames[2]+frames[3]+frames[4]+frames[5]+frames[6]+frames[7];
					if(frames[8]==(check&0xff))//verify the received data as per protocol
					{
						dist=frames[2]+frames[3]*256;//calculate distance value
						strength=frames[4]+frames[5]*256;//calculate signal strength value
						ESP_LOGI(tag, "dist = %d    strength = %d", dist, strength);
					}
				}
			}
//			ESP_LOGI(tag, "Read %d bytes: '%s'", rxBytes, data);
//			ESP_LOG_BUFFER_HEXDUMP(tag, data, rxBytes, ESP_LOG_INFO);



		vTaskDelay(10/portTICK_RATE_MS);
	}
	free(data);
}

void app_main(void)
{
	xTaskCreate(echo_task, "uart_echo_task", 1024, NULL, 10, NULL);
}

