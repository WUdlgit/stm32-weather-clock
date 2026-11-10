#ifndef __APP_H__
#define __APP_H__

#define APP_VERSION "v1.0"

/* WiFi 凭据：默认占位符；本机存在 app/wifi_config_local.h（已 gitignore）时，
 * Makefile 会传入 -DWIFI_CREDENTIALS_LOCAL，用真实凭据覆盖（见 wifi_config_local.h）。
 * 目的：凭据永不入库，同时保证任何环境都能编译。 */
#ifdef WIFI_CREDENTIALS_LOCAL
#include "wifi_config_local.h"
#else
#define WIFI_SSID   "your_wifi_ssid"
#define WIFI_PASSWD "your_wifi_password"
#endif

/* 天气服务密钥：默认占位符；本机存在 app/weather_config_local.h 且工程
 * 定义 WEATHER_CREDENTIALS_LOCAL 时，用本地真实密钥覆盖。真实密钥不入库。 */
#ifdef WEATHER_CREDENTIALS_LOCAL
#include "weather_config_local.h"
#else
#define SENIVERSE_API_KEY "your_seniverse_api_key"
#endif

void wifi_init(void);
void wifi_wait_connect(void);

void main_loop_init(void);
void main_loop(void);


#endif /* __APP_H__ */
