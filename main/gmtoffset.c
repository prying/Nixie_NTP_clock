#include "gmtoffset.h"
#include "esp_log.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "app_wifi.h"

#define SERVER_PORT "80" //needs to be a char

#define BUFF_SIZE 256

#define NODE_NAME "api.timezonedb.com" // web server

#define STATUS_OK "Ok"

static const char *tag = "HTTP request";

int get_GMT_offset(const char *HTTPheader, int *gmtOffset)
{
    //create TCP socket
    int fd, err = 0;
    struct addrinfo hints, *results;
    char recvBuff[BUFF_SIZE];
    struct in_addr *addr;
    char *serPtr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; //connection type socket
    hints.ai_family = AF_UNSPEC;     //IPv4 or IPv6;

    //Get server info
    app_wifi_wait_IPv4();
    err = getaddrinfo(NODE_NAME, SERVER_PORT, &hints, &results);
    if (err != 0)
    {
        //error
        ESP_LOGE(tag, "getaddringo: %d", err);
        freeaddrinfo(results);
        return 1;
    }
    addr = &((struct sockaddr_in *)results->ai_addr)->sin_addr;
    ESP_LOGI(tag, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    //create socket
    fd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    if (fd < 0)
    {
        ESP_LOGE(tag, "Failed to create socket");
        freeaddrinfo(results);
        close(fd);
        return 2; //failed to get new offset condition
    }

    //connect to server
    err = connect(fd, results->ai_addr, results->ai_addrlen);
    if (err == -1)
    {
        //failed to connect to the server
        ESP_LOGE(tag, "Failed to connect to server");
        freeaddrinfo(results);
        close(fd);
        return 3;
    }

    freeaddrinfo(results);
    //send the HTTP header to the target server
    int headerLength = strlen(HTTPheader);
    err = write(fd, HTTPheader, headerLength); //equvilent to send with flags set to 0
    if (err < headerLength)
    {
        //failed to send all data.
        ESP_LOGE(tag, "Failed to send header to server (%i/%i bits sent)\n", err, headerLength);
        close(fd);
        return 4;
    }

    ESP_LOGI(tag, "ready to recive");
    err = recv(fd, recvBuff, sizeof(recvBuff), 0);

    //remove HTTP header
    serPtr = strstr(recvBuff, "\r\n\r\n");
    if (serPtr)
    {
        serPtr += 4;
    }
    char status[10];
    int Offset;
    //check status
    //char *tmp = strstr(serPtr, '{');
    int cont = sscanf(serPtr, "%*[^{] {\"status\":\"%[^\"]\",%*[^,],\"gmtOffset\": %d, %*[^}]", status, &Offset);

    //error check with cont needed

    //preusme it is always correct
    if (status == STATUS_OK)
    {
        *gmtOffset = Offset;
    }
    else
    {
        //add fail condition 
    }
    close(fd);
    return 0;
}