
#include <sys/param.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "config.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <tcpip_adapter.h>
#include <lwip/udp.h>

#include "esp_http_client.h"

#include "app_wifi.h"
#include "app_rtc.h"
#include "gmtoffset.h"


#define NTP_PACKET_SIZE 48
#define NTP_CALL_RATE_MS 30000

#define UDP_PORT 123
#define UNIX_OFFSET 2208988800UL

#define HTTP_REQUEST_URI "/v2/get-time-zone?key=2ZVJKMES2VF7&format=json&fields=gmtOffset,dst&by=zone&zone=Australia/Melbourne"
#define HTTP_REQUEST_DOMAIN_NAME "api.timezonedb.com"
// 
static const char *ntpServerAddress = "pool.ntp.org";
static const char* HTTPheader = 
"GET " HTTP_REQUEST_URI " HTTP/1.1\r\n"
"Host: "HTTP_REQUEST_DOMAIN_NAME"\r\n"
"Accept: */*\r\n"
"\r\n";
//"User-Agent: Mozilla/5.0 (compatible; Rigor/1.0.0; http://rigor.com)"

//log tags
static const char *TagUDP = "UDP client";

//event groups



static void vGMT_get_offset(void *pvParameters)
{
    
} 

//returns -1 if failed 
int UDP_client(char *data_buffer)
{

    int fd, recvLen;
    struct sockaddr_in servAddr;
    struct hostent *hp;
    size_t buffSize = NTP_PACKET_SIZE;

    ESP_LOGI(TagUDP, "data_buff size: %u", buffSize);   
    //creat socket 
    //AF_INET -> IPv4, AF_INET6 -> IPv6
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        ESP_LOGE(TagUDP, "Failed to create socket");
        close(fd);
        return -1;
    }

    //get ip of host 
    hp = gethostbyname(ntpServerAddress);
    if (!hp)
    {
        ESP_LOGE(TagUDP, "Failed to get target IP");
        close(fd);
        return -1;
    }

    //server socket info 
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(UDP_PORT);
    memcpy(&servAddr.sin_addr, hp->h_addr_list[0], (size_t)hp->h_length);

    //send packet
    if (sendto(fd, data_buffer, buffSize, 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0 )
    {
        ESP_LOGE(TagUDP, "Failed to send packet");
        close(fd);
        return -1;
    }

    //recive packet
    //IDK why this works as it should be recvfrom as this is not bind() and/or connect()
    //this could cause issues later ???
    recvLen = recv(fd, data_buffer, buffSize, 0); //, (struct sockaddr*)&servAddr, sizeof(servAddr)
    if (recvLen >= buffSize)
    {
        //ESP_LOGI(TagUDP, "Recived packet");
        close(fd);
        return 1; 
    }
    else
    {
        ESP_LOGE(TagUDP, "Failed to recive packet");
        close(fd);
        return -1;
    }
}

static void vNTP_Time(void *pvParameters)
{
    //creat buffer
    char NTP_buffer[NTP_PACKET_SIZE];
    while (true)
    {
        //check for an ip
        app_wifi_wait_IPv4();
        //clear the buffer
        memset(NTP_buffer, 0, NTP_PACKET_SIZE);
        //set NTP bits for more info https://tools.ietf.org/html/rfc5905 at page 19

                                    //Variable Value Bits
        NTP_buffer[0] = 0b11100011; //Leap Indicator 3 2|Version Number 4 3|Mode 3 3
        NTP_buffer[1] = 0;          //Stratum 0 8
        NTP_buffer[2] = 6;          //Poll 6 8
        NTP_buffer[3] = 0xEC;       //Precision 236 8
                                    //
        NTP_buffer[12] = 't';       //Reference ID TIME 32
        NTP_buffer[13] = 'i';       //does not realy matter
        NTP_buffer[14] = 'm';       //what goes her, just not anything
        NTP_buffer[15] = 'e';       //that starts with x

        if (UDP_client(NTP_buffer) != -1)
        {
            //extract time from buffer
            //time in sec form byte 40 to 43 and fractions of seconds are the next 4
            uint32_t NTP_time = (NTP_buffer[40] << (3 * 8) | NTP_buffer[41] << (2 * 8) | NTP_buffer[42] << 8 | NTP_buffer[43]);

            //turn it into unix time 1970
            NTP_time = NTP_time - UNIX_OFFSET;

            printf("ntp time is now :%ds\n", NTP_time);

            time_t *now;
            now = &NTP_time; 

            app_rtc_set_time(now);
        }
        vTaskDelay(NTP_CALL_RATE_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void vDisplay_time()
{
    struct tm ts;
    vTaskDelay(5000/portTICK_RATE_MS);
    while(true)
    {
        //every 10 seconds display current time
        app_rtc_get_time(&ts);

        printf("Time: %d:%d:%d\n", ts.tm_hour, ts.tm_min, ts.tm_sec);
        vTaskDelay(10000/portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}


void app_main(void)
{
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    app_wifi_sta_start();
    int tempint = 3;
    get_GMT_offset(HTTPheader, &tempint);
    //app_rtc_start();
    //xTaskCreate(&vNTP_Time, "vNTP_time", 4048, NULL, 1, NULL);
    //xTaskCreate(&vDisplay_time, "vDisplay_time", 2048, NULL, 1, NULL);

}
