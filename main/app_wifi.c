#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "app_wifi.h"

//event groups
static EventGroupHandle_t eventGroup_wifi;

//event group bits
//only looking for ip
static const int WIFI_IP_BIT = BIT0;

static const char *TagWifi = "WiFi";

//SSID and password
#define SSID CONFIG_ESP_WIFI_SSID
#define PASS CONFIG_ESP_WIFI_PASSWORD


//event handler
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TagWifi, "Connecting...");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        //print ip once connected and possable start app
        //myIp = &event->event_info.got_ip.ip_info.ip;
        ESP_LOGI(TagWifi, "Connected, ip:%s \n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

        //tell the app of the new ip if a new ip is assigned
        xEventGroupSetBits(eventGroup_wifi, WIFI_IP_BIT);

        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        //tell event group
        xEventGroupClearBits(eventGroup_wifi, WIFI_IP_BIT);
        ESP_LOGI(TagWifi, "Lost connection to %s \n", SSID);
        //need to add a reconnect
        break;

    default:
        break;
    }
    return ESP_OK;
}

void app_wifi_sta_start()
{
    //keep track of device holding an IP
    eventGroup_wifi = xEventGroupCreate();
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TagWifi, "Wifi initialization finished \n");
    ESP_LOGI(TagWifi, "Connecting to :%s \n", SSID);
}

void app_wifi_wait_IPv4()
{
    xEventGroupWaitBits(eventGroup_wifi, WIFI_IP_BIT, false, true, portMAX_DELAY);
}

void app_wifi_wait_connected()
{
    //not implemented yet    
}