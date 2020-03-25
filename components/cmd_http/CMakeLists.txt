idf_component_register(SRCS "cmd_http.c"
                    INCLUDE_DIRS .
                    REQUIRES esp-tls console esp_http_client)