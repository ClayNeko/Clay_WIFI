#pragma once
#include <string.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static const char *Clay_WIFI_TAG = "Clay_WIFI";
static uint32_t rssi;
static int rssi_mutex = 0;

typedef struct {
    wifi_ap_record_t    ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t            ap_count = 0;
} wifi_scan_result;

typedef struct {
    char                ssid[32];
    char                password[64];
    wifi_auth_mode_t    authmode = WIFI_AUTH_WPA_WPA2_PSK;
    char                mac[18] = { 0 };
} clay_wifi_config_sta;

typedef struct {
    char                ssid[32];
    char                password[64];
    wifi_auth_mode_t    authmode = WIFI_AUTH_WPA_WPA2_PSK;
    uint8_t             max_connection = 10;
    uint8_t             channel = 6;
} clay_wifi_config_ap;

class Clay_WIFI
{
public:
    esp_err_t init();
    esp_err_t init(wifi_mode_t wifi_mode, clay_wifi_config_ap clay_cfg_ap);
    wifi_scan_result scan();
    esp_err_t connect(clay_wifi_config_sta clay_cfg_sta);
    esp_netif_ip_info_t get_ip_info();
    uint32_t get_rssi();
private:
    void hex_to_mac(uint8_t* dest, char* src);
    void maccpy(uint8_t* dest, char* mac);
private:
    wifi_mode_t wifi_mode;
};

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    ESP_LOGD(Clay_WIFI_TAG, "wifi_event_handler got event_id: %d", int(event_id));
    /* STA Core Events */
    if (event_id == WIFI_EVENT_STA_START) {
    	ESP_LOGI(Clay_WIFI_TAG, "ESP STA started");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	ESP_LOGI(Clay_WIFI_TAG, "esp_wifi_connect error....");
    }
    /* AP Events */
    else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(Clay_WIFI_TAG, "station ["MACSTR"] JOIN, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(Clay_WIFI_TAG, "station ["MACSTR"] LEAVE, AID=%d", MAC2STR(event->mac), event->aid);
    }
    /* Other Events */
    else if (event_id == IP_EVENT_STA_GOT_IP) {
    	ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    	ESP_LOGI(Clay_WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
    else if (event_id == WIFI_EVENT_STA_BSS_RSSI_LOW) {
    	wifi_event_bss_rssi_low_t* event = (wifi_event_bss_rssi_low_t*) event_data;
        rssi = event->rssi;
        rssi_mutex = 0;
	    //ESP_LOGI(Clay_WIFI_TAG, "bss rssi is=%d", int(event->rssi));
    }
}

esp_err_t
Clay_WIFI::init() {
    wifi_mode = WIFI_MODE_STA;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));

    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

esp_err_t
Clay_WIFI::init(wifi_mode_t clay_wifi_mode, clay_wifi_config_ap clay_cfg_ap) {
    if (wifi_mode == WIFI_MODE_STA) {
        ESP_LOGE(Clay_WIFI_TAG, "clay_cfg_ap provided, can't use WIFI_MODE_STA");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_mode = clay_wifi_mode;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    if (wifi_mode == WIFI_MODE_APSTA) {
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        assert(sta_netif);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));

    wifi_config_t wifi_config_ap = {
        .ap = {
            .channel = clay_cfg_ap.channel,
            .authmode = clay_cfg_ap.authmode,
            .max_connection = clay_cfg_ap.max_connection,
            .pmf_cfg = {
                .required = false,
            },
        }
    };
    strcpy((char *) wifi_config_ap.ap.ssid, clay_cfg_ap.ssid);
    strcpy((char *) wifi_config_ap.ap.password, clay_cfg_ap.password);
    wifi_config_ap.ap.ssid_len = strlen(clay_cfg_ap.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,  &wifi_config_ap));

    ESP_LOGI(Clay_WIFI_TAG, "wifi_config_ap ->");
    ESP_LOGI(Clay_WIFI_TAG, "\t ssid - %s", (char*) &wifi_config_ap.ap.ssid);
    ESP_LOGI(Clay_WIFI_TAG, "\t pass - %s", (char*) &wifi_config_ap.ap.password);

    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

wifi_scan_result
Clay_WIFI::scan() {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    wifi_scan_result scan_result;

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, scan_result.ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&scan_result.ap_count));

    return scan_result;
}

esp_err_t
Clay_WIFI::connect(clay_wifi_config_sta clay_cfg_sta) {
    if (wifi_mode == WIFI_MODE_AP) {
        ESP_LOGE(Clay_WIFI_TAG, "ClayWifi is running in WIFI_MODE_AP(only) mode, STA functions is not support");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config_sta = {
            .sta = {
                .threshold = {
                    .authmode = clay_cfg_sta.authmode,
                }
            }
        };

    strcpy((char *) wifi_config_sta.sta.ssid, clay_cfg_sta.ssid);
    strcpy((char *) wifi_config_sta.sta.password, clay_cfg_sta.password);
    wifi_config_sta.sta.threshold.authmode = clay_cfg_sta.authmode;
    if (strlen(clay_cfg_sta.mac) != 0)
        maccpy(wifi_config_sta.sta.bssid, clay_cfg_sta.mac);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));

    ESP_LOGI(Clay_WIFI_TAG, "wifi_config_sta ->");
    ESP_LOGI(Clay_WIFI_TAG, "\t ssid - %s", (char*) &wifi_config_sta.sta.ssid);
    ESP_LOGI(Clay_WIFI_TAG, "\t pass - %s", (char*) &wifi_config_sta.sta.password);
    if (strlen(clay_cfg_sta.mac) != 0)
        ESP_LOGI(Clay_WIFI_TAG, "\t mac  - %s", (char*) &clay_cfg_sta.mac);

    esp_err_t connect_error = esp_wifi_connect();
    return connect_error;
}

uint32_t
Clay_WIFI::get_rssi() {
    rssi_mutex = 1;
    esp_wifi_set_rssi_threshold(-50);
    while (rssi_mutex == 1) vTaskDelay(500/portTICK_PERIOD_MS);
    return rssi;
}

esp_netif_ip_info_t
Clay_WIFI::get_ip_info() {
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

    return ip_info;
}
void
Clay_WIFI::hex_to_mac(uint8_t* dest, char* src) {
    int values[6]; 
    int i; 

    if(6 == sscanf(src, "%x:%x:%x:%x:%x:%x%*c", 
        &values[0], &values[1], &values[2], 
        &values[3], &values[4], &values[5])) 
    { 
        for(i = 0; i < 6; ++i) 
            dest[i] = (uint8_t) values[i]; 
    }
}
void
Clay_WIFI::maccpy(uint8_t* dest, char* mac) {
    uint8_t MACAddress[6] = { 0 };
    hex_to_mac(MACAddress, mac);

    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, &MACAddress[0]));
    memcpy(dest,MACAddress,sizeof(MACAddress));
}
