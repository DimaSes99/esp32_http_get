#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#define INPUT_BUFFER_SIZE 50
QueueHandle_t inputStingQueue;

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	return ESP_OK;
}


void keyboard_input(void *pvParameters)
{
	inputStingQueue = xQueueCreate(10, sizeof(char *));
	char data[INPUT_BUFFER_SIZE] ;
	memset(data, 0, sizeof(data));
	char *pdata = data;
	while(true)
	{
		char c = 0xff;
		printf("Print something: ");
		while(c != '\n')
		{
			if(c != 0xff)
			{
				data[strlen(data)] = c;
				printf("%c", c);
			}
			c = getchar();
			vTaskDelay(10/portTICK_PERIOD_MS);
		}
		printf("\n");
		xQueueGenericSend(inputStingQueue, &pdata, (TickType_t)0, queueSEND_TO_FRONT);
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

void receive_from_queue(void *pvParameters)
{
	char *prxData;
	while(true)
	{
		if(inputStingQueue != 0)
		{
			if(xQueueGenericReceive(inputStingQueue, &prxData, (TickType_t)0, false))
			{
				printf("Received: %s\n", prxData);
				memset(prxData, 0, 50);
			}
		}
		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}

void app_main(void)
{
	nvs_flash_init();
	//	tcpip_adapter_init();
	//	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	//	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	//	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	//	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	//	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	//	wifi_config_t sta_config = {
	//			.sta = {
	//					.ssid = CONFIG_ESP_WIFI_SSID,
	//					.password = CONFIG_ESP_WIFI_PASSWORD,
	//					.bssid_set = false
	//			}
	//	};
	//	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
	//	ESP_ERROR_CHECK( esp_wifi_start() );
	//	ESP_ERROR_CHECK( esp_wifi_connect() );

	xTaskCreate(keyboard_input, "keyboard", 2048, NULL, 1, NULL);
	xTaskCreate(receive_from_queue, "receive", 2048, NULL, 1, NULL);
	while (true) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

