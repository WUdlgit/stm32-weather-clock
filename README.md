# STM32 桌面环境信息终端（FreeRTOS + ESP-AT）

STM32F407VET6 上的桌面环境信息终端：FreeRTOS 任务调度、ESP-AT 联网校时与天气获取、AHT20 温湿度采集、ST7789 LCD 界面，以及配合串口 IAP Bootloader 的升级入口。

> 公开仓库只保留可公开源码与占位配置，不包含真实 WiFi 凭据、天气服务密钥、原始日志、整片 Flash 备份和本地学习记录。

## 功能

- FreeRTOS 多任务：显示/业务/网络/传感器任务通过任务通知、消息队列和信号量协作。
- ESP-AT 联网：串口 AT 指令连接 WiFi，SNTP 校时，HTTP 获取天气 JSON。
- 环境采集：AHT20 温湿度数据读取。
- LCD 显示：ST7789 屏幕，字符/图片与页面刷新。
- 升级入口：长按 KEY2 约 2 秒，向 RTC 备份寄存器 BKP19R 写入一次性请求并软件复位，由 Bootloader 读取后留在升级模式。
- App 链接到 `0x08010000`，`SystemInit` 使用 `VECT_TAB_OFFSET=0x10000`，与串口 IAP Bootloader 的分区约定一致。

## 目录

```text
app/        应用、页面、图片/字体资源、升级入口
driver/     ESP-AT、LCD、AHT20、RTC、按键、LED、日志等驱动
third_lib/  FreeRTOS 与第三方组件
firmware/   STM32F4 标准外设库与 CMSIS
Makefile    GCC/Make 构建
```

## 构建

工具：Arm GNU Toolchain、GNU Make。

```powershell
make -j4
```

生成 `build/stm32f407.elf`、`.hex`、`.bin`。`make clean` 清理构建产物；`make flash` 可使用 ST-Link/OpenOCD 烧录（按本机安装情况配置）。

## 本地凭据配置

真实 WiFi 与天气服务密钥不会提交。构建前按需创建以下被 `.gitignore` 忽略的文件：

1. 复制 `app/weather_config_local.example.h` 为 `app/weather_config_local.h`，填入天气服务密钥。
2. 创建 `app/wifi_config_local.h`：

```c
#define WIFI_SSID   "your_wifi_ssid"
#define WIFI_PASSWD "your_wifi_password"
```

Makefile 检测到这两个本地文件时会自动定义 `WIFI_CREDENTIALS_LOCAL` / `WEATHER_CREDENTIALS_LOCAL`；没有本地文件时使用 `app/app.h` 中的占位符。

## 与 Bootloader 的配合

串口 IAP Bootloader 位于 [WUdlgit/bootloader](https://github.com/WUdlgit/bootloader)。升级接口为 USART3（PD8 TX、PD9 RX），日志口为 USART1（PA9/PA10）。App 镜像通过 `tools/upgrade.py` 发送，升级完成后由 Bootloader 校验并交接启动。

本仓库只包含 App 侧代码；Bootloader、分区表、升级协议和板端验证记录不在本仓库重复。

## 范围

- 单 App 原地更新，不包含回滚、断点续传、外部 Flash 暂存或云端 OTA。
- 公开版本不包含实际硬件测试日志、LCD 目视记录和整片 Flash 备份。
- 屏幕交互、长期运行和真实断电恢复未在公开材料中声明为已验证。
