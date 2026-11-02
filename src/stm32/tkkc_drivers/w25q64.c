#include "w25q64.h"

/*==========================================================================
 * W25Q64 规格参数
 *==========================================================================*/
#define W25Q64_PAGE_SIZE            256U            /* 每页 256 字节 */
#define W25Q64_SECTOR_SIZE          4096U           /* 每扇区 4KB */
#define W25Q64_BLOCK_32K_SIZE       32768U          /* 32KB 块 */
#define W25Q64_BLOCK_64K_SIZE       65536U          /* 64KB 块 */
#define W25Q64_TOTAL_SIZE           0x800000U       /* 8MB = 64Mbit */

#define W25Q64_MANUFACTURER_ID      0xEF
#define W25Q64_DEVICE_ID            0x4017

/*==========================================================================
 * 指令表
 *==========================================================================*/
#define W25Q64_CMD_WRITE_ENABLE     0x06
#define W25Q64_CMD_WRITE_DISABLE    0x04
#define W25Q64_CMD_READ_STATUS1     0x05
#define W25Q64_CMD_READ_STATUS2     0x35
#define W25Q64_CMD_READ_STATUS3     0x15
#define W25Q64_CMD_WRITE_STATUS     0x01
#define W25Q64_CMD_READ_DATA        0x03
#define W25Q64_CMD_FAST_READ        0x0B
#define W25Q64_CMD_PAGE_PROGRAM     0x02
#define W25Q64_CMD_SECTOR_ERASE     0x20
#define W25Q64_CMD_BLOCK_ERASE_32K  0x52
#define W25Q64_CMD_BLOCK_ERASE_64K  0xD8
#define W25Q64_CMD_CHIP_ERASE       0xC7  /* 或 0x60 */
#define W25Q64_CMD_POWER_DOWN       0xB9
#define W25Q64_CMD_RELEASE_POWERDOWN 0xAB
#define W25Q64_CMD_READ_JEDEC_ID    0x9F
#define W25Q64_CMD_READ_UID         0x4B
#define W25Q64_CMD_RESET_ENABLE     0x66
#define W25Q64_CMD_RESET            0x99

/*==========================================================================
 * 状态寄存器位定义
 *==========================================================================*/
#define W25Q64_SR1_BUSY             (1 << 0)  /* 忙标志 */
#define W25Q64_SR1_WEL              (1 << 1)  /* 写使能锁存 */

/* 内部静态变量 —— 保存平台 IO 接口 */
static const W25Q64_IO *pio = 0;

/* 内部辅助宏 */
#define ADDR24_H(addr)  (uint8_t)(((addr) >> 16) & 0xFF)
#define ADDR24_M(addr)  (uint8_t)(((addr) >> 8)  & 0xFF)
#define ADDR24_L(addr)  (uint8_t)((addr)         & 0xFF)

/**
  * @brief  初始化驱动，绑定平台 IO 接口
  * 
  * @param  io 平台 IO 接口结构体指针
  */
void W25Q64_Init(const W25Q64_IO *io)
{
    pio = io;
}

/**
  * @brief  发送写使能指令 (06h)
  */
static void W25Q64_WriteEnable(void)
{
    uint8_t cmd = W25Q64_CMD_WRITE_ENABLE;
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();
}

/**
  * @brief  读取状态寄存器
  * 
  * @param  reg 寄存器编号 (1/2/3)
  * 
  * @retval 状态寄存器值
  */
static uint8_t W25Q64_ReadStatus(uint8_t reg)
{
    uint8_t cmd, status = 0;
    switch (reg) {
        case 1: cmd = W25Q64_CMD_READ_STATUS1; break;
        case 2: cmd = W25Q64_CMD_READ_STATUS2; break;
        case 3: cmd = W25Q64_CMD_READ_STATUS3; break;
        default: return 0;
    }
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->spi_rx(&status, 1);
    pio->cs_high();
    return status;
}

/**
  * @brief  等待 Flash 空闲（轮询 SR1 BUSY 位）
  */
static void W25Q64_WaitBusy(void)
{
    while (W25Q64_ReadStatus(1) & W25Q64_SR1_BUSY);
}

/**
  * @brief  读取 JEDEC ID (9Fh)
  * 
  * @retval 32 位 ID，格式: (制造商ID << 16 | 设备ID)
  */
uint32_t W25Q64_ReadJEDEC_ID(void)
{
    uint8_t cmd = W25Q64_CMD_READ_JEDEC_ID;
    uint8_t buf[3] = {0};
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->spi_rx(buf, 3);
    pio->cs_high();
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

/**
  * @brief  标准读取数据 (03h)
  * 
  * @param  addr 24 位起始地址
  * @param  buf  读取数据缓冲区
  * @param  len  读取字节数
  */
void W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t cmd[4];
    cmd[0] = W25Q64_CMD_READ_DATA;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);

    pio->cs_low();
    pio->spi_tx(cmd, 4);
    pio->spi_rx(buf, (uint16_t)len);
    pio->cs_high();
}

/**
  * @brief  快速读取数据 (0Bh)，带 1 字节 dummy
  * 
  * @param  addr 24 位起始地址
  * @param  buf  读取数据缓冲区
  * @param  len  读取字节数
  * 
  * @note   SPI时钟高于50Mhz可以使用
  */
void W25Q64_FastRead(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t cmd[5];
    cmd[0] = W25Q64_CMD_FAST_READ;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);
    cmd[4] = 0x00; /* dummy byte */

    pio->cs_low();
    pio->spi_tx(cmd, 5);
    pio->spi_rx(buf, (uint16_t)len);
    pio->cs_high();
}

/**
  * @brief  页编程 (02h)，最大 256 字节，不跨页
  * 
  * @param  addr 24 位起始地址
  * @param  buf  写入数据缓冲区
  * @param  len  写入字节数 (0 ~ 256)
  */
static void W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    if (len == 0 || len > W25Q64_PAGE_SIZE) return;

    uint8_t cmd[4];
    cmd[0] = W25Q64_CMD_PAGE_PROGRAM;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);

    W25Q64_WriteEnable();
    pio->cs_low();
    pio->spi_tx(cmd, 4);
    pio->spi_tx(buf, len);
    pio->cs_high();
    W25Q64_WaitBusy();
}

/**
  * @brief  写入数据，自动处理跨页
  * 
  * @param  addr 24 位起始地址
  * @param  buf  写入数据缓冲区
  * @param  len  写入字节数
  */
void W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t remain = len;
    uint32_t offset = 0;
    while (remain > 0) {
        uint16_t space = (uint16_t)(W25Q64_PAGE_SIZE - (addr % W25Q64_PAGE_SIZE));
        uint16_t chunk = (remain < space) ? (uint16_t)remain : space;
        W25Q64_PageProgram(addr + offset, buf + offset, chunk);
        offset += chunk;
        remain -= chunk;
    }
}

/**
  * @brief  扇区擦除 (20h)，4KB
  * 
  * @param  addr 扇区内任意 24 位地址
  */
void W25Q64_SectorErase(uint32_t addr)
{
    uint8_t cmd[4];
    cmd[0] = W25Q64_CMD_SECTOR_ERASE;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);

    W25Q64_WriteEnable();
    pio->cs_low();
    pio->spi_tx(cmd, 4);
    pio->cs_high();
    W25Q64_WaitBusy();
}

/**
  * @brief  块擦除 32KB (52h)
  * 
  * @param  addr 块内任意 24 位地址
  */
void W25Q64_BlockErase_32K(uint32_t addr)
{
    uint8_t cmd[4];
    cmd[0] = W25Q64_CMD_BLOCK_ERASE_32K;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);

    W25Q64_WriteEnable();
    pio->cs_low();
    pio->spi_tx(cmd, 4);
    pio->cs_high();
    W25Q64_WaitBusy();
}

/**
  * @brief  块擦除 64KB (D8h)
  * 
  * @param  addr 块内任意 24 位地址
  */
void W25Q64_BlockErase_64K(uint32_t addr)
{
    uint8_t cmd[4];
    cmd[0] = W25Q64_CMD_BLOCK_ERASE_64K;
    cmd[1] = ADDR24_H(addr);
    cmd[2] = ADDR24_M(addr);
    cmd[3] = ADDR24_L(addr);

    W25Q64_WriteEnable();
    pio->cs_low();
    pio->spi_tx(cmd, 4);
    pio->cs_high();
    W25Q64_WaitBusy();
}

/**
  * @brief  全片擦除 (C7h)，耗时较长
  */
void W25Q64_ChipErase(void)
{
    uint8_t cmd = W25Q64_CMD_CHIP_ERASE;

    W25Q64_WriteEnable();
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();
    W25Q64_WaitBusy();
}

/**
  * @brief  读取 64 位唯一 ID (4Bh)
  * 
  * @param  uid 输出缓冲区，至少 8 字节
  */
void W25Q64_ReadUID(uint8_t *uid)
{
    uint8_t cmd[5];
    cmd[0] = W25Q64_CMD_READ_UID;
    cmd[1] = 0x00;
    cmd[2] = 0x00;
    cmd[3] = 0x00;
    cmd[4] = 0x00;

    pio->cs_low();
    pio->spi_tx(cmd, 5);
    pio->spi_rx(uid, 8);
    pio->cs_high();
}

/**
  * @brief  进入掉电模式 (B9h)
  */
void W25Q64_PowerDown(void)
{
    uint8_t cmd = W25Q64_CMD_POWER_DOWN;
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();
    pio->delay_ms(5);
}

/**
  * @brief  从掉电模式唤醒 (ABh)
  */
void W25Q64_WakeUp(void)
{
    uint8_t cmd = W25Q64_CMD_RELEASE_POWERDOWN;
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();
    pio->delay_ms(5);
}

/**
  * @brief  软件复位 (66h + 99h)
  */
void W25Q64_Reset(void)
{
    uint8_t cmd;
    cmd = W25Q64_CMD_RESET_ENABLE;
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();

    cmd = W25Q64_CMD_RESET;
    pio->cs_low();
    pio->spi_tx(&cmd, 1);
    pio->cs_high();

    pio->delay_ms(30);
}
