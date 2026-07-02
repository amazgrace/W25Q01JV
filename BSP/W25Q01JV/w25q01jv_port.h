#ifndef __W25Q01JV_PORT_H
#define __W25Q01JV_PORT_H

#include <stdint.h>

void W25Q01JV_CS_Enable(void);
void W25Q01JV_CS_Disable(void);
void W25Q01JV_Delay_ms(uint32_t ms);
uint8_t W25Q01JV_SPI_SwapByte(uint8_t txData);
void W25Q01JV_SPI_Transmit(uint8_t *pData, uint32_t size);
void W25Q01JV_SPI_Receive(uint8_t *pData, uint32_t size);

#endif // __W25Q01JV_PORT_H

