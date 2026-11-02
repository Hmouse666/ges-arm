#ifndef __W25Q64_H
#define __W25Q64_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*==========================================================================
 * 平台抽象 IO 接口
 * 移植到不同平台时，只需实现以下 5 个函数
 *==========================================================================*/

typedef struct {
    void (*cs_low)(void);                               /* CS 拉低 */
    void (*cs_high)(void);                              /* CS 拉高 */
    void (*spi_tx)(const uint8_t *data, uint16_t len);  /* SPI 发送 */
    void (*spi_rx)(uint8_t *data, uint16_t len);        /* SPI 接收 */
    void (*delay_ms)(uint32_t ms);                      /* 毫秒延时 */
} W25Q64_IO;

/*==========================================================================
 * API 函数声明
 *==========================================================================*/

void W25Q64_Init(const W25Q64_IO *io);
uint32_t W25Q64_ReadJEDEC_ID(void);

void W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void W25Q64_FastRead(uint32_t addr, uint8_t *buf, uint32_t len);

void W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

void W25Q64_SectorErase(uint32_t addr);
void W25Q64_BlockErase_32K(uint32_t addr);
void W25Q64_BlockErase_64K(uint32_t addr);
void W25Q64_ChipErase(void);

void W25Q64_ReadUID(uint8_t *uid);   /* uid 需要至少 8 字节 */
void W25Q64_PowerDown(void);
void W25Q64_WakeUp(void);
void W25Q64_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q64_H */
