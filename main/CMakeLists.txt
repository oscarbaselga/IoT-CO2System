idf_component_register(SRCS "main.c" "sensors/sensor_sgp30.c" 
                    "communications/comm_mqtt.c" "communications/comm_http.c" "communications/comm_sntp.c" "communications/comm_ble.c" 
                    "provisioning/prov.c" "provisioning/prov_handlers.c"
                    INCLUDE_DIRS ".")
