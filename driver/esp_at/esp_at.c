#include "esp_at.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_usart.h"
#include "task.h"


#define ESP_AT_DEBUG 1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum
{
    AT_ACK_NONE,
    AT_ACK_OK,
    AT_ACK_ERROR,
    AT_ACK_BUSY,
    AT_ACK_READY,
} at_ack_t;

typedef struct
{
    at_ack_t ack;
    const char *string;
} at_ack_match_t;

static const at_ack_match_t at_ack_matches[] = {
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p...\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};

static char rxbuf[1024];
static char txbuf[1024];
static uint32_t rxlen_isr;
static SemaphoreHandle_t ack_semaphore, semphr;
static at_ack_t rxack;
static const char *line = rxbuf;
static char tx_dma_buf[sizeof(txbuf) + 2];

static void esp_at_usart_write(const char *data);
static bool esp_at_wait_boot(uint32_t timeout);

static void esp_at_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

static void esp_at_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream6, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
}

static void esp_at_interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

bool esp_at_init(void)
{
    esp_at_usart_init();
    esp_at_dma_init();
    esp_at_interrupt_init();

    ack_semaphore = xSemaphoreCreateBinary();
    configASSERT(ack_semaphore);
    semphr = xSemaphoreCreateBinary();
    configASSERT(semphr);

    if (!esp_at_wait_boot(3000))
        return false;
    if (!esp_at_write_command("AT+RESTORE", 2000))
        return false;
    if (!esp_at_wait_ready(5000))
        return false;

    return true;
}

static void esp_at_usart_write(const char *data)
{
    size_t len = strlen(data);

    configASSERT(len <= sizeof(tx_dma_buf) - 2);

    memcpy(tx_dma_buf, data, len);
    tx_dma_buf[len] = '\r';
    tx_dma_buf[len + 1] = '\n';

    DMA1_Stream6->M0AR = (uint32_t)tx_dma_buf;
    DMA1_Stream6->NDTR = len + 2;

    DMA_Cmd(DMA1_Stream6, ENABLE);
    xSemaphoreTake(semphr, portMAX_DELAY);

    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}  

static at_ack_t match_internal_ack(const char *str)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)
    {
        if (strcmp(str, at_ack_matches[i].string) == 0)
            return at_ack_matches[i].ack;
    }

    return AT_ACK_NONE;
}

static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)
{
    if (xSemaphoreTake(ack_semaphore, timeout) == pdTRUE)
    {
        return rxack;
    }
    else
    {
        return AT_ACK_NONE;
    }

    return AT_ACK_NONE;
}

bool esp_at_wait_ready(uint32_t timeout)
{
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}

bool esp_at_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", command);
#endif

    rxack = 0;
    rxlen_isr = 0;
    xSemaphoreTake(ack_semaphore, 0);
    line = rxbuf;
    for (uint32_t i = 0; i < sizeof(rxbuf) - 1; i++)
    {
        rxbuf[i] = 0;
    }
    
    esp_at_usart_write(command);

    at_ack_t ack = esp_at_usart_wait_receive(timeout);

#if ESP_AT_DEBUG
    printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif

    return ack == AT_ACK_OK;
}

const char *esp_at_get_response(void)
{
    return rxbuf;
}

static bool esp_at_wait_boot(uint32_t timeout)
{
    for (int t = 0; t < timeout; t += 100)
    {
        if (esp_at_write_command("AT", 100))
            return true;
    }

    return false;
}

bool esp_at_wifi_init(void)
{
    return esp_at_write_command("AT+CWMODE=1", 2000);
}

bool esp_at_connect_wifi(const char *ssid, const char *pwd, const char *mac)
{
    if (ssid == NULL || pwd == NULL)
        return false;

    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (mac)
        snprintf(cmd + len, sizeof(cmd) - len, ",\"%s\"", mac);

    return esp_at_write_command(cmd, 5000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)
{
    //    AT+CWSTATE?
    //    +CWSTATE:2,"Xiaomi Mi MIX 3_5577"

    //    OK
    response = strstr(response, "+CWSTATE:");
    if (response == NULL)
        return false;

    int wifi_state;
    if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
        return false;

    info->connected = (wifi_state == 2);

    return true;
}

static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)
{
    //    AT+CWJAP?
    //    +CWJAP:"Xiaomi Mi MIX 3_5577","da:b5:3a:e3:2f:60",9,-48,0,1,3,0,1

    //    OK
    response = strstr(response, "+CWJAP:");
    if (response == NULL)
        return false;

    if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid,
               &info->channel, &info->rssi) != 4)
        return false;

    return true;
}

bool esp_at_get_wifi_info(esp_wifi_info_t *info)
{
    if (!esp_at_write_command("AT+CWSTATE?", 2000))
        return false;

    if (!parse_cwstate_response(esp_at_get_response(), info))
        return false;

    if (info->connected == true)
    {
        if (!esp_at_write_command("AT+CWJAP?", 2000))
            return false;

        if (!parse_cwjap_response(esp_at_get_response(), info))
            return false;
    }

    return true;
}

bool wifi_is_connected(void)
{
    esp_wifi_info_t info;
    if (esp_at_get_wifi_info(&info))
    {
        return info.connected;
    }
    return false;
}

bool esp_at_sntp_init(void)
{
    if (!esp_at_write_command("AT+CIPSNTPCFG=1,8", 2000))
        return false;

    return true;
}

static uint8_t month_str_to_num(const char *month_str)
{
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; i++)
    {
        if (strcmp(month_str, months[i]) == 0)
        {
            return i + 1;
        }
    }
    return 0;
}


static uint8_t weekday_str_to_num(const char *weekday_str)
{
    const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (uint8_t i = 0; i < 7; i++)
    {
        if (strcmp(weekday_str, weekdays[i]) == 0)
        {
            return i + 1;
        }
    }
    return 0;
}

static bool parse_two_num(const char *text, uint8_t *value)
{
    if (text == NULL || value == NULL)
        return false;

    if (text[0] >= '0' && text[0] <= '9' && text[1] >= '0' && text[1] <= '9')
    {
        *value = (text[0] - '0') * 10 + (text[1] - '0');
        return true;
    }

    return false;
}

static bool parse_hour_min_sec(const char *text, esp_date_time_t *date)
{
    if (!parse_two_num(&text[24], &date->hour))
        return false;
    if (!parse_two_num(&text[27], &date->minute))
        return false;
    if (!parse_two_num(&text[30], &date->second))
        return false;

    if (date->hour > 23)
        return false;
    if (date->minute > 59)
        return false;
    if (date->second > 59)
        return false;

    return true;
}

static bool parse_day(const char *text, esp_date_time_t *date)
{
    if (text == NULL || date == NULL)
        return false;

    if (text[21] == ' ')
    {
        if (text[22] > '0' && text[22] <= '9')
        {
            date->day = text[22] - '0';
        }
    }
    else 
    {
        if (!parse_two_num(&text[21], &date->day))
            return false;
    }

    if (date->day > 0 && date->day <= 31)
        return true;

    return false;
}

static bool parse_year(const char *text, esp_date_time_t *date)
{
    if (text == NULL || date == NULL)
        return false;

    if (text[33] - '0' >= 0 && text[34] - '0' >= 0 && text[35] - '0' >= 0 && text[36] - '0' >= 0)
    {
        if (text[33] - '0' <= 9 && text[34] - '0' <= 9 && text[35] - '0' <= 9 && text[36] - '0' <= 9)
        {
            date->year = (text[33] - '0') * 1000 + (text[34] - '0') * 100 + (text[35] - '0') * 10 + (text[36] - '0');
            if (date->year < 2020)
                return false;

            return true;
        }
    }
    
    return false;
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)
{
    //	AT+CIPSNTPTIME?
    //	+CIPSNTPTIME:Sun Jul 27 14:07:19 2025
    //	OK
    char weekday_str[8];
    char month_str[4];
    esp_date_time_t date_temporary = {0};
    if (response == NULL || date == NULL)
        return false;

    const char *start = strstr(response, "+CIPSNTPTIME:");
    if (start == NULL)
        return false;


    if (strlen(start) >= 37)
    {
        if (start[16] != ' ' || start[20] != ' ' || start[23] != ' ' || start[26] != ':' || start[29] != ':' || start[32] != ' ')
        {
            return false;
        }

        if (!parse_hour_min_sec(start, &date_temporary))
            return false;
        if (!parse_day(start, &date_temporary))
            return false;
        if (!parse_year(start, &date_temporary))
            return false;

        weekday_str[0] = start[13]; weekday_str[1] = start[14]; weekday_str[2] = start[15]; weekday_str[3] = '\0';
        month_str[0] = start[17]; month_str[1] = start[18]; month_str[2] = start[19]; month_str[3] = '\0';
    }
    else
    {
        return false;
    }

    date_temporary.weekday = weekday_str_to_num(weekday_str);
    if (date_temporary.weekday == 0) 
        return false;
    date_temporary.month = month_str_to_num(month_str);
    if (date_temporary.month == 0) 
        return false;

    *date = date_temporary;
    
    return true;
}

bool esp_at_sntp_get_time(esp_date_time_t *date)
{
    if (!esp_at_write_command("AT+CIPSNTPTIME?", 2000))
        return false;

    if (!parse_cipsntptime_response(esp_at_get_response(), date))
        return false;

    return true;
}

const char *esp_at_http_get(const char *url)
{
    //    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=<api-key>&location=<location>&language=en&unit=c",,,2
    //    +HTTPCLIENT:261,{"results":[{"location":{"id":"WTEMH46Z5N09","name":"Hefei","country":"CN","path":"Hefei,Hefei,Anhui,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Cloudy","code":"4","temperature":"32"},"last_update":"2025-07-26T16:30:00+08:00"}]}

    //    OK
    snprintf(txbuf, sizeof(txbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2", url);
    bool ret = esp_at_write_command(txbuf, 10000);
    return ret ? esp_at_get_response() : NULL;
}

void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        char data = USART_ReceiveData(USART2);

        if (rxlen_isr < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen_isr++] = data;
            rxbuf[rxlen_isr] = '\0';
            if (rxbuf[rxlen_isr - 1] == '\n')
            {
                rxack = match_internal_ack(line);
                line = rxbuf + rxlen_isr;
                if (rxack > 0)
                {
                    xSemaphoreGiveFromISR(ack_semaphore, &xHigherPriorityTaskWoken);
                    USART_ClearITPendingBit(USART2, USART_IT_RXNE);
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }
            }
        }
    }
}

void DMA1_Stream6_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; 

    if (DMA_GetITStatus(DMA1_Stream6, DMA_IT_TCIF6) == SET)
    {
        xSemaphoreGiveFromISR(semphr, &xHigherPriorityTaskWoken);
        DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
