#include "comm_ble.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

#include "globals.h"


#define MAX_NUM_BLE_DEVICES 40 // menuconfig
#define BLE_RSSI_THRESOLD	-80

MAC_BLE_ELEM_t zero_addr = {{0,0,0,0,0,0}, NULL};

// Array for the found devices
MAC_BLE_ELEM_t* ble_dev_found;

static uint8_t last_total_dev = 0;

static int cmp_addr(MAC_BLE_ELEM_t* op1, MAC_BLE_ELEM_t* op2) {
	return op1->addr[0]==op2->addr[0] 
        && op1->addr[1]==op2->addr[1] 
        && op1->addr[2]==op2->addr[2] 
        && op1->addr[3]==op2->addr[3] 
        && op1->addr[4]==op2->addr[4] 
        && op1->addr[5]==op2->addr[5];
}

static int repeted_addr(MAC_BLE_ELEM_t* actual_addr) {
	if(ble_dev_found->prev == NULL) return 0;
	
	MAC_BLE_ELEM_t* ble_dev_aux = ble_dev_found;
	int encontrado = 0;

	while(ble_dev_aux->prev != NULL){
		encontrado |= cmp_addr(actual_addr, ble_dev_aux);
		ble_dev_aux = ble_dev_aux->prev;
	}

	return encontrado;
}

static void update_list(esp_ble_gap_cb_param_t *param, int finished) {
	if (!finished){

		MAC_BLE_ELEM_t* actual_addr;
		actual_addr = (MAC_BLE_ELEM_t*)malloc(sizeof(MAC_BLE_ELEM_t));
		
		actual_addr->addr[0] = param->scan_rst.bda[0];
		actual_addr->addr[1] = param->scan_rst.bda[1];
		actual_addr->addr[2] = param->scan_rst.bda[2];
		actual_addr->addr[3] = param->scan_rst.bda[3];
		actual_addr->addr[4] = param->scan_rst.bda[4];
		actual_addr->addr[5] = param->scan_rst.bda[5];
		
		if (!repeted_addr(actual_addr)) {
			actual_addr->prev = ble_dev_found;
			ble_dev_found = actual_addr;
			/*ESP_LOGI(TAG_BLE, "Contenido ble_dev_found %d %d %d %d %d %d %p %p", 
				actual_addr->addr[0], 
				actual_addr->addr[1], 
				actual_addr->addr[2], 
				actual_addr->addr[3], 
				actual_addr->addr[4], 
				actual_addr->addr[5], 
				actual_addr, actual_addr->prev);*/
		}

	} else if (finished) {

		uint8_t total_dev = 0;
		MAC_BLE_ELEM_t* ble_dev_aux = ble_dev_found;

		while(ble_dev_found->prev != NULL){
			total_dev++;
			ble_dev_aux = ble_dev_found;
			ble_dev_found = ble_dev_found->prev;
			free(ble_dev_aux);
		}

		last_total_dev = total_dev;
		ESP_LOGI(TAG_BLE,"Number of devices in the room -> %d", total_dev);

	}
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {

    switch (event) {
		//Results of the scanning (important --> int rssi)
		case ESP_GAP_BLE_SCAN_RESULT_EVT: {

			int rssiValue = 0;
		    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;

		    switch (scan_result->scan_rst.search_evt) {

                //New device found
                case ESP_GAP_SEARCH_INQ_RES_EVT:

                    //Get rssi value
                    rssiValue = scan_result->scan_rst.rssi;
                    if(rssiValue >= BLE_RSSI_THRESOLD){
                        update_list(param, 0);
                    }
                    break;
					
                //Scanning duration completed
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:

                    update_list(param, 1);
                    break;

                default:
                    break;

		    }
		    break;
		}
		// in case esp_ble_stop_scanning is called and this event is triggered
		case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:

		    if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
		        ESP_LOGE(TAG_BLE, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
		        break;
		    }
		    ESP_LOGI(TAG_BLE, "stop scan successfully");
		    break;
		    
		default:
		    break;
	}
}

void esp_ble_init(void) {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG_BLE, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG_BLE, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG_BLE, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG_BLE, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    
    //Set scan parameters
	static esp_ble_scan_params_t ble_scan_params = {
		.scan_type              = BLE_SCAN_TYPE_ACTIVE,
		.own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
		.scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
		.scan_interval          = 0x0050, //100 msec 0x00A0
		.scan_window            = 0x0030,
		.scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
	};
	esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret){
        ESP_LOGE(TAG_BLE, "set scan params error, error code = %x", scan_ret);
    }
    
   	//register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(TAG_BLE, "%s gap register failed, error code = %x\n", __func__, ret);
        return;
    }
    ESP_LOGI(TAG_BLE, "Bluetooth initialized.");

}

void scan_BLE_devices(int duration) {   
	//allocate memory to store the devices
	ble_dev_found = (MAC_BLE_ELEM_t*)malloc(sizeof(MAC_BLE_ELEM_t) * MAX_NUM_BLE_DEVICES);
	ble_dev_found = &zero_addr;

	//start scanning 
	ESP_LOGI(TAG_BLE, "Device scanning started (duration %ds)", duration);
	esp_ble_gap_start_scanning(duration);  
}

uint8_t get_people_estimation (void) {
	return last_total_dev;
}
