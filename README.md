# IoT project for air quality monitoring
### Context
This project is part of the IoT Master's Degree at the Complutense University of Madrid. It covers three subjects, from the architecture and firmware of the nodes to their networking. The final system must be able to:
- Measure the air quality.
- Estimate the number of people in a room using BLE technology.
- Transmit all these information via MQTT.
- Interact with external users through an API REST.

### Environment
During the development of this project, the ESP-IDF v4.1.2 has been used on an ESP32-DevKitC v4. Most of the tools and components offered by ESP-IDF have been integrated into the project, so this must be taken into account when exporting the project.

The gas sensor for measuring the air quality is the SGP30 developed by Sensirion.

### Technical requirements
This section describes all the technical requirements implemented in the project.
- An I2C interface has been developed to capture data from the SGP30 sensor. This sensor is able to measure the quantity of CO2 (ppm) and humidity (%) in the air. In this project, only the CO2 has been taken.
- The power consumption of the node is an important issue, so two solutions has been included. The first one allows the ESP to enter Light Sleep whenever it deems appropiate. The second is to enter Deep Sleep in a pre-defined time interval, e.g. from 10 PM to 8 AM. These two methods help the node to optimise energy consumption.
- To find out the current time in the zone where the node is located, it connects to an SNTP server. Once it obtains the time, synchronises the system time.
- Internet access is required to establish a connection to any server. Therefore, the provisioning process facilitates the exchange of WiFi credentials with the nodes from an external host. This exchange is done via WiFi.
- Periodically, the node detects nearby BLE devices. It starts the scanning on GAP for a while and interprets the distance of the found devices by checking their RSSI value. With this approximation, it can estimate how many people are in the room.
- In terms of communications, the node transmits the data via MQTT. To simplify this part, an open broker for testing (```test.mosquitto.org```) has been used. In addition, although it is not recommended in IoT, an HTTP server has been deployed in the node to interact through an API REST. *HTTP was required in the project just to train with this technology.*
- As these protocols involve traffic overhead, the data has been sent in CBOR representation.
- These protocols should not be used without security mechanisms, so both MQTT and HTTP have been developed on top of SSL/TLS. For MQTT SSL, the broker's certificate was uploaded to the node, and for HTTPS, the node generated its own private key and certificate, which is passed during requests from the host.
- Lastly, a dashboard has been designed in Node-RED to visualise data traffic and manage a global view of the system.

### Authors
Oscar Baselga Lahoz (Computer Engineer)
Jon Ayuso Hernández (Computer Engineer)
