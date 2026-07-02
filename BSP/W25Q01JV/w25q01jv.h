#ifndef __W25Q01JV_H
#define __W25Q01JV_H

#include <stdint.h>

// ================== 指令宏定义 ==================
#define W25Q_CMD_WRITE_ENABLE         0x06
#define W25Q_CMD_WRITE_DISABLE        0x04
#define W25Q_CMD_READ_STAT1           0x05  // 读状态寄存器1 (BUSY位)
#define W25Q_CMD_READ_STAT2           0x35  // 读状态寄存器2 (SUS位)
#define W25Q_CMD_READ_STAT3           0x15  // 读状态寄存器3 (ADS位)
#define W25Q_CMD_JEDEC_ID             0x9F  // 读JEDEC ID
#define W25Q_CMD_DEVICE_ID            0x90  // 读设备ID (需要地址0x000000)

#define W25Q_CMD_ENTER_4B_MODE        0xB7  // 进入4字节地址模式
#define W25Q_CMD_EXIT_4B_MODE         0xE9  // 退出4字节地址模式

#define W25Q_CMD_POWER_DOWN           0xB9  // 进入掉电模式
#define W25Q_CMD_RELEASE_POWER_DOWN   0xAB  // 释放掉电模式
#define W25Q_CMD_SET_READ_PARAM       0xC0  // 设置读取参数

// --- 在 3-Byte 模式下的标准读写指令（在进入4字节地址模式后可以正常使用） ---
#define W25Q_CMD_READ_DATA            0x03  // 普通读数据
#define W25Q_CMD_FAST_READ            0x0B  // 快速读
#define W25Q_CMD_PAGE_PROGRAM         0x02  // 页编程
#define W25Q_CMD_SECTOR_ERASE         0x20  // 扇区擦除(4KB)
#define W25Q_CMD_BLOCK_ERASE_32K      0x52  // 块擦除(32KB)
#define W25Q_CMD_BLOCK_ERASE_64K      0xD8  // 块擦除(64KB)

// --- 在 4-Byte 模式下的标准读写指令 ---
#define W25Q_CMD_READ_DATA_4B         0x13  // 4字节地址普通读数据（50MHz）
#define W25Q_CMD_FAST_READ_4B         0x0C  // 4字节地址快速读 (最高133MHz)
#define W25Q_CMD_PAGE_PROGRAM_4B      0x12  // 4字节地址页编程(最大256字节)
#define W25Q_CMD_SECTOR_ERASE_4B      0x21  // 4字节地址扇区擦除(4KB)
#define W25Q_CMD_BLOCK_ERASE_64K_4B   0xDC  // 块擦除(64KB)
#define W25Q_CMD_CHIP_ERASE        0xC7  // 整片擦除(慎用)

// ================== 外部函数声明 ==================
// 核心基础
void W25Q01JV_WaitBusy(void);
void W25Q01JV_WriteEnable(void);
uint32_t W25Q01JV_Read_JEDEC_ID(void);
uint16_t W25Q01JV_Read_DEVICE_ID(void);

// 4字节模式管理
void W25Q01JV_Enter4ByteMode(void);
uint8_t W25Q01JV_Check4ByteMode(void);

// 扇区/块擦除指令
void W25Q01JV_EraseSector_4K_4B(uint32_t Address);
void W25Q01JV_EraseBlock_64K_4B(uint32_t Address);
void W25Q01JV_EraseSector_4K(uint32_t Address);
void W25Q01JV_EraseBlock_32K(uint32_t Address);
void W25Q01JV_EraseBlock_64K(uint32_t Address);
void W25Q01JV_EraseChip(void);

// 4 字节地址专属读写指令
void W25Q01JV_WritePage_4B(uint32_t Address, uint8_t *pBuffer, uint16_t Size);
void W25Q01JV_ReadData_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size);
void W25Q01JV_FastReadData_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size);
void W25Q01JV_WriteBuffer_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size);

// 普通 3 字节地址读写指令集合
void W25Q01JV_FastReadData(uint32_t Address, uint8_t *pBuffer, uint32_t Size);
void W25Q01JV_ReadData(uint32_t Address, uint8_t *pBuffer, uint32_t Size);
void W25Q01JV_WritePage(uint32_t Address, uint8_t *pBuffer, uint16_t Size);
void W25Q01JV_WriteBuffer(uint32_t Address, uint8_t *pBuffer, uint32_t Size);

#endif // __W25Q01JV_H 
