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

#define BUF_SIZE (72) // was 1024, 72 is 9 bytes of data which is one frame of information. Maybe I'm supposed to read all of that....

extern "C" {
	void app_main(void);
}

//class Greeting {
//public:
//	void helloEnglish() {
//		ESP_LOGD(tag, "Hello %s", name.c_str());
//	}
//
//	void helloFrench() {
//		ESP_LOGD(tag, "Bonjour %s", name.c_str());
//	}
//
//	void setName(std::string name) {
//		this->name = name;
//	}
//private:
//	std::string name = "";
//
//};

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

//    uint8_t hex_start[] = {0x42, 0x57, 0x02, 0x00, 0x00, 0x00, 0x01, 0x06};
//    uint8_t* hex_start_ptr = hex_start;
//    int response = uart_write_bytes(UART_NUM_1, (const char *) hex_start_ptr, 8);
//    printf("WROTE %d BYTES TO FIFO\n", response);



//    uart_write_bytes(UART_NUM_1, (const char *) 0x42, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x57, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x02, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x00, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x00, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x00, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x01, 1);
//	uart_write_bytes(UART_NUM_1, (const char *) 0x06, 1);
	// Configure a temporary buffer for the incoming data
//    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    printf("LET'S GOOOOO ALREADY!\n");

//    while (1) {
        // Read data from the UART
//        int len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 20 / portTICK_RATE_MS);
//        printf("Got back %d bytes\n",len);
//        printf("%X\n",*data);
//        printf("\n");
        // Write data back to the UART
//        uart_write_bytes(UART_NUM_1, (const char *) data, len);
//    }

    uint8_t frames [9];
    uint8_t* data = (uint8_t*) malloc(100);
    uint8_t frame_idx = 0;
	while (1) {
		// So this is going to populate a data array each time we read, and I have to partially loop over this data array,
		// so start with something that is empty and then start to fill it with what you pull out,
		// continue parsing and handling what you got from the newest read until there is no data left to pull in and handle and then read again,
		// possibly continuing to fill a previous array of frames.

		const int rxBytes = uart_read_bytes(UART_NUM_1, data, 100, 1000 / portTICK_RATE_MS);
		if (rxBytes > 0) {
			data[rxBytes] = 0;
			for (int i = 0; i<rxBytes; i++){
				frames[frame_idx] = data[i];
				frame_idx++;
				if (frame_idx >= 9) {
					if (data[i] == 0x59){
						// we ended one set of frames, and we need to check if we're onto the next ones?
					}
					// Dump frame, restart frame_idx
					frame_idx = 0;
				}
			}
			ESP_LOGI(tag, "Read %d bytes: '%s'", rxBytes, data);
			ESP_LOG_BUFFER_HEXDUMP(tag, data, rxBytes, ESP_LOG_INFO);
		}


		vTaskDelay(100/portTICK_RATE_MS);
	}
	free(data);
}

void app_main(void)
{
	xTaskCreate(echo_task, "uart_echo_task", 1024, NULL, 10, NULL);
//	Greeting myGreeting;
//	myGreeting.setName("Neil");
//	myGreeting.helloEnglish();
//	myGreeting.helloFrench();
}

