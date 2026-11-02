#include "mpu6050.h"
#include <stdio.h>

/*==========================================================================
 * 寄存器定义
 *==========================================================================*/
#define MPU6050_SLAVE_ADDR                  (0x68)

#define MPU6050_WHO_AM_I                    (0x75)          // WHO AM I Register

#define MPU6050_REGISTER_SMPLRT_DIV         (0x19)          // SMPLRT_DIV
#define MPU6050_REGISTER_FSYNC_DLPF         (0x1A)          // external Frame Synchronization and Digital Low Pass Filter
#define MPU6050_REGISTER_GYRO_CONFIG        (0x1B)          // Gyro Config Register
#define MPU6050_REGISTER_ACC_CONFIG         (0x1C)          // Acc Config Register
#define MPU6050_REGISTER_PCC                (0x6B)          // Power Clock Config Register

#define MPU6050_REGISTER_GYRO_XOUT_H        (0x43)
#define MPU6050_REGISTER_ACC_XOUT_H         (0x3B)

/**
 * @brief   陀螺仪量程配置
 *
 * @param   MPU6050_GYRO_SAMPLE_250DPS      陀螺仪量程为±250 dps
 *                                          获取到的陀螺仪数据除以131.2 可以转化为带物理单位的数据，单位为：°/s
 * @param   MPU6050_GYRO_SAMPLE_500DPS      陀螺仪量程为±500 dps
 *                                          获取到的陀螺仪数据除以65.6  可以转化为带物理单位的数据，单位为：°/s
 * @param   MPU6050_GYRO_SAMPLE_1000DPS     陀螺仪量程为±1000 dps
 *                                          获取到的陀螺仪数据除以32.8  可以转化为带物理单位的数据，单位为：°/s
 * @param   MPU6050_GYRO_SAMPLE_2000DPS     陀螺仪量程为±2000 dps
 *                                          获取到的陀螺仪数据除以16.4  可以转化为带物理单位的数据，单位为：°/s
 */
typedef enum {
    MPU6050_GYRO_SAMPLE_250DPS      = 0x00,

    MPU6050_GYRO_SAMPLE_500DPS      = 0x08,

    MPU6050_GYRO_SAMPLE_1000DPS     = 0x10,

    MPU6050_GYRO_SAMPLE_2000DPS     = 0x18,
} MPU6050_GYRO_SAMPLE_CONFIG;

/**
 * @brief   加速度计量程配置
 *
 * @param   MPU6050_ACC_SAMPLE_2G           加速度计量程为±2 G
 *                                          获取到的陀螺仪数据除以16384 可以转化为带物理单位的数据，单位为：g≈9.80 m/s^2
 * @param   MPU6050_ACC_SAMPLE_4G           加速度计量程为±4 G
 *                                          获取到的陀螺仪数据除以8192  可以转化为带物理单位的数据，单位为：g≈9.80 m/s^2
 * @param   MPU6050_ACC_SAMPLE_8G           加速度计量程为±8 G
 *                                          获取到的陀螺仪数据除以4096  可以转化为带物理单位的数据，单位为：g≈9.80 m/s^2
 * @param   MPU6050_ACC_SAMPLE_16G          加速度计量程为±16 G
 *                                          获取到的陀螺仪数据除以2048  可以转化为带物理单位的数据，单位为：g≈9.80 m/s^2
 */
typedef enum {
    MPU6050_ACC_SAMPLE_2G           = 0x00,

    MPU6050_ACC_SAMPLE_4G           = 0x08,

    MPU6050_ACC_SAMPLE_8G           = 0x10,

    MPU6050_ACC_SAMPLE_16G          = 0x18,
} MPU6050_ACC_SAMPLE_CONFIG;

/* 内部静态变量 —— 保存平台 IO 接口 */
static const MPU6050_IO *pio = 0;

/* 内部静态变量 —— 量程配置 */
static MPU6050_GYRO_SAMPLE_CONFIG   gyro_config = MPU6050_GYRO_SAMPLE_1000DPS;
static MPU6050_ACC_SAMPLE_CONFIG    acc_config  = MPU6050_ACC_SAMPLE_8G;

/* 内部静态变量 —— 原始数据 */
static int16_t mpu6050_gyro_x = 0, mpu6050_gyro_y = 0, mpu6050_gyro_z = 0;
static int16_t mpu6050_acc_x  = 0, mpu6050_acc_y  = 0, mpu6050_acc_z  = 0;

/**
 * @brief 转换陀螺仪数据        单位：°/s
 */
static float mpu6050_gyro_transition(int16_t gyro_value)
{
    float gyro_data = 0;
    switch (gyro_config)
    {
        case MPU6050_GYRO_SAMPLE_250DPS:    gyro_data = (float)gyro_value / 131.0f; break;
        case MPU6050_GYRO_SAMPLE_500DPS:    gyro_data = (float)gyro_value / 65.5f;  break;
        case MPU6050_GYRO_SAMPLE_1000DPS:   gyro_data = (float)gyro_value / 32.8f;  break;
        case MPU6050_GYRO_SAMPLE_2000DPS:   gyro_data = (float)gyro_value / 16.4f;  break;
        default: break;
    }
    return gyro_data;
}

/**
 * @brief 转换加速度计数据      单位：g(m/s^2)
 */
static float mpu6050_acc_transition(int16_t acc_value)
{
    float acc_data = 0;
    switch (acc_config)
    {
        case MPU6050_ACC_SAMPLE_2G:     acc_data = (float)acc_value / 16384.0f; break;
        case MPU6050_ACC_SAMPLE_4G:     acc_data = (float)acc_value / 8192.0f;  break;
        case MPU6050_ACC_SAMPLE_8G:     acc_data = (float)acc_value / 4096.0f;  break;
        case MPU6050_ACC_SAMPLE_16G:    acc_data = (float)acc_value / 2048.0f;  break;
        default: break;
    }
    return acc_data;
}

/**
 * @brief       初始化驱动，绑定平台 IO 接口
 * 
 * @param       io              平台 IO 接口结构体指针
 * @retval      -1              MPU6050_IO 结构体错误
 * @retval      -2              WHO AM I读取失败
 * @retval      0               初始化成功
 */
int MPU6050_Init(const MPU6050_IO *io)
{
    uint8_t ret;
    if(io->read_buf == NULL || io->read_byte == NULL) {
        return -1;
    }
    if(io->write_buf == NULL || io->write_byte == NULL) {
        return -1;
    }
    if(io->i2c_address == 0) {
        return -1;
    }
    pio = io;
    pio->read_byte(pio->i2c_address, MPU6050_WHO_AM_I, &ret);
    if (ret != 0x68) {
        return -2;
    }

    // 详细请查看芯片手册
    pio->write_byte(pio->i2c_address, MPU6050_REGISTER_PCC, 0x00);
    pio->write_byte(pio->i2c_address, MPU6050_REGISTER_GYRO_CONFIG, gyro_config);
    pio->write_byte(pio->i2c_address, MPU6050_REGISTER_ACC_CONFIG, acc_config);
    pio->write_byte(pio->i2c_address, MPU6050_REGISTER_SMPLRT_DIV, 0x07);
    pio->write_byte(pio->i2c_address, MPU6050_REGISTER_FSYNC_DLPF, 0x02);
    return 0;
}

/**
 * @brief  读取MPU6050陀螺仪数据（内部函数）
 */
static void mpu6050_read_gyro(void)
{
    uint8_t dat[6];

    pio->read_buf(pio->i2c_address, MPU6050_REGISTER_GYRO_XOUT_H, dat, 6);
    mpu6050_gyro_x = (int16_t)(((int16_t)dat[0] << 8) | dat[1]);
    mpu6050_gyro_y = (int16_t)(((int16_t)dat[2] << 8) | dat[3]);
    mpu6050_gyro_z = (int16_t)(((int16_t)dat[4] << 8) | dat[5]);
}

/**
 * @brief  读取MPU6050加速度计数据（内部函数）
 */
static void mpu6050_read_acc(void)
{
    uint8_t dat[6];

    pio->read_buf(pio->i2c_address, MPU6050_REGISTER_ACC_XOUT_H, dat, 6);
    mpu6050_acc_x = (int16_t)(((int16_t)dat[0] << 8) | dat[1]);
    mpu6050_acc_y = (int16_t)(((int16_t)dat[2] << 8) | dat[3]);
    mpu6050_acc_z = (int16_t)(((int16_t)dat[4] << 8) | dat[5]);
}

/**
 * @brief  获取陀螺仪数据
 * 
 * @param  data 输出数据结构体指针（仅填充 gyro_* 字段）
 */
void MPU6050_GetGyro(MPU6050_Data *data)
{
    if (!data) return;

    mpu6050_read_gyro();

    data->gyro_x = mpu6050_gyro_transition(mpu6050_gyro_x);
    data->gyro_y = mpu6050_gyro_transition(mpu6050_gyro_y);
    data->gyro_z = mpu6050_gyro_transition(mpu6050_gyro_z);
}

/**
 * @brief  获取加速度计数据
 * 
 * @param  data 输出数据结构体指针（仅填充 acc_* 字段）
 */
void MPU6050_GetAcc(MPU6050_Data *data)
{
    if (!data) return;

    mpu6050_read_acc();

    data->acc_x = mpu6050_acc_transition(mpu6050_acc_x);
    data->acc_y = mpu6050_acc_transition(mpu6050_acc_y);
    data->acc_z = mpu6050_acc_transition(mpu6050_acc_z);
}

/**
 * @brief  获取陀螺仪与加速度计全部数据
 * 
 * @param  data 输出数据结构体指针
 */
void MPU6050_GetAccGyro(MPU6050_Data *data)
{
    if (!data) return;

    mpu6050_read_gyro();
    mpu6050_read_acc();

    data->gyro_x = mpu6050_gyro_transition(mpu6050_gyro_x);
    data->gyro_y = mpu6050_gyro_transition(mpu6050_gyro_y);
    data->gyro_z = mpu6050_gyro_transition(mpu6050_gyro_z);

    data->acc_x  = mpu6050_acc_transition(mpu6050_acc_x);
    data->acc_y  = mpu6050_acc_transition(mpu6050_acc_y);
    data->acc_z  = mpu6050_acc_transition(mpu6050_acc_z);
}
