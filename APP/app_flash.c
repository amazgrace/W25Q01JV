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

/**
 * @brief Flash 读写可靠性测试
 * @note  真测真写：
 *        1. 擦除 -> 2. 写入特定模式 -> 3. 读取 -> 4. 逐字节比对
 *        全部通过才算测试成功，只要有一个字节不对就报 FAIL
 */
void App_Flash_Test(void) {
    uint8_t write_buf[256];
    uint8_t read_buf[256];
    uint32_t test_addr = FLASH_PART_USER_DATA;  // 用分区表里第二个扇区来测试

    printf("\r\n");
    printf("========================================\r\n");
    printf("  W25Q01JV Flash Read/Write Test Start\r\n");
    printf("========================================\r\n");

    // -------------------------------------------------
    // Step 1: 准备测试数据 — 使用多种模式交叉填充
    // -------------------------------------------------
    // 模式1: 递增数 (0x00, 0x01, 0x02 ... 0xFF)
    for (int i = 0; i < 128; i++) {
        write_buf[i] = (uint8_t)i;
    }
    // 模式2: 固定值交错 (0xAA, 0x55, 0xAA, 0x55 ...)
    for (int i = 128; i < 256; i++) {
        write_buf[i] = (i % 2 == 0) ? 0xAA : 0x55;
    }

    printf("[TEST] Test Pattern Generated.\r\n");

    // -------------------------------------------------
    // Step 2: 擦除测试扇区
    // -------------------------------------------------
    printf("[TEST] Erasing Sector (0x%08lX)...\r\n", test_addr);
    W25Q01JV_EraseSector_4K_4B(test_addr);
    printf("[TEST] Erase Done.\r\n");

    // -------------------------------------------------
    // Step 3: 写入 256 字节 (正好一页)
    // -------------------------------------------------
    printf("[TEST] Writing 256 bytes to 0x%08lX...\r\n", test_addr);
    W25Q01JV_WriteBuffer_4B(test_addr, write_buf, 256);
    printf("[TEST] Write Done.\r\n");

    // -------------------------------------------------
    // Step 4: 读取回来
    // -------------------------------------------------
    printf("[TEST] Reading back 256 bytes from 0x%08lX...\r\n", test_addr);
    memset(read_buf, 0x00, 256);
    W25Q01JV_ReadData_4B(test_addr, read_buf, 256);
    printf("[TEST] Read Done.\r\n");

    // -------------------------------------------------
    // Step 5: 逐字节比对
    // -------------------------------------------------
    uint32_t error_count = 0;
    for (int i = 0; i < 256; i++) {
        if (write_buf[i] != read_buf[i]) {
            error_count++;
            if (error_count <= 5) {  // 只打印前 5 个错误，避免刷屏
                printf("[TEST]  MISMATCH at offset %d: wrote 0x%02X, read 0x%02X\r\n",
                       i, write_buf[i], read_buf[i]);
            }
        }
    }

    // -------------------------------------------------
    // Step 6: 打印测试结论
    // -------------------------------------------------
    printf("----------------------------------------\r\n");
    if (error_count == 0) {
        printf("[TEST] RESULT: PASS! All 256 bytes match.\r\n");
    } else {
        printf("[TEST] RESULT: FAIL! %lu mismatches found.\r\n", error_count);
    }
    printf("========================================\r\n\r\n");
}