#include "w25q01jv_port.h"

#include "main.h"
#include "spi.h"
#include "gpio.h"

void W25Q01JV_CS_Enable(void)
{
    HAL_GPIO_WritePin(W25Q01JV_CS_GPIO_Port, W25Q01JV_CS_Pin, GPIO_PIN_RESET);
}
void W25Q01JV_CS_Disable(void)
{
    HAL_GPIO_WritePin(W25Q01JV_CS_GPIO_Port, W25Q01JV_CS_Pin, GPIO_PIN_SET);
}

void W25Q01JV_Delay_ms(uint32_t ms) {
    HAL_Delay(ms); // 绑定 STM32 的系统滴答延时
}

// 接口 1：单字节双向交换 (用于短指令、读状态)
uint8_t W25Q01JV_SPI_SwapByte(uint8_t txData) {
    uint8_t rxData;
    HAL_SPI_TransmitReceive(&hspi5, &txData, &rxData, 1, 100);
    return rxData;
}

// 接口 2：连续发送数组 (用于页编程写数据、发地址组合)
void W25Q01JV_SPI_Transmit(uint8_t *pData, uint16_t size) {
    HAL_SPI_Transmit(&hspi5, pData, size, HAL_MAX_DELAY);
    // 如果想提高速度，可以直接把这里改成 DMA 方式
    // HAL_SPI_Transmit_DMA(&hspi5, pData, size); 
}

// 接口 3：连续接收数组 (用于读取 Flash 数据)
void W25Q01JV_SPI_Receive(uint8_t *pData, uint16_t size) {
    HAL_SPI_Receive(&hspi5, pData, size, HAL_MAX_DELAY);
}
