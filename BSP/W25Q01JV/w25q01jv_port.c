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

/**
 * @brief SPI 底层多字节发送接口 (带超大容量分块防溢出机制)
 * @param pData 要发送的数据指针
 * @param size  要发送的总字节数 (32位，支持发送超过64KB的数据)
 */
void W25Q01JV_SPI_Transmit(uint8_t *pData, uint32_t size) {
    uint32_t remain_size = size;    // 剩余需要发送的长度
    uint8_t *pBuffer = pData;       // 当前数据发送指针
    uint16_t current_transmit;      // 本次实际交给 HAL 库的发送长度

    while (remain_size > 0) {
        // 迎合 HAL 库的 16 位参数限制，超过 65535 就切片
        if (remain_size > 65535) {
            current_transmit = 65535;
        } else {
            current_transmit = (uint16_t)remain_size;
        }
        
        // 调用 HAL 库，发送这一小块
        HAL_SPI_Transmit(&hspi5, pBuffer, current_transmit, HAL_MAX_DELAY);
        
        // 推进指针，扣减剩余长度
        pBuffer += current_transmit;
        remain_size -= current_transmit;
    }
}

// 接口 3：连续接收数组 (用于读取 Flash 数据)
void W25Q01JV_SPI_Receive(uint8_t *pData, uint32_t size) {
    uint32_t remain_size = size;    // 剩余需要读取的长度
    uint8_t *pBuffer = pData;       // 当前数据存放指针
    uint16_t current_read;          // 本次实际交给 HAL 库的读取长度

    while (remain_size > 0) {
        // 如果剩余数据大于 65535 (uint16_t 上限)，就按 65535 切一刀；否则就把剩下的全读完
        if (remain_size > 65535) {
            current_read = 65535;
        } else {
            current_read = (uint16_t)remain_size;
        }
        
        // 调用 HAL 库，读这一小块
        HAL_SPI_Receive(&hspi5, pBuffer, current_read, HAL_MAX_DELAY);
        
        // 推进指针，扣减剩余长度
        pBuffer += current_read;
        remain_size -= current_read;
    }
}
