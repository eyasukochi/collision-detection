#include <esp_log.h>
#include <string.h>
#include <string>
#include "sdkconfig.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "soc/rtc.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"



static char tag[]="collision-detection";

#define ECHO_TEST_TXD  (GPIO_NUM_4)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE (256) // frankly, just so it's bigger than the hardware buffer size (UART_FIFO_LEN)

extern "C" {
	#include "dac-cosine.h"
	#include "sdmmc_cmd.h"
	void app_main(void);
}


static QueueHandle_t data_logging = NULL;

// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO GPIO_NUM_2
#define PIN_NUM_MOSI GPIO_NUM_15
#define PIN_NUM_CLK  GPIO_NUM_14
#define PIN_NUM_CS   GPIO_NUM_13


/* Declare global sine waveform parameters
 * so they may be then accessed and changed from debugger
 * over an JTAG interface
 */
int clk_8m_div = 0;      // RTC 8M clock divider (division is by clk_8m_div+1, i.e. 0 means 8MHz frequency)
int frequency_step = 8;  // Frequency step for CW generator
int scale = 1;           // 50% of the full scale
int offset;              // leave it default / 0 = no any offset
int invert = 2;          // invert MSB to get sine waveform

uint8_t* data = (uint8_t*) malloc(1);

static uint8_t read_one_byte(){
	uart_read_bytes(UART_NUM_1, data, 1, 100 / portTICK_RATE_MS);
	return data[0];
}

static void detect_task(void *arg)
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
    uint32_t check;
    uint32_t dist;
    uint32_t strength;

    if (data_logging == NULL){
    	ESP_LOGE(tag, "data_logging queue was null");
    } else {
    	ESP_LOGI(tag, "data_logging was instantiated");
//    	data_logging->
    }

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
						xQueueSendToBack(data_logging, &dist, portMAX_DELAY); // Values are copied out of the dist addr into the queue storage area
					}
					else
					{
						ESP_LOGW(tag, "skipping value due to bad hash");
					}
				}
			}
//			ESP_LOGI(tag, "Read %d bytes: '%s'", rxBytes, data);
//			ESP_LOG_BUFFER_HEXDUMP(tag, data, rxBytes, ESP_LOG_INFO);



		vTaskDelay(10/portTICK_RATE_MS);
	}
	free(data);
}

static void dumb_producer(void *arg) {
	uint32_t constval = 100;
	if (data_logging == NULL ){
		ESP_LOGW(tag, "data_logging queue was null!!");
	} else {
		ESP_LOGI(tag, "data_logging queue has been created");
	}
	while(1) {
		ESP_LOGI(tag, "Trying to write onto queue");
		xQueueSendFromISR(data_logging, &constval, NULL);
		vTaskDelay(10/portTICK_RATE_MS);
	}
}

static void dumb_receiver(void *arg) {
	uint32_t value;
	if (data_logging == NULL ){
		ESP_LOGW(tag, "data_logging queue was null!!");
	} else {
		ESP_LOGI(tag, "data_logging queue has been created");
	}
	while(1) {
		xQueueReceive(data_logging, &value, portMAX_DELAY);
		ESP_LOGI(tag, "Got back %d", value);
	}
}

static void sound_task(void *arg) {
		dac_cosine_enable(DAC_CHANNEL_1);
//	    dac_cosine_enable(DAC_CHANNEL_2);

	    dac_output_enable(DAC_CHANNEL_1);
//	    dac_output_enable(DAC_CHANNEL_2);

	    while(1){

			// frequency setting is common to both channels
			dac_frequency_set(clk_8m_div, frequency_step);

			/* Tune parameters of channel 2 only
			 * to see and compare changes against channel 1
			 */
			dac_scale_set(DAC_CHANNEL_1, scale);
			dac_offset_set(DAC_CHANNEL_1, offset);
			dac_invert_set(DAC_CHANNEL_1, invert);

			float frequency = RTC_FAST_CLK_FREQ_APPROX / (1 + clk_8m_div) * (float) frequency_step / 65536;
			printf("clk_8m_div: %d, frequency step: %d, frequency: %.0f Hz\n", clk_8m_div, frequency_step, frequency);
			printf("DAC2 scale: %d, offset %d, invert: %d\n", scale, offset, invert);
			vTaskDelay(2000/portTICK_PERIOD_MS);
		}
}

static void file_writer_task(void *arg) {
	ESP_LOGI(tag, "Using SPI peripheral");

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = PIN_NUM_MISO;
	slot_config.gpio_mosi = PIN_NUM_MOSI;
	slot_config.gpio_sck  = PIN_NUM_CLK;
	slot_config.gpio_cs   = PIN_NUM_CS;
	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.


	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
	// Please check its source code and implement error recovery when developing
	// production applications.
	sdmmc_card_t* card;
	esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(tag, "Failed to mount filesystem. "
				"If you want the card to be formatted, set format_if_mount_failed = true.");
		} else {
			ESP_LOGE(tag, "Failed to initialize the card (%s). "
				"Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
		}
		return;
	}

	// Card has been initialized, print its properties
	sdmmc_card_print_info(stdout, card);
	// TODO at this point, should read from a queue and when a certain buffer size has been reached, write a new file with all of those data points
	// Qs: what is that buffer size? Or can I just leave the file open as I consume N values from the queue.
	// Definitely have to write periodically because there will not be a shutdown op.

	//TODO need to detect existing files and create incremental folder per "on" cycle. basically per time car was turned on.
	//Then use esp_log_timestamp() to create per-cycle-unique file names

	//convert from dir name to int, find the largest int, and then increment by one to create new dir
//	    strtoimax("1", NULL, 10);

	int values_to_write = 60000; // should be roughly once a minute
	while(1) {

		ESP_LOGI(tag, "Opening file");

//	    	uint32_t now = esp_log_timestamp();
		FILE* f = fopen("/sdcard/hello.txt", "w");
		if (f == NULL) {
			ESP_LOGE(tag, "Failed to open file for writing");
			return;
		}

		uint32_t value;
		for (int i = 0; i< values_to_write; i++) {
			xQueueReceive(data_logging, &value, portMAX_DELAY);
			fprintf(f, "%d,", value);
		}
		fclose(f);
	}

	// All done, unmount partition and disable SDMMC or SPI peripheral
	//UNCLEAR HOW THIS COULD BE REACHED?
//	esp_vfs_fat_sdmmc_unmount();
//	ESP_LOGI(tag, "Card unmounted");
}

void app_main(void)
{
	data_logging = xQueueCreate(100, sizeof(uint32_t)); //TODO is this uint32?


//	xTaskCreate(dumb_producer, "dumb_producer", 2048, NULL, 10, NULL);
//	xTaskCreate(dumb_receiver, "dumb_receiver", 2048, NULL, 10, NULL);

	ESP_LOGI(tag, "Using SPI peripheral");

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = PIN_NUM_MISO;
	slot_config.gpio_mosi = PIN_NUM_MOSI;
	slot_config.gpio_sck  = PIN_NUM_CLK;
	slot_config.gpio_cs   = PIN_NUM_CS;
	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.


	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
	// Please check its source code and implement error recovery when developing
	// production applications.
	sdmmc_card_t* card;
	esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(tag, "Failed to mount filesystem. "
				"If you want the card to be formatted, set format_if_mount_failed = true.");
		} else {
			ESP_LOGE(tag, "Failed to initialize the card (%s). "
				"Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
		}
		return;
	}

	// Card has been initialized, print its properties
//	sdmmc_card_print_info(stdout, card);

	// create detector later because of how long sd initalization takes...
	xTaskCreate(detect_task, "detector_task", 2*1024, NULL, 10, NULL);

	int values_to_write = 1000;
	while(1) {

		ESP_LOGW(tag, "Opening file");

		uint32_t now = esp_log_timestamp();
		char fileName[50];
		sprintf(fileName, "/sdcard/%d.txt", now);
		FILE* f = fopen(fileName, "w");
		if (f == NULL) {
			ESP_LOGE(tag, "Failed to open file for writing");
			return;
		}

		int value;
		for (int i = 0; i< values_to_write; i++) {
			xQueueReceive(data_logging, &value, portMAX_DELAY);
			fprintf(f, "%d,", value);
		}
		fclose(f);
	}

//	xTaskCreate(sound_task, "dactask", 1024*3, NULL, 10, NULL);
//	xTaskCreate(file_writer_task, "sd_write_task", 3*1024, NULL, 10, NULL);




}

