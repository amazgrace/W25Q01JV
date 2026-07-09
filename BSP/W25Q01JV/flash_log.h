/**
 * @file flash_log.h
 * @brief 环形追加日志系统业务层头文件
 * @note  APP 层引用此文件即可，无需关心底层 Flash 物理特性
 */

#ifndef FLASH_LOG_H
#define FLASH_LOG_H

#include <stdint.h>
#include <stdbool.h>

/* ================= 业务层数据格式定义 (拿到通信文档后只改这里) ================= */

// 定义日志类型枚举
typedef enum {
    LOG_TYPE_SYSTEM_STATUS = 0x01, // 系统定时运行信息
    LOG_TYPE_FAULT_ALARM   = 0x02, // 突发故障报警信息
    LOG_TYPE_DEBUG_MSG     = 0x03  // 研发阶段调试信息
} LogRecordType_e;

// 业务载荷1：故障报警记录 (举例)
#pragma pack(push, 1)
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
#pragma pack(pop)

/* ================= 应用层使用 API ================= */

/**
 * @brief 系统上电初始化：扫描并寻找上次断电前的写指针
 */
void FlashLog_Init(void);

/**
 * @brief 业务接口：保存一条【系统故障】日志
 * @param timestamp 当前时间戳 (如系统运行毫秒数，或 RTC 时间)
 * @param fault_data 填好的故障结构体指针
 * @retval true 写入成功, false 写入失败或超长
 */
bool FlashLog_SaveFault(uint32_t timestamp, FaultRecord_t *fault_data);

/**
 * @brief 业务接口：保存一条【系统状态】日志
 * @param timestamp 当前时间戳
 * @param status_data 填好的状态结构体指针
 * @retval true 写入成功, false 写入失败或超长
 */
bool FlashLog_SaveSystemStatus(uint32_t timestamp, SysStatusRecord_t *status_data);

#endif /* FLASH_LOG_H */