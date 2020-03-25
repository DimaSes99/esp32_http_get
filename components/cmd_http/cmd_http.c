/*
 * cmd_http.c
 *
 *  Created on: Mar 24, 2020
 *      Author: dima
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
//#include "esp_system.h"
//#include "nvs_flash.h"
#include "esp_event.h"
//#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_http_client.h"

extern volatile bool wifiConnectFlag;

static const char *TAG = "HTTP_CLIENT";
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
                 printf("%.*s", evt->data_len, (char*)evt->data);
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

static struct {
    struct arg_str *method;
    struct arg_str *url;
    struct arg_str *body;
    struct arg_end *end;
} http_args;

static int http_perform(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &http_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, http_args.end, argv[0]);
        return 1;
    }

    /*check wifi connection*/
    if(wifiConnectFlag==false){
    	ESP_LOGW(TAG, "Wi-Fi is not connected! Connect to AP and try again. ");
    	return 1;
    }

    esp_http_client_config_t config = {
    	.url = "http://httpbin.org",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if( strcmp(http_args.method->sval[0], "GET") == 0 ){
    	esp_http_client_set_url(client, http_args.url->sval[0]);
    	esp_http_client_set_method(client, HTTP_METHOD_GET);
    	esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }
    }else if(strcmp(http_args.method->sval[0], "POST") == 0){
    	esp_http_client_set_url(client, http_args.url->sval[0]);
    	esp_http_client_set_method(client, HTTP_METHOD_POST);
    	esp_http_client_set_post_field(client, http_args.body->sval[0], sizeof(http_args.body->sval[0]) );
    	esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }else{
    	ESP_LOGW(TAG, "Invalid method!");
//    	printf("Invalid method!\n");
    	esp_http_client_cleanup(client);
    	return 1;
    }
    esp_http_client_cleanup(client);
    return 0;
}

void register_http(void)
{
	http_args.method = arg_str1(NULL, NULL, "<method>", "GET or POST method");
	http_args.url = arg_str1(NULL, NULL, "<URL>","remote server URL");
	http_args.body = arg_str0(NULL, NULL, "<BODY>", "request body in case of POST method");
	http_args.end = arg_end(2);

    const esp_console_cmd_t http_cmd = {
        .command = "http",
        .help = "Perform http GET or POST request",
        .hint = NULL,
        .func = &http_perform,
        .argtable = &http_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&http_cmd) );
}
