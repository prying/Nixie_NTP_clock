#include "app_rtc.h"
#include <sys/time.h>
#include "time.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/event_groups.h"

#define I2C_FASTMODE_FREQ_HZ 400000 //400 khz
#define I2C_PORT I2C_NUM_0
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_RTC_ADDR 0x68 // 0xD0 // 7 bit address
#define ACK_CHECK_EN 1

#define RTC_REG_CONTROL 0x0E
#define RTC_REG_SEC 0x00
#define RTC_REG_MIN 0x01
#define RTC_REG_HOUR 0x02

#define RTC_DATA_CONTROL 0x1C

#define MODE_24_HR true

#define BIT_0 (1 << 0)
#define BIT_1 (1 << 1)

//eventgroup 
static EventGroupHandle_t eventgroup_time;
static const int RTC_SETUP = BIT_0;
static const int RTC_I2C_READY_USE = BIT_1;

//log tag
static const char *tag = "RTC";

//globle config
static i2c_config_t i2c_cfg;
static int SDA = I2C_SDA_PIN;
static int SCL = I2C_SCL_PIN;
static uint8_t ADDR = I2C_RTC_ADDR;
static bool mode_24_HR = MODE_24_HR;

uint8_t get_from_register(uint8_t reg)
{
    //get the data from the regeister and return it
    uint8_t data;

    xEventGroupWaitBits(eventgroup_time, RTC_I2C_READY_USE, pdTRUE, pdTRUE, portMAX_DELAY);
    i2c_cmd_handle_t cmdq = i2c_cmd_link_create();
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, reg, ACK_CHECK_EN);
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmdq, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmdq);
    i2c_master_cmd_begin(I2C_PORT, cmdq, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmdq);

    xEventGroupSetBits(eventgroup_time, RTC_I2C_READY_USE);

    return data;
}

void app_rtc_start()
{
    eventgroup_time = xEventGroupCreate();
    if (eventgroup_time == NULL)
    {
        //exit and restart esp32
        ESP_LOGE(tag, "Failed to create eventgroup");
        abort();
    }
    
    //configure the i2c driver
    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_io_num = SDA;
    i2c_cfg.scl_io_num = SCL;
    i2c_cfg.sda_pullup_en = GPIO_PULLUP_DISABLE;
    i2c_cfg.scl_pullup_en = GPIO_PULLUP_DISABLE;
    i2c_cfg.master.clk_speed = I2C_FASTMODE_FREQ_HZ;

    //initialize config
    i2c_param_config(I2C_PORT, &i2c_cfg);

    //install driver
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);

    //task protection
    //write to control register
    i2c_cmd_handle_t cmdq = i2c_cmd_link_create();
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (I2C_RTC_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, RTC_REG_CONTROL, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, RTC_DATA_CONTROL, ACK_CHECK_EN);
    i2c_master_stop(cmdq);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmdq, 100 / portTICK_RATE_MS);
    if (err != ESP_OK)
    {
        ESP_LOGE(tag, "%s", esp_err_to_name(err));
    }
    //free up mem
    i2c_cmd_link_delete(cmdq);
    ESP_LOGI(tag, "RTC setup");

    xEventGroupSetBits(eventgroup_time, RTC_I2C_READY_USE | RTC_SETUP);
}

void app_rtc_set_i2c_pins(int da, int cl)
{
    if (!(da == NULL || da > 39 || da < 0 || da == cl))
    {
        //default pins
        SDA = da;
    }

    if (!(cl == NULL || cl > 39 || cl < 0 || da == cl))
    {
        //default pins
        SCL = cl;
    }
}

void app_rtc_set_i2c_addr(uint8_t addr)
{
    ADDR = addr;
}

void app_rtc_set_time(time_t *in_time)
{
    i2c_cmd_handle_t cmdq;
    uint8_t Tsec, sec, Tmin, min, Thour, hour, R00, R01, R02;
    struct tm *ts;
    ts = localtime(in_time);

    //create BCD for time
    sec = ts->tm_sec % 10;
    Tsec = ts->tm_sec / 10;
    min = ts->tm_min % 10;
    Tmin = ts->tm_min / 10;
    hour = ts->tm_hour % 10;
    Thour = ts->tm_hour / 10;

    //package to send to registers
    R00 = (Tsec << 4) | sec;
    R01 = (Tmin << 4) | min;
    R02 = (mode_24_HR << 6) | (Thour << 4) | hour;

    xEventGroupWaitBits(eventgroup_time, RTC_I2C_READY_USE, pdTRUE, pdTRUE, portMAX_DELAY);

    //start i2c communitcation
    cmdq = i2c_cmd_link_create();
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    //send data
    i2c_master_write_byte(cmdq, RTC_REG_SEC, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, R00, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, R01, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, R02, ACK_CHECK_EN);
    i2c_master_stop(cmdq);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmdq, 100 / portTICK_RATE_MS);
    if (err != ESP_OK)
    {
        ESP_LOGE(tag, "%s", esp_err_to_name(err));
    }
    i2c_cmd_link_delete(cmdq);

    xEventGroupSetBits(eventgroup_time, RTC_I2C_READY_USE);
}

void app_rtc_set_12hour(bool t)
{
    mode_24_HR = t;
}

void app_rtc_get_time(struct tm *ts)
{
    //get all the time at once
    //need to impliment day, date ..ect
    i2c_cmd_handle_t cmdq;
    uint8_t R00, R01, R02;
    R01 = 0;
    xEventGroupWaitBits(eventgroup_time, RTC_I2C_READY_USE, pdTRUE, pdTRUE, portMAX_DELAY);
    cmdq = i2c_cmd_link_create();
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmdq, RTC_REG_SEC, ACK_CHECK_EN);
    i2c_master_start(cmdq);
    i2c_master_write_byte(cmdq, (ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmdq, &R00, I2C_MASTER_ACK);
    i2c_master_read_byte(cmdq, &R01, I2C_MASTER_ACK);
    i2c_master_read_byte(cmdq, &R02, I2C_MASTER_NACK);
    i2c_master_stop(cmdq);
    i2c_master_cmd_begin(I2C_PORT, cmdq, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmdq);

    xEventGroupSetBits(eventgroup_time, RTC_I2C_READY_USE);

    ts->tm_sec = (((R00 & 0x70) >> 4) * 10) + (R00 & 0x0F);
    ts->tm_min = ((R01 & 0x70) >> 4) * 10 + (R01 & 0x0F);
    ts->tm_hour = ((R02 & 0x30) >> 4) * 10 + (R02 & 0x0F);
}

int app_rtc_get_sec()
{
    uint8_t secReg;
    int t, u;

    secReg = get_from_register(RTC_REG_MIN);
    //turn BCD into int
    u = (secReg & 0b00001111);
    t = ((secReg & 0b01110000) >> 4);
    return (t * 10 + u);
}
int app_rtc_get_min()
{
    uint8_t minReg;
    int t, u;

    minReg = get_from_register(RTC_REG_MIN);
    //turn BCD into int
    u = (minReg & 0b00001111);
    t = ((minReg & 0b01110000) >> 4);
    return (t * 10 + u);
}
int app_rtc_get_hour()
{
    uint8_t hourReg;
    int t, u;

    hourReg = get_from_register(RTC_REG_MIN);
    //turn BCD into int
    u = (hourReg & 0b00001111);
    t = ((hourReg & 0b00110000) >> 4);
    return (t * 10 + u);
}
