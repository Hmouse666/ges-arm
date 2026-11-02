#ifndef __WUWU_MPU6050_H_
#define __WUWU_MPU6050_H_

#include <stdint.h>

/*==========================================================================
 * 平台抽象 IO 接口
 * 移植到不同平台时，只需实现以下内容
 *==========================================================================*/
typedef struct {
    uint8_t i2c_address;
    void (*write_byte)(uint8_t dev_addr, uint8_t reg, uint8_t data);
    void (*write_buf) (uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len);
    void (*read_byte) (uint8_t dev_addr, uint8_t reg, uint8_t *data);
    void (*read_buf)  (uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len);
} MPU6050_IO;

/*==========================================================================
 * 数据结构
 *==========================================================================*/
typedef struct {
    float gyro_x, gyro_y, gyro_z;       /* 陀螺仪数据，单位：°/s */
    float acc_x,  acc_y,  acc_z;        /* 加速度计数据，单位：g(m/s^2) */
} MPU6050_Data;

/*==========================================================================
 * API 函数声明
 *==========================================================================*/


int MPU6050_Init(const MPU6050_IO *io);

void MPU6050_GetGyro(MPU6050_Data *data);
void MPU6050_GetAcc(MPU6050_Data *data);
void MPU6050_GetAccGyro(MPU6050_Data *data);

#endif




//#include "main.h"
//#include "tkkc_st7735.h"
//#include "mpu6050.h"

//void MPU6050_Read_Buf(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len)
//{
//    HAL_I2C_Mem_Read(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 10);
//}

//void MPU6050_Read_Byte(uint8_t dev_addr, uint8_t reg, uint8_t *data)
//{
//    HAL_I2C_Mem_Read(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 1, 10);
//}

//void MPU6050_Write_Buf(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len)
//{
//    HAL_I2C_Mem_Write(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 10);
//}

//void MPU6050_Write_Byte(uint8_t dev_addr, uint8_t reg, uint8_t data)
//{
//    HAL_I2C_Mem_Write(&hi2c2, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
//}

//MPU6050_IO mpu6050_io = {
//    .i2c_address = (0x68 << 1),
//    .read_buf = MPU6050_Read_Buf,
//    .read_byte = MPU6050_Read_Byte,
//    .write_buf = MPU6050_Write_Buf,
//    .write_byte = MPU6050_Write_Byte,
//};

//void app_main(void)
//{
//    lcd_init(LCD_DISPLAY_NORMAL, 0, 0);

//    MPU6050_Init(&mpu6050_io);

//    while(1)
//    {
//        MPU6050_Data mpu6050_data;
//        MPU6050_GetAccGyro(&mpu6050_data);

//        lcd_ShowString(0, 0 * 8, "acc_x:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowString(0, 1 * 8, "acc_y:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowString(0, 2 * 8, "acc_z:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(36, 0 * 8, mpu6050_data.acc_x, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(36, 1 * 8, mpu6050_data.acc_y, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(36, 2 * 8, mpu6050_data.acc_z, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);

//        lcd_ShowString(0, 4 * 8, "gyro_x:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowString(0, 5 * 8, "gyro_y:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowString(0, 6 * 8, "gyro_z:", 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(42, 4 * 8, mpu6050_data.gyro_x, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(42, 5 * 8, mpu6050_data.gyro_y, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//        lcd_ShowFloat(42, 6 * 8, mpu6050_data.gyro_z, 5, 2, 0x0000, 0xffff, LCD_FONT_SIZE_6x8);
//    }
//}
