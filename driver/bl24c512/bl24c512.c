#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

#define BL24C512_PAGE_SIZE    128

void bl24c512_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_I2C1);
    
    I2C_InitTypeDef I2C_InitStruct;
    I2C_StructInit(&I2C_InitStruct);
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStruct.I2C_ClockSpeed = 100ul * 1000ul;
    I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_Init(I2C1, &I2C_InitStruct);
	
	I2C_Cmd(I2C1, ENABLE);
}

#define I2C_CHECK_EVENT(EVENT, TIMEOUT) \
    do { \
        uint32_t timeout = TIMEOUT; \
        while (!I2C_CheckEvent(I2C1, EVENT) && timeout > 0) { \
            vTaskDelay(pdMS_TO_TICKS(1)); \
            timeout -= 10; \
        } \
        if (timeout <= 0) \
            return false; \
    } while (0)

static bool bl24c512_ready(void)
{
    uint32_t timeout = 100;
    
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) && timeout > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
    if (timeout == 0) return false;
    
    I2C_Send7bitAddress(I2C1, 0xA0, I2C_Direction_Transmitter);
    
    timeout = 100;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && timeout > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
    
    I2C_GenerateSTOP(I2C1, ENABLE);
    
    return (timeout > 0);  // 只有收到 ACK 才返回 true
}

static bool bl24c512_wait_ready(uint32_t timeout)
{
    while (timeout > 0)
    {
        if (bl24c512_ready())
            return true;
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout -= 10;
    }
    return false;
}

static bool bl24c512_page_write(uint16_t address, uint8_t data[], uint32_t length)
{
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_GenerateSTART(I2C1, ENABLE);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
    I2C_Send7bitAddress(I2C1, 0xA0, I2C_Direction_Transmitter);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 1000);
    I2C_SendData(I2C1, (address >> 8) & 0xff);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    I2C_SendData(I2C1, address & 0xff);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    for (uint32_t i = 0; i < length; i++)
    {
        I2C_SendData(I2C1, data[i]);
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    }
    I2C_GenerateSTOP(I2C1, ENABLE);
    
    return true;
}

static bool bl24c512_page_read(uint16_t address, uint8_t data[], uint32_t length)
{
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_GenerateSTART(I2C1, ENABLE);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
    I2C_Send7bitAddress(I2C1, 0xA0, I2C_Direction_Transmitter);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 1000);
    I2C_SendData(I2C1, (address >> 8) & 0xff);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    I2C_SendData(I2C1, address & 0xff);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING, 1000);
    
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_GenerateSTART(I2C1, ENABLE);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT, 1000);
    I2C_Send7bitAddress(I2C1, 0xA0, I2C_Direction_Receiver);
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 1000);
    for (uint32_t i = 0; i < length; i++)
    {
        if (i == length - 1)
            I2C_AcknowledgeConfig(I2C1, DISABLE);
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, 1000);
        data[i] = I2C_ReceiveData(I2C1);
    }
    I2C_GenerateSTOP(I2C1, ENABLE);
    
    return true;
}

/*
    跨页检测与拆分写入
*/
bool bl24c512_write(uint16_t address, uint8_t data[], uint32_t length)
{
    uint32_t available_size = BL24C512_PAGE_SIZE - (address % BL24C512_PAGE_SIZE);
    uint32_t write_size = length < available_size ? length : available_size;
    while (length > 0)
    {
        if (!bl24c512_wait_ready(5 * 1000))
            return false;
        if (bl24c512_page_write(address, data, write_size) == false)
            return false;
        address += write_size;
        data += write_size;
        length -= write_size;
        write_size = length < BL24C512_PAGE_SIZE ? length : BL24C512_PAGE_SIZE;
    }
    
    return true;
}

bool bl24c512_read(uint16_t address, uint8_t data[], uint32_t length)
{
    uint32_t available_size = BL24C512_PAGE_SIZE - (address % BL24C512_PAGE_SIZE);
    uint32_t read_size = length < available_size ? length : available_size;
    while (length > 0)
    {
        if (bl24c512_page_read(address, data, read_size) == false)
            return false;
        address += read_size;
        data += read_size;
        length -= read_size;
        read_size = length < BL24C512_PAGE_SIZE ? length : BL24C512_PAGE_SIZE;
    }
    
    return true;
}
