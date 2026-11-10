#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aht20.h"
#include "app.h"
#include "esp_at.h"
#include "FreeRTOS.h"
#include "page.h"
#include "portmacro.h"
#include "projdefs.h"
#include "rtc.h"
#include "st7789.h"
#include "ui.h"
#include "stm32f4xx.h"
#include "task.h"
#include "timers.h"
#include "weather.h"


#define MLOOP_EVT_TIME_SYNC      (1 << 0)    //0x01
#define MLOOP_EVT_WIFI_UPDATE    (1 << 1)    //0x02
#define MLOOP_EVT_TIME_UPDATE    (1 << 2)    //0x04
#define MLOOP_EVT_INNER_UPDATE   (1 << 3)    //0x08
#define MLOOP_EVT_OUTDOOR_UPDATE (1 << 4)    //0x10

#define MLOOP_EVENT_ALL \
    (MLOOP_EVT_TIME_SYNC | MLOOP_EVT_WIFI_UPDATE | MLOOP_EVT_TIME_UPDATE | \
     MLOOP_EVT_INNER_UPDATE | MLOOP_EVT_OUTDOOR_UPDATE)

#define MILLISECONDS(x) x
#define SECONDS(x)      MILLISECONDS(x * 1000)
#define MINUTES(x)      SECONDS(x * 60)
#define HOURS(x)        MINUTES(x * 60)
#define DAYS(x)         HOURS(x * 24)
#define MONTHS(x)       DAYS(x * 30)
#define YEARS(x)        MONTHS(x * 12)

#define TIME_SYNC_INTERVAL_MS      HOURS(1)
#define WIFI_UPDATE_INTERVAL_MS    SECONDS(3)
#define TIME_UPDATE_INTERVAL_MS    SECONDS(1)
#define INNER_UPDATE_INTERVAL_MS   SECONDS(5)
#define OUTDOOR_UPDATE_INTERVAL_MS MINUTES(1)

static TaskHandle_t mloop_task;
static TimerHandle_t time_sync_timer;
static TimerHandle_t wifi_update_timer;
static TimerHandle_t time_update_timer;
static TimerHandle_t inner_update_timer;
static TimerHandle_t outdoor_update_timer;

static rtc_date_time_t time_new, time_old;
static esp_wifi_info_t wifi_info_new, wifi_info_old;
static esp_date_time_t date_time_new;
static weather_info_t weather_info_new, weather_info_old;
static float temperature_old, humidity_old;

static void time_update(void)
{
    rtc_get_time(&time_new);
    if (time_new.year < 2020)
        return;

    if (memcmp(&time_new, &time_old, sizeof(rtc_date_time_t)) != 0)
    {
        main_page_redraw_date(&time_new);
        main_page_redraw_time(&time_new);
        time_old = time_new;
    }
}

static void wifi_update(void)
{
    if (esp_at_get_wifi_info(&wifi_info_new))
    {
        if ((wifi_info_new.connected != wifi_info_old.connected) ||
            strcmp(wifi_info_new.ssid, wifi_info_old.ssid) != 0)
        {
            if (!wifi_info_new.connected)
                main_page_redraw_wifi_ssid("wifi lost");
            else
                main_page_redraw_wifi_ssid(wifi_info_new.ssid);
        }

        wifi_info_old = wifi_info_new;
    }
}

static void inner_update(void)
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    if (aht20_start_measurement())
    {
        if (aht20_wait_for_measurement())
        {
            if (!aht20_read_measurement(&temperature, &humidity))
                return;

            if (temperature != temperature_old || humidity != humidity_old)
            {
                main_page_redraw_inner_temperature(temperature);
                main_page_redraw_inner_humidity(humidity);
                temperature_old = temperature;
                humidity_old = humidity;
            }
        }
    }
    else
    {
        printf("aht20 start false\r\n");
    }
}

static void outdoor_update(void)
{
    const char *response = esp_at_http_get(
        "https://api.seniverse.com/v3/weather/now.json?key=" SENIVERSE_API_KEY
        "&location=baoji&language=en&unit=c");

    if (response == NULL)
    {
        printf("[outdoor update] http get error\r\n");
        return;
    }

    if (parse_seniverse_response(response, &weather_info_new))
    {
        if (memcmp(&weather_info_new, &weather_info_old, sizeof(weather_info_t)) != 0)
        {
            //main_page_redraw_outdoor_city(weather_info_new.city);
            main_page_redraw_outdoor_temperature(weather_info_new.temperature);
            main_page_redraw_outdoor_weather_icon(weather_info_new.weather_code);
            memcpy(&weather_info_old, &weather_info_new, sizeof(weather_info_t));
        }
    }
    else
    {
        printf("outdoor update false\r\n");
    }
}

static void time_sync(void)
{
    if (esp_at_sntp_get_time(&date_time_new) && date_time_new.year >= 2020)
    {
        rtc_date_time_t rtc_date_time;

        rtc_date_time.day = date_time_new.day;
        rtc_date_time.hour = date_time_new.hour;
        rtc_date_time.minute = date_time_new.minute;
        rtc_date_time.month = date_time_new.month;
        rtc_date_time.second = date_time_new.second;
        rtc_date_time.weekday = date_time_new.weekday;
        rtc_date_time.year = date_time_new.year;

        rtc_set_time(&rtc_date_time);
        xTimerChangePeriod(time_sync_timer, TIME_SYNC_INTERVAL_MS / portTICK_PERIOD_MS, 0);
        xTaskNotify(mloop_task, MLOOP_EVT_TIME_UPDATE, eSetBits);
        return;
    }
    else
    {
        printf("time sync error\r\n");
        goto error;
    }

error:
    xTimerChangePeriod(time_sync_timer, 1000 / portTICK_PERIOD_MS, 0);
}

static void mloop_func(void *param)
{
    while (1)
    {
        uint32_t event = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (event & MLOOP_EVT_TIME_UPDATE)
        {
            time_update();
        }
        if (event & MLOOP_EVT_WIFI_UPDATE)
        {
            wifi_update();
        }
        if (event & MLOOP_EVT_INNER_UPDATE)
        {
            inner_update();
        }
        if (event & MLOOP_EVT_OUTDOOR_UPDATE)
        {
            outdoor_update();
        }
        if (event & MLOOP_EVT_TIME_SYNC)
        {
            time_sync();
        }
    }
}

static void vtimercallback(TimerHandle_t xTimer)
{
    xTaskNotify(mloop_task, (uint32_t)pvTimerGetTimerID(xTimer), eSetBits);
}

void main_loop_init()
{
    time_sync_timer = xTimerCreate("time_sync", 1, pdFALSE, (void *)MLOOP_EVT_TIME_SYNC, vtimercallback);
    time_update_timer = xTimerCreate("time_update", pdMS_TO_TICKS(TIME_UPDATE_INTERVAL_MS), pdTRUE, (void *)MLOOP_EVT_TIME_UPDATE, vtimercallback);
    wifi_update_timer = xTimerCreate("wifi_update", pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL_MS), pdTRUE, (void *)MLOOP_EVT_WIFI_UPDATE, vtimercallback);
    inner_update_timer = xTimerCreate("inner_update", pdMS_TO_TICKS(INNER_UPDATE_INTERVAL_MS), pdTRUE, (void *)MLOOP_EVT_INNER_UPDATE, vtimercallback);
    outdoor_update_timer = xTimerCreate("outdoor_update", pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL_MS), pdTRUE, (void *)MLOOP_EVT_OUTDOOR_UPDATE, vtimercallback);

    xTaskCreate(mloop_func, "mloop_func", 1024, NULL, 5, &mloop_task);

    xTaskNotify(mloop_task, MLOOP_EVENT_ALL, eSetBits);

    xTimerStart(time_update_timer, 0);
    xTimerStart(wifi_update_timer, 0);
    xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);
}
