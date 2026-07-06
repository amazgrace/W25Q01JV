#ifndef __APP_FLASH_H
#define __APP_FLASH_H

#include <stdint.h>

// =========================================================
// 1. Flash 空间分区表 (Partition Map)
// 芯片总容量 128MB (0x00000000 ~ 0x07FFFFFF)
// =========================================================
#define FLASH_PART_SYS_PARAM    0x00000000  // [扇区0] 存放系统参数 (4KB)
#define FLASH_PART_USER_DATA    0x00001000  // [扇区1] 存放用户历史数据 (4KB)
#define FLASH_PART_OTA_FW       0x00100000  // [1MB处] 存放 OTA 升级固件包 (预留几MB)
#define FLASH_PART_UI_IMAGES    0x01000000  // [16MB处] 存放屏幕 UI 图片资源
#define FLASH_PART_FONTS        0x04000000  // [64MB处] 存放字库文件

// =========================================================
// 2. 业务层数据结构定义
// =========================================================
// 存一个设备的运行参数
typedef struct {
    uint32_t boot_count;       // 开机次数
    float    target_temp;      // 设定温度
    uint8_t  ip_addr[4];       // 设备 IP 地址
    uint32_t crc32;            // 数据校验和（预留，防止Flash意外断电损坏）
} SysParam_t;

// =========================================================
// 3. 应用层对外接口暴露
// =========================================================
void App_Flash_Init(void);
void App_Flash_Test(void); 
void App_Flash_SaveParam(SysParam_t *pParam);
void App_Flash_LoadParam(SysParam_t *pParam);

#endif