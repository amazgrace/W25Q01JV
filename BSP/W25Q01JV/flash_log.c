/**
 * @file flash_log.c
 * @brief 适配 W25Q01JV 的环形追加日志系统 (带业务层接口预留)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "w25q01jv.h" 

/* ================= 1. 硬件参数配置 (针对 W25Q01JV 128MB) ================= */
#define FLASH_SECTOR_SIZE   4096       // 最小擦除单位 4KB
#define FLASH_SECTOR_COUNT  32768     // 分配 32768 个扇区 = 128MB 专门用于日志存储
#define FLASH_TOTAL_SIZE    (FLASH_SECTOR_SIZE * FLASH_SECTOR_COUNT)
#define FLASH_BASE_ADDR     0x00000000 // 日志区域起始地址 (0~128MB)
// 提示: 0x04000000 ~ 0x07FFFFFF 为日志区域，使用整个flash芯片的128MB空间

/* ================= 2. 底层驱动绑定 ================= */
#define HAL_Flash_Read(addr, buf, len)       W25Q01JV_FastReadData_4B(addr, buf, len)
#define HAL_Flash_Write(addr, buf, len)      W25Q01JV_WriteBuffer_4B(addr, buf, len)
#define HAL_Flash_EraseSector(sector_addr)   W25Q01JV_EraseSector_4K_4B(sector_addr)

/* ================= 3. 业务层数据格式定义 (对应技术文档修改) ================= */

// 定义日志类型枚举
typedef enum {
    LOG_TYPE_SYSTEM_STATUS = 0x01, // 系统定时运行信息
    LOG_TYPE_FAULT_ALARM   = 0x02, // 突发故障报警信息
    LOG_TYPE_DEBUG_MSG     = 0x03  // 研发阶段调试信息
} LogRecordType_e;

// 业务载荷1：故障报警记录 (举例)
#pragma pack(push, 1) // 保持字节对齐
typedef struct {
    uint16_t error_code;    // 故障码
    uint8_t  sub_system_id; // 发生故障的子系统(比如某个舵机)
    // 拿到文档后，继续往下加：比如当前电流、温度等
} FaultRecord_t;

// 业务载荷2：系统运行状态记录 (举例)
typedef struct {
    uint16_t bus_voltage;   // 总线电压 (单位: 0.1V)
    int16_t  temperature;   // 主板温度
    uint32_t total_run_time;// 总运行时间
    // 拿到文档后，继续往下加...
} SysStatusRecord_t;
#pragma pack(pop) // 恢复默认对齐

/* ================= 4. 底层数据帧结构 (通用包裹层) ================= */
#define RECORD_MAGIC_WORD   0xAA55

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;      // 固定为 0xAA55
    uint8_t  type;       // 对应 LogRecordType_e
    uint16_t length;     // 后面紧跟的 Payload 的长度
    uint32_t timestamp;  // 时间戳 (如系统运行毫秒数，或 RTC 时间)
    // uint8_t payload[];// 这里放实际的 FaultRecord_t 或 SysStatusRecord_t
    // uint8_t checksum; // 尾部附加 1 字节校验和
} RecordHeader_t;
#pragma pack(pop)

/* ================= 内部全局状态 ================= */
static uint32_t current_write_addr = FLASH_BASE_ADDR;
static bool     is_initialized = false;

/* ================= 内部辅助函数 ================= */
// 简单的校验和计算
static uint8_t Calculate_Checksum(uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) sum ^= data[i]; // 简单异或校验
    return sum;
}

// 核心写入逻辑 (对业务层隐藏)
static bool FlashLog_WriteRaw(uint8_t type, uint32_t timestamp, uint8_t *payload, uint16_t payload_len) {
    if (!is_initialized) return false;
    
    // 限制单条记录不能超过 4KB (防止一次写入跨越太多扇区引发擦除逻辑复杂化)
    if (payload_len > 2048) return false; 
    
    uint32_t total_len = sizeof(RecordHeader_t) + payload_len + 1; // 1是校验和长度
    
    // 1. 回绕处理 (如果空间不够了，回到开头)
    if (current_write_addr + total_len > FLASH_BASE_ADDR + FLASH_TOTAL_SIZE) {
        current_write_addr = FLASH_BASE_ADDR;
    }
    
    // 2. 跨扇区擦除保护
    uint32_t current_sector = current_write_addr / FLASH_SECTOR_SIZE;
    uint32_t end_sector = (current_write_addr + total_len - 1) / FLASH_SECTOR_SIZE;
    
    // 如果碰巧写在扇区首个字节，或者数据跨界到了下一个扇区，擦除那个扇区
    if ((current_write_addr % FLASH_SECTOR_SIZE == 0) || (current_sector != end_sector)) {
        HAL_Flash_EraseSector(end_sector * FLASH_SECTOR_SIZE);
    }
    
    // 3. 准备头部
    RecordHeader_t header;
    header.magic = RECORD_MAGIC_WORD;
    header.type = type;
    header.length = payload_len;
    header.timestamp = timestamp;
    
    uint8_t checksum = Calculate_Checksum(payload, payload_len);
    
    // 4. 执行物理写入
    HAL_Flash_Write(current_write_addr, (uint8_t*)&header, sizeof(RecordHeader_t));
    current_write_addr += sizeof(RecordHeader_t);
    
    HAL_Flash_Write(current_write_addr, payload, payload_len);
    current_write_addr += payload_len;
    
    HAL_Flash_Write(current_write_addr, &checksum, 1);
    current_write_addr += 1;
    
    return true;
}

/* ================= 暴露给业务层的 API ================= */

/**
 * @brief 系统上电初始化：寻找上次断电前的写指针
 */
void FlashLog_Init(void) {
    uint32_t addr = FLASH_BASE_ADDR;
    uint32_t check_val = 0;
    
    // 扫描寻找 0xFFFFFFFF 空白区边界
    while (addr < FLASH_BASE_ADDR + FLASH_TOTAL_SIZE) {
        HAL_Flash_Read(addr, (uint8_t*)&check_val, 4);
        if (check_val == 0xFFFFFFFF) {
            current_write_addr = addr;
            is_initialized = true;
            return;
        }
        // 优化：实际上可以按扇区跳跃扫描，找到非全FF的扇区后再细化找字节，以加快 64MB 的扫描速度。
        // 这里暂时使用步进扫描演示原理
        addr += 256; // 每次跳过一页加快点速度，实际工业中必须用二分法。
    }
    
    current_write_addr = FLASH_BASE_ADDR;
    HAL_Flash_EraseSector(current_write_addr);
    is_initialized = true;
}

/**
 * @brief 业务接口：保存一条【系统故障】日志
 * @param timestamp 当前时间戳
 * @param fault_data 填好的故障结构体
 */
bool FlashLog_SaveFault(uint32_t timestamp, FaultRecord_t *fault_data) {
    return FlashLog_WriteRaw(LOG_TYPE_FAULT_ALARM, timestamp, (uint8_t*)fault_data, sizeof(FaultRecord_t));
}

/**
 * @brief 业务接口：保存一条【系统状态】日志
 * @param timestamp 当前时间戳
 * @param status_data 填好的状态结构体
 */
bool FlashLog_SaveSystemStatus(uint32_t timestamp, SysStatusRecord_t *status_data) {
    return FlashLog_WriteRaw(LOG_TYPE_SYSTEM_STATUS, timestamp, (uint8_t*)status_data, sizeof(SysStatusRecord_t));
}