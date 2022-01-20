# IoT project for air quality monitoring

This project is part of the IoT Master's Degree at the Complutense University of Madrid.

It is divided into 3 parts:
- The first one corresponds to sensors. The ESP32 DevKitC is used to get the quantity of CO2 in the air from the SGP30 sensor. This task is power-optimised by means of the sleep mode provided by ESP-IDF.
- The second focuses on using BLE to estimate the number of people in a room. In addition, it also includes the provisioning process for transmitting WiFi credentials to end-devices.
- The latter is responsible for communication with the outside. It utilizes MQTT to send data messages to the host. The data, which is represented in CBOR, is encrypted along its path.
