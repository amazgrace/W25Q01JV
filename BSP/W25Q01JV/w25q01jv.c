#include "w25q01jv.h"
#include "w25q01jv_port.h"


/**
 * @brief 等待空闲。轮询读取状态寄存器1的Bit0，直到变为0
 */
void W25Q01JV_WaitBusy(void) {
    uint8_t status = 0;
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_READ_STAT1); // 发送读SR1指令
    do {
        // 发送 0xFF(Dummy Byte) 提供时钟，换回内部状态字
        status = W25Q01JV_SPI_SwapByte(0xFF);  
    } while ((status & 0x01) == 0x01); // BUSY位为1时死循环等待
    W25Q01JV_CS_Disable();
}

/**
 * @brief 写使能。执行任何擦写前必须调用
 */
void W25Q01JV_WriteEnable(void) {
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_WRITE_ENABLE);
    W25Q01JV_CS_Disable();
}

/**
 * @brief 读芯片JEDEC识别ID (W25Q01JV应该返回 0xEF4021)
 */
uint32_t W25Q01JV_Read_JEDEC_ID(void) {
    uint32_t id = 0;
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_JEDEC_ID); // 发送9Fh
    // 连续接收3个字节
    id |= (W25Q01JV_SPI_SwapByte(0xFF) << 16); // 厂商ID (EF)
    id |= (W25Q01JV_SPI_SwapByte(0xFF) << 8);  // 存储器类型 (40)
    id |= W25Q01JV_SPI_SwapByte(0xFF);         // 容量ID (21)
    W25Q01JV_CS_Disable();
    
    return id;
}

/**
 * @brief 读芯片设备ID (W25Q01JV应该返回 0xEF20)
 */
uint16_t W25Q01JV_Read_DEVICE_ID(void) {
    uint16_t id = 0;
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_DEVICE_ID); // 发送90h
    // 发送3字节地址 (0x000000)
    W25Q01JV_SPI_SwapByte(0x00);
    W25Q01JV_SPI_SwapByte(0x00);
    W25Q01JV_SPI_SwapByte(0x00);
    // 连续接收2个字节
    id |= (W25Q01JV_SPI_SwapByte(0xFF) << 8);  // 设备ID (EF)
    id |= W25Q01JV_SPI_SwapByte(0xFF);         // 设备ID (20)
    W25Q01JV_CS_Disable();
    
    return id;
}


/**
 * @brief 全局进入 4 字节地址模式
 */
void W25Q01JV_Enter4ByteMode(void) {
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_ENTER_4B_MODE); // 发送B7h
    W25Q01JV_CS_Disable();
}

/**
 * @brief 检查是否成功处于 4 字节模式
 * @retval 1: 是  0: 否
 */
uint8_t W25Q01JV_Check4ByteMode(void) {
    uint8_t sr3 = 0;
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_READ_STAT3); // 读SR3
    sr3 = W25Q01JV_SPI_SwapByte(0xFF);
    W25Q01JV_CS_Disable();
    
    return (sr3 & 0x01); // ADS(Current Address Mode) 在Bit0
}


/**
 * @brief 内部静态辅助函数：把1字节指令和4字节地址拼成5个字节的数组
 * 这样就可以直接用你的底层多字节Transmit接口一次性发出去，避免中断打断和冗余代码
 */
static void Build_Command_Header_4B(uint8_t cmd, uint32_t addr, uint8_t *headerBuf) {
    headerBuf[0] = cmd;
    headerBuf[1] = (addr >> 24) & 0xFF; // A31-24
    headerBuf[2] = (addr >> 16) & 0xFF; // A23-16
    headerBuf[3] = (addr >> 8) & 0xFF;  // A15-8
    headerBuf[4] = addr & 0xFF;         // A7-0
}

/**
 * @brief 内部静态函数：拼装 1字节指令 + 3字节地址
 */
static void Build_Command_Header(uint8_t cmd, uint32_t addr, uint8_t *headerBuf) {
    headerBuf[0] = cmd;
    headerBuf[1] = (addr >> 16) & 0xFF; // A23-16
    headerBuf[2] = (addr >> 8)  & 0xFF; // A15-8
    headerBuf[3] = addr & 0xFF;         // A7-0
}


// =========================================================================
//                            扇区/块擦除指令集合
// =========================================================================

/**
 * @brief 擦除4KB扇区 (地址必须是 4096 的整数倍)
 */
void W25Q01JV_EraseSector_4K_4B(uint32_t Address) {
    uint8_t header[5];

    // 4KB 地址对齐校验
    // 4096 = 0x1000，最低 12 位必须为 0
    if ((Address & 0x0FFF) != 0) {
        return; // 地址未对齐，直接拒绝执行，防止误擦除！
    }

    Build_Command_Header_4B(W25Q_CMD_SECTOR_ERASE_4B, Address, header);
    
    W25Q01JV_WriteEnable(); // 1. 写使能
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 5); // 2. 一次性发送指令+32位地址
    W25Q01JV_CS_Disable();            // 3. 拉高CS，触发内部擦除
    
    W25Q01JV_WaitBusy();    // 4. 死等擦除完成 (扇区擦除一般需几十毫秒)
}

/**
 * @brief 擦除64KB块 (4字节地址专属指令版)
 */
void W25Q01JV_EraseBlock_64K_4B(uint32_t Address) {
    uint8_t header[5];

    // 64KB 地址对齐校验
    // 65536 = 0x10000，最低 16 位必须为 0
    if ((Address & 0xFFFF) != 0) {
        return; // 地址未对齐，直接拒绝执行，防止误擦除！
    }

    Build_Command_Header_4B(W25Q_CMD_BLOCK_ERASE_64K_4B, Address, header);
    W25Q01JV_WriteEnable(); 
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 5); 
    W25Q01JV_CS_Disable();            
    W25Q01JV_WaitBusy();    
}

/**
 * @brief 擦除4KB扇区 (普通 3 字节地址指令版)
 * @note  若未进入4字节模式，地址需限制在 0x00FFFFFF 内
 */
void W25Q01JV_EraseSector_4K(uint32_t Address) {
    uint8_t header[4];

    // 4KB 地址对齐校验
    // 4096 = 0x1000，最低 12 位必须为 0
    if ((Address & 0x0FFF) != 0) {
        return; // 地址未对齐，直接拒绝执行，防止误擦除！
    }

    Build_Command_Header(W25Q_CMD_SECTOR_ERASE, Address, header);
    W25Q01JV_WriteEnable(); 
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 4); 
    W25Q01JV_CS_Disable();            
    W25Q01JV_WaitBusy();    
}

/**
 * @brief 擦除32KB块 (普通 3 字节地址指令版)
 */
void W25Q01JV_EraseBlock_32K(uint32_t Address) {
    uint8_t header[4];

    // 32KB 地址对齐校验
    // 32768 = 0x8000，最低 15 位必须为 0
    if ((Address & 0x7FFF) != 0) {
        return; // 地址未对齐，直接拒绝执行，防止误擦除！
    }

    Build_Command_Header(W25Q_CMD_BLOCK_ERASE_32K, Address, header);
    W25Q01JV_WriteEnable(); 
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 4); 
    W25Q01JV_CS_Disable();            
    W25Q01JV_WaitBusy();    
}

/**
 * @brief 擦除64KB块 (普通 3 字节地址指令版)
 */
void W25Q01JV_EraseBlock_64K(uint32_t Address) {
    uint8_t header[4];

    // 64KB 地址对齐校验
    // 65536 = 0x10000，最低 16 位必须为 0
    if ((Address & 0xFFFF) != 0) {
        return; // 地址未对齐，直接拒绝执行，防止误擦除！
    }

    Build_Command_Header(W25Q_CMD_BLOCK_ERASE_64K, Address, header);
    W25Q01JV_WriteEnable(); 
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 4); 
    W25Q01JV_CS_Disable();            
    W25Q01JV_WaitBusy();    
}

/**
 * @brief 全片擦除 (Chip Erase)
 * @note  警告：1Gb (128MB) 容量的全片擦除极其耗时
 * 根据芯片手册，全片擦除典型耗时约 400 秒，最大甚至更久。
 * 调用此函数会导致单片机阻塞几分钟之久。若有看门狗，务必在 WaitBusy 中喂狗
 */
void W25Q01JV_EraseChip(void) {
    W25Q01JV_WriteEnable(); // 1. 写使能
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_SwapByte(W25Q_CMD_CHIP_ERASE); // 2. 仅发送单字节的擦除指令即可
    W25Q01JV_CS_Disable();                      // 3. 拉高CS，触发Flash内部开始全片擦除
    
    W25Q01JV_WaitBusy();    // 4. 死等擦除完成 (极度耗时)
}



// =========================================================================
//                         4 字节地址专属读写指令集合
// =========================================================================

/**
 * @brief 页编程 (往一页里写数据，Size最大256，不能跨页)
 */
void W25Q01JV_WritePage_4B(uint32_t Address, uint8_t *pBuffer, uint16_t Size) {
    uint8_t header[5];
    // 硬件防呆，防止物理越界回环覆盖
    uint16_t max_allow = 256 - (Address % 256);
    if(Size > max_allow) {
        Size = max_allow; 
    }
    
    Build_Command_Header_4B(W25Q_CMD_PAGE_PROGRAM_4B, Address, header);
    
    W25Q01JV_WriteEnable();
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 5);     // 1. 发送头部 (指令+地址)
    W25Q01JV_SPI_Transmit(pBuffer, Size); // 2. 直接透传业务层数据，不拷贝，效率最高
    W25Q01JV_CS_Disable();                // 3. 拉高CS，启动内部烧写
    
    W25Q01JV_WaitBusy();    // 4. 等待烧写完成
}

/**
 * @brief 写入任意长度的连续数据 (自动处理页对齐，防止跨页回环)
 * @note 使用前必须执行擦除，如果数据超过4KB，需要循环擦除多个扇区或者直接调用块擦除
 * @param Address  写入的起始绝对地址
 * @param pBuffer  要写入的数据缓冲区指针
 * @param Size     要写入的总字节数
 */
void W25Q01JV_WriteBuffer_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    if (Size == 0) return; // 如果没有数据要写，直接返回
    // 1. 计算当前地址所在页的剩余空间 (例如地址是250，256-250=6，当前页只能再写6个字节)
    uint16_t pageremain = 256 - (Address % 256);
    
    // 2. 如果要写的数据总长度 <= 当前页剩余空间，说明不会跨页，直接把 pageremain 更新为真实数据长度
    if (Size <= pageremain) {
        pageremain = Size; 
    }
    
    // 3. 循环切割写入
    while (1) {
        // 调用底层的单页物理写入函数
        // (注意：底层的 WritePage 里面依然要保留 waitBusy 等待上一次擦写完成的逻辑)
        W25Q01JV_WritePage_4B(Address, pBuffer, pageremain);
        
        // 检查是不是全都写完了
        if (Size == pageremain) {
            break; // 数据写完，退出死循环
        } else {
            // 还没写完，推进指针和地址，准备写下一页
            pBuffer += pageremain;   // 数据指针往后移
            Address += pageremain;   // 物理地址往后移 (此时 Address 一定是对齐到页首的，比如 0x0100)
            Size -= pageremain;      // 剩余需要写入的长度减少
            
            // 计算下一次要写的长度：如果剩余数据大于256，一次最多只能写256；否则把剩下的全写完
            if (Size > 256) {
                pageremain = 256;
            } else {
                pageremain = Size;
            }
        }
    }
}

/**
 * @brief 读取任意长度数据 (跨页读、全片读都没问题，只要时钟不断就会自动递增地址)
 * @param Address  256MB范围内的绝对地址 (0x00000000 ~ 0x07FFFFFF)
 * @param pBuffer  接收数据缓冲区指针
 * @param Size     要读取的数据字节数
 */
void W25Q01JV_ReadData_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    uint8_t header[5];
    Build_Command_Header_4B(W25Q_CMD_READ_DATA_4B, Address, header);
    
    W25Q01JV_WaitBusy(); // 读之前确认芯片不在干别的重活
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 5);    // 1. 发送读指令和起始地址
    W25Q01JV_SPI_Receive(pBuffer, Size); // 2. 使用你的批量接收接口，一次性抽回所有数据
    W25Q01JV_CS_Disable();
}

/**
 * @brief 快速读取任意长度数据 (兼容 SPI 时钟 > 50MHz，最高可达 133MHz)
 * @note  使用 4 字节专属指令 0x0C，自带 4 字节地址解析，无需切换芯片状态
 * @param Address  256MB范围内的绝对地址 (0x00000000 ~ 0x07FFFFFF)
 * @param pBuffer  接收数据缓冲区指针
 * @param Size     要读取的数据字节数
 */
void W25Q01JV_FastReadData_4B(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    uint8_t header[6]; 
    
    // 1. 拼装 6 字节头部：1字节指令 + 4字节地址 + 1字节 Dummy
    header[0] = W25Q_CMD_FAST_READ_4B;      // 0x0C 指令
    header[1] = (Address >> 24) & 0xFF;     // A31-A24
    header[2] = (Address >> 16) & 0xFF;     // A23-A16
    header[3] = (Address >> 8)  & 0xFF;     // A15-A8
    header[4] = Address & 0xFF;             // A7-A0
    header[5] = 0xFF;                       // 8个 Dummy Clocks (必须发1个字节的空数据)
    
    W25Q01JV_WaitBusy(); // 读之前确认芯片不在执行擦写操作
    
    W25Q01JV_CS_Enable();
    
    // 2. 发送指令、地址和 Dummy Byte
    W25Q01JV_SPI_Transmit(header, 6); 
    
    // 3. 连续接收核心数据 (此时Flash内部已经准备好数据了)
    W25Q01JV_SPI_Receive(pBuffer, Size); 
    
    W25Q01JV_CS_Disable();
}




// =========================================================================
//                         普通 3 字节地址读写指令集合
// =========================================================================

/**
 * @brief 普通读数据 (3 字节地址版，最高 50MHz)
 */
void W25Q01JV_ReadData(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    uint8_t header[4];
    Build_Command_Header(W25Q_CMD_READ_DATA, Address, header); 
    
    W25Q01JV_WaitBusy(); 
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 4);    
    W25Q01JV_SPI_Receive(pBuffer, Size); 
    W25Q01JV_CS_Disable();
}

/**
 * @brief 快速读数据 (3 字节地址版，兼容 SPI 时钟 > 50MHz)
 */
void W25Q01JV_FastReadData(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    uint8_t header[5]; 
    
    header[0] = W25Q_CMD_FAST_READ;      
    header[1] = (Address >> 16) & 0xFF;     
    header[2] = (Address >> 8)  & 0xFF;     
    header[3] = Address & 0xFF;             
    header[4] = 0xFF; // Dummy Byte
    
    W25Q01JV_WaitBusy(); 
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 5);     
    W25Q01JV_SPI_Receive(pBuffer, Size);  
    W25Q01JV_CS_Disable();
}

/**
 * @brief 单页编程 (3 字节地址版，仅供 WriteBuffer 调用)
 */
void W25Q01JV_WritePage(uint32_t Address, uint8_t *pBuffer, uint16_t Size) {
    uint8_t header[4];
    
    uint16_t max_allow = 256 - (Address % 256);
    if(Size > max_allow) {
        Size = max_allow; 
    }
    
    Build_Command_Header(W25Q_CMD_PAGE_PROGRAM, Address, header);
    
    W25Q01JV_WriteEnable();
    
    W25Q01JV_CS_Enable();
    W25Q01JV_SPI_Transmit(header, 4);     
    W25Q01JV_SPI_Transmit(pBuffer, Size); 
    W25Q01JV_CS_Disable();                
    
    W25Q01JV_WaitBusy(); 
}

/**
 * @brief 连续写入数据 (3 字节地址版，带自动页对齐切割)
 */
void W25Q01JV_WriteBuffer(uint32_t Address, uint8_t *pBuffer, uint32_t Size) {
    if (Size == 0) return; // 如果没有数据要写，直接返回

    uint16_t pageremain = 256 - (Address % 256); 
    
    if (Size <= pageremain) {
        pageremain = Size; 
    }
    
    while (1) {
        W25Q01JV_WritePage(Address, pBuffer, pageremain);
        
        if (Size == pageremain) {
            break; 
        } else {
            pBuffer += pageremain;   
            Address += pageremain;   
            Size -= pageremain;      
            
            if (Size > 256) {
                pageremain = 256;
            } else {
                pageremain = Size;
            }
        }
    }
}