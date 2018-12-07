#ifndef _APP_RTC_H_
#define _APP_RTC_H_

#include "driver/i2c.h"
#include "time.h"
//for use with DS3231M
//start the rtc w/o time, defaults are SDA = 18, SCL = 19
void app_rtc_start();
//must be called befor rtc start!
void app_rtc_set_i2c_pins(int SDA, int SCL);
void app_rtc_set_i2c_addr(uint8_t addr);

//set the rtc time using unix time
void app_rtc_set_time(time_t *in_time);

//set 24 or 12 hour time
//defaults to 24 hour time
void app_rtc_set_12hour(bool t);

//retun time funcs
int app_rtc_get_sec();
int app_rtc_get_min();
int app_rtc_get_hour();
void app_rtc_get_time(struct tm *ts);

#endif