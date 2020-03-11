#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define INPUT_BUFFER_SIZE 100

TaskHandle_t keyboard_input_taskHandle;
QueueHandle_t inputStingQueue;	//������� ��� �������� ������ � ����������
wifi_config_t wifi_config;	//��������� ��� ������������ wi-fi
uint8_t wifi_configReady = 0;	//���������� ���������������� ������

#define EXAMPLE_ESP_MAXIMUM_RETRY 5
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_retry_num = 0;
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                 printf("\nReceived data:\n%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}


void keyboard_input_task(void *pvParameters)//���� ������ � ����������
{
	inputStingQueue = xQueueCreate(1, sizeof(char *));
	char data[INPUT_BUFFER_SIZE] ;
	memset(data, 0, sizeof(data));
	char *pdata = data;
	while(true)
	{
		char c = 0xff;
		//printf("Print something: ");
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
		//memset(data, 0, sizeof(data));
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

void wifi_init_sta(void) {
	static uint8_t connectionState = false;
		s_wifi_event_group = xEventGroupCreate();
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
		ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
		ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	while(connectionState != true)
	{
	char *prxData;	//��������� �� ����� ����� � ����������
	vTaskResume(keyboard_input_taskHandle);
	printf("Enter SSID: ");
	xQueueGenericReceive(inputStingQueue, &prxData, portMAX_DELAY, false);
	memcpy(&wifi_config.sta.ssid, prxData, 32);	//����������� �� ������ � ������������
	memset(prxData, 0, INPUT_BUFFER_SIZE);	//�������� �����

	printf("Enter password: ");
	xQueueGenericReceive(inputStingQueue, &prxData, portMAX_DELAY, false);
	memcpy(&wifi_config.sta.password, prxData, 32);	//����������� �� ������ � ������������
	memset(prxData, 0, INPUT_BUFFER_SIZE);	//�������� �����
	vTaskSuspend(keyboard_input_taskHandle);//���������� ���� ����� � ����������

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
	WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
	pdFALSE,
	pdFALSE,
	portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
		connectionState = true;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
		esp_wifi_stop();
		xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	}
	ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,&wifi_event_handler));
	ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
	vEventGroupDelete(s_wifi_event_group);
}

void app_main(void)
{
	nvs_flash_init();
	tcpip_adapter_init();
	xTaskCreate(keyboard_input_task, "keyboard", 2048, NULL, 1, &keyboard_input_taskHandle);
	vTaskSuspend(keyboard_input_taskHandle);	//����� �� ��������� ���� �����
	wifi_init_sta();

	esp_http_client_config_t config = {
	        .host = "httpbin.org",
	        .path = "/get",
	        .transport_type = HTTP_TRANSPORT_OVER_TCP,
	        .event_handler = _http_event_handler,
	    };
	    esp_http_client_handle_t client = esp_http_client_init(&config);

	    // GET
	    esp_err_t err = esp_http_client_perform(client);
	    if (err == ESP_OK) {
	        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
	                esp_http_client_get_status_code(client),
	                esp_http_client_get_content_length(client));
	    } else {
	        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
	    }
	    esp_http_client_cleanup(client);
	while (true) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

