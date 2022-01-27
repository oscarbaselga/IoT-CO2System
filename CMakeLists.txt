# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(miot)

target_add_binary_data(miot.elf "main/communications/certs/mqtt_test_broker.pem" TEXT)
target_add_binary_data(miot.elf "main/communications/certs/http_cert.pem" TEXT)
target_add_binary_data(miot.elf "main/communications/certs/http_key.pem" TEXT)
