#pragma once
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static const char *Clay_WIFI_TAG = "Clay_WIFI";

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
    void init(wifi_mode_t wifi_mode, clay_wifi_config_ap clay_cfg_ap);
    wifi_scan_result scan();
    esp_err_t connect(clay_wifi_config_sta clay_cfg_sta);
    esp_netif_ip_info_t get_ip_info();
private:
    void hex_to_mac(uint8_t* dest, char* src);
    void maccpy(uint8_t* dest, char* mac);
private:
    wifi_mode_t wifi_mode;
};

void
Clay_WIFI::init(wifi_mode_t clay_wifi_mode, clay_wifi_config_ap clay_cfg_ap) {
    wifi_mode = clay_wifi_mode;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (wifi_mode == WIFI_MODE_STA || wifi_mode == WIFI_MODE_APSTA) {
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        assert(sta_netif);
    }
    if (wifi_mode == WIFI_MODE_AP || wifi_mode == WIFI_MODE_APSTA) {
        esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
        assert(ap_netif);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));

    if (wifi_mode == WIFI_MODE_AP || wifi_mode == WIFI_MODE_APSTA) {
        ESP_LOGI(Clay_WIFI_TAG, "wifi_mode includes WIFI_MODE_AP");
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
    }

    ESP_ERROR_CHECK(esp_wifi_start());
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
