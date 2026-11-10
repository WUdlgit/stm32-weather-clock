#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "rtc.h"

bool rtc_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(RCC_LSE_ON);

    uint32_t timeout = 0x0FFFFFFF;

    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET && timeout > 0)
    {
        timeout--;
    }

    if (timeout == 0)
        return false;
    
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    RCC_RTCCLKCmd(ENABLE);

    RTC_InitTypeDef RTC_InitStruct;
    RTC_StructInit(&RTC_InitStruct);
    if (RTC_Init(&RTC_InitStruct) == ERROR)
        return false;

    if (RTC_WaitForSynchro() == ERROR)
        return false;

    return true;
}

static void _rtc_set_time_once(const rtc_date_time_t *date_time)
{
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    
    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);
    
    date.RTC_Year = date_time->year - 2000;
    date.RTC_Month = date_time->month;
    date.RTC_Date = date_time->day;
    date.RTC_WeekDay = date_time->weekday;
    time.RTC_Hours = date_time->hour;
    time.RTC_Minutes = date_time->minute;
    time.RTC_Seconds = date_time->second;
    
    RTC_SetDate(RTC_Format_BIN, &date);
    RTC_SetTime(RTC_Format_BIN, &time);
}

static void _rtc_get_time_once(rtc_date_time_t *date_time)
{
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    
    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);
    
    RTC_GetDate(RTC_Format_BIN, &date);
    RTC_GetTime(RTC_Format_BIN, &time);
    
    date_time->year = 2000 + date.RTC_Year;
    date_time->month = date.RTC_Month;
    date_time->day = date.RTC_Date;
    date_time->weekday = date.RTC_WeekDay;
    date_time->hour = time.RTC_Hours;
    date_time->minute = time.RTC_Minutes;
    date_time->second = time.RTC_Seconds;
}

void rtc_set_time(const rtc_date_time_t *date_time)
{
    rtc_date_time_t rtime;
    do {
        _rtc_set_time_once(date_time);
        _rtc_get_time_once(&rtime);
    } while (date_time->second != rtime.second);
}

void rtc_get_time(rtc_date_time_t *date_time)
{
    rtc_date_time_t time1, time2;
    do {
        _rtc_get_time_once(&time1);
        _rtc_get_time_once(&time2);
    } while (memcmp(&time1, &time2, sizeof(rtc_date_time_t)) != 0);
    
    memcpy(date_time, &time1, sizeof(rtc_date_time_t));
}
