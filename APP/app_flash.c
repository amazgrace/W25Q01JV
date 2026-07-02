#include "app_flash.h"
#include "w25q01jv.h"  // 引入底层驱动
#include <stdio.h>
#include <string.h>

/**
 * @brief Flash 业务层初始化
 */
void App_Flash_Init(void) {
    uint32_t id = W25Q01JV_Read_JEDEC_ID();
    if (id == 0xEF4021) {
        printf("[APP_FLASH] W25Q01JV Mount Success!\r\n");
    } else {
        printf("[APP_FLASH] W25Q01JV Mount Failed! ID: 0x%06X\r\n", id);
        // 这里可以加蜂鸣器报警，或者让系统进入错误状态
    }
}

/**
 * @brief 保存系统参数到 Flash
 * @note  应用层绝活：必须遵循 "读->改(略)->擦->写" 的真理
 */
void App_Flash_SaveParam(SysParam_t *pParam) {
    printf("[APP_FLASH] Saving System Parameters...\r\n");

    // 1. 擦除存放参数的目标扇区 (使用我们写好的带防呆的4字节指令版)
    W25Q01JV_EraseSector_4K_4B(FLASH_PART_SYS_PARAM);

    // 2. 直接将结构体指针强转为 uint8_t*，连着结构体的大小一起写进去
    W25Q01JV_WriteBuffer_4B(FLASH_PART_SYS_PARAM, (uint8_t *)pParam, sizeof(SysParam_t));

    printf("[APP_FLASH] Parameter Saved.\r\n");
}

/**
 * @brief 从 Flash 加载系统参数
 */
void App_Flash_LoadParam(SysParam_t *pParam) {
    // 读数据不需要擦除，直接按结构体大小抽出来盖在 RAM 里即可
    W25Q01JV_ReadData_4B(FLASH_PART_SYS_PARAM, (uint8_t *)pParam, sizeof(SysParam_t));
    
    // 如果是第一次开机，Flash里全是 0xFF，可以用这个特征做个初始化
    if (pParam->boot_count == 0xFFFFFFFF) {
        printf("[APP_FLASH] First boot detected. Loading default parameters.\r\n");
        pParam->boot_count = 0;
        pParam->target_temp = 25.5f;
        pParam->ip_addr[0] = 192;
        pParam->ip_addr[1] = 168;
        pParam->ip_addr[2] = 1;
        pParam->ip_addr[3] = 100;
        // 存回 Flash
        App_Flash_SaveParam(pParam);
    } else {
        printf("[APP_FLASH] Parameters Loaded. Boot Count: %d\r\n", pParam->boot_count);
    }
}