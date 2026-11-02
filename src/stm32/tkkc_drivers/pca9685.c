#include "pca9685.h"

/* ==========================================================================
 * PCA9685 驱动实现(软件 I2C: PA6 = SCL, PA7 = SDA, 引脚在 pca9685.h 里改)
 *
 * 为什么用软件 I2C:
 *   硬件 I2C2 的脚(PB10/PB11)现在给 FPGA 的手势数据当串口了(USART3), 所以舵机
 *   模块改到 PA6/PA7 上, 用普通 IO 一位一位模拟 I2C。慢一点(几十 kHz), 但写几个
 *   寄存器、给舵机发发脉宽足够用了, 也不会拖慢主循环多少。
 *
 * I2C 是怎么"模拟"出来的(两根线都是开漏输出):
 *   1) 平时两根线都被上拉电阻拉成高电平, 谁都可以把线拉低, 但没人会把线"推高"
 *      —— 所以引脚配成 GPIO_MODE_OUTPUT_OD(开漏): 写 1 = 松手(靠上拉拉高),
 *      写 0 = 把线拉低;
 *   2) 起始: SCL 高的时候, SDA 由高变低;
 *      停止: SCL 高的时候, SDA 由低变高;
 *   3) 每个字节 8 位(先发高位), 第 9 个时钟从机拉低 SDA 表示"收到"(应答);
 *   4) 读数据时把 SDA 切成输入(GPIO_MODE_INPUT + 上拉), 让从机拉线。
 *
 * 对外就是 4 个函数(见 pca9685.h), 和以前用硬件 I2C 时的用法完全一样,
 * 上层 servo.c / APP.c 一个字都不用改。
 * ========================================================================== */

/* ---- 两根线的操作(开漏输出: 写 1 = 松手, 写 0 = 拉低) ---- */
#define SCL_H()     HAL_GPIO_WritePin(PCA9685_SCL_PORT, PCA9685_SCL_PIN, GPIO_PIN_SET)
#define SCL_L()     HAL_GPIO_WritePin(PCA9685_SCL_PORT, PCA9685_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H()     HAL_GPIO_WritePin(PCA9685_SDA_PORT, PCA9685_SDA_PIN, GPIO_PIN_SET)
#define SDA_L()     HAL_GPIO_WritePin(PCA9685_SDA_PORT, PCA9685_SDA_PIN, GPIO_PIN_RESET)

/**
  * @brief  I2C 的"慢一点": 每个电平保持一小会儿
  * @note   空循环 PCA9685_I2C_DELAY 次(在 pca9685.h 里改, 越大越慢越稳)
  */
static void I2C_Delay(void)
{
    volatile uint8_t i;

    for (i = 0; i < PCA9685_I2C_DELAY; i++)
    {
    }
}

/**
  * @brief  SDA 切成"输出"(开漏): 我们说话, 可以把线拉低
  */
static void SDA_Out(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin   = PCA9685_SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;      /* 开漏: 写 1 只是松手, 不会推高 */
    g.Pull  = GPIO_PULLUP;              /* 内部上拉(模块上一般也有上拉, 双保险) */
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(PCA9685_SDA_PORT, &g);
}

/**
  * @brief  SDA 切成"输入"(带上拉): 让从机(模块)去拉这根线
  */
static void SDA_In(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin  = PCA9685_SDA_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(PCA9685_SDA_PORT, &g);
}

/* 读 SDA 现在是什么电平: 1 = 高, 0 = 低 */
static uint8_t SDA_Level(void)
{
    return (HAL_GPIO_ReadPin(PCA9685_SDA_PORT, PCA9685_SDA_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/**
  * @brief  把 SCL/SDA 两个脚配好(开漏输出 + 上拉), 并让总线空闲(两根线都是高)
  */
static void I2C_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();               /* PA6/PA7 在 GPIOA; 换个口的话这里也要跟着改 */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = PCA9685_SCL_PIN;
    HAL_GPIO_Init(PCA9685_SCL_PORT, &g);

    g.Pin = PCA9685_SDA_PIN;
    HAL_GPIO_Init(PCA9685_SDA_PORT, &g);

    SCL_H();                                    /* 总线空闲: SCL、SDA 都是高 */
    SDA_H();
    I2C_Delay();
}

/* 起始条件: SCL 高的时候, SDA 由高变低 */
static void I2C_Start(void)
{
    SDA_Out();
    SDA_H();
    SCL_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

/* 停止条件: SCL 高的时候, SDA 由低变高 */
static void I2C_Stop(void)
{
    SDA_Out();
    SDA_L();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
}

/**
  * @brief  发 1 个字节, 再把从机的应答读回来
  * @retval 1 = 从机应答了(ACK); 0 = 没应答(NACK, 说明模块没接好 / 地址不对)
  */
static uint8_t I2C_WriteByte(uint8_t b)
{
    uint8_t i;
    uint8_t ack;

    SDA_Out();

    for (i = 0; i < 8; i++)                     /* 8 位, 先发最高位 */
    {
        if (b & 0x80)
        {
            SDA_H();
        }
        else
        {
            SDA_L();
        }
        b <<= 1;

        I2C_Delay();
        SCL_H();                                /* 时钟高电平期间, 数据必须稳定 */
        I2C_Delay();
        SCL_L();
        I2C_Delay();
    }

    /* 第 9 个时钟: 我们松手, 让从机把 SDA 拉低表示"收到" */
    SDA_H();
    SDA_In();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    ack = (SDA_Level() == 0) ? 1 : 0;           /* 被拉低了就是应答 */
    SCL_L();
    I2C_Delay();
    SDA_Out();
    SDA_H();
    I2C_Delay();

    return ack;
}

/**
  * @brief  读 1 个字节
  * @param  ack : 读到之后给不给应答(1 = 还要继续读, 0 = 读完了)
  */
static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t b = 0;

    SDA_H();
    SDA_In();                                   /* 松手, 让从机拉线 */

    for (i = 0; i < 8; i++)
    {
        b <<= 1;
        I2C_Delay();
        SCL_H();
        I2C_Delay();
        if (SDA_Level() != 0)
        {
            b |= 1;
        }
        SCL_L();
        I2C_Delay();
    }

    SDA_Out();                                  /* 换回我们说话: 给应答 */
    if (ack != 0)
    {
        SDA_L();                                /* 应答: 拉低 */
    }
    else
    {
        SDA_H();                                /* 不应答: 松手 */
    }
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
    SDA_H();
    I2C_Delay();

    return b;
}

/**
  * @brief   往模块的某个寄存器开始写 len 个字节
  * @retval  0 = 成功, 1 = 模块没应答
  */
static uint8_t I2C_WriteRegs(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    uint8_t i;

    I2C_Start();

    if (I2C_WriteByte((uint8_t)(PCA9685_ADDR8 | 0x00)) == 0)    /* 地址 + 写 */
    {
        I2C_Stop();
        return 1;
    }
    if (I2C_WriteByte(reg) == 0)                                /* 寄存器地址 */
    {
        I2C_Stop();
        return 1;
    }
    for (i = 0; i < len; i++)                                   /* 数据(芯片会自动往后写) */
    {
        if (I2C_WriteByte(buf[i]) == 0)
        {
            I2C_Stop();
            return 1;
        }
    }

    I2C_Stop();
    return 0;
}

/**
  * @brief   从模块的某个寄存器开始读 len 个字节
  * @retval  0 = 成功, 1 = 模块没应答
  */
static uint8_t I2C_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    I2C_Start();

    if (I2C_WriteByte((uint8_t)(PCA9685_ADDR8 | 0x00)) == 0)    /* 地址 + 写(先告诉它读哪个寄存器) */
    {
        I2C_Stop();
        return 1;
    }
    if (I2C_WriteByte(reg) == 0)
    {
        I2C_Stop();
        return 1;
    }

    I2C_Start();                                                /* 重新起始 */

    if (I2C_WriteByte((uint8_t)(PCA9685_ADDR8 | 0x01)) == 0)    /* 地址 + 读 */
    {
        I2C_Stop();
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        buf[i] = I2C_ReadByte((uint8_t)(((i + 1) < len) ? 1 : 0));   /* 最后一个字节不应答 */
    }

    I2C_Stop();
    return 0;
}

/* ---------------------- 往一个寄存器写一个字节 ---------------------- */
static void WriteReg(uint8_t reg, uint8_t data)
{
    I2C_WriteRegs(reg, &data, 1);
}

/* ---------------------- 从寄存器读一个字节 ---------------------- */
static uint8_t ReadReg(uint8_t reg)
{
    uint8_t data = 0;

    I2C_ReadRegs(reg, &data, 1);

    return data;
}

/* ============================== 初始化 ============================== */

/**
  * @brief  PCA9685 初始化
  * @retval 0 = 成功, 1 = 模块没应答(查 PA6/PA7 接线、模块供电、地址跳线)
  * @note   流程: 配好两个脚 -> 试试模块在不在 -> 复位 -> 设成推挽输出 -> 50Hz -> 全部关闭
  */
uint8_t PCA9685_Init(void)
{
    I2C_Init();                                 /* 先把 PA6/PA7 配成软件 I2C 的两个脚 */

    /* 检查模块有没有应答: 有应答说明 SDA/SCL 和供电都正常 */
    I2C_Start();
    if (I2C_WriteByte((uint8_t)(PCA9685_ADDR8 | 0x00)) == 0)
    {
        I2C_Stop();
        return 1;                               /* 没人应答: 屏幕上会显示 PCA9685 ERR */
    }
    I2C_Stop();

    WriteReg(PCA9685_MODE1, 0x00);              /* 复位: 退出休眠 */
    HAL_Delay(1);

    WriteReg(PCA9685_MODE2, 0x04);              /* 0x04 = 推挽输出, 驱动舵机用这个 */

    PCA9685_SetFreq(50);                        /* 舵机用 50Hz */
    PCA9685_AllOff();                           /* 16 路先全部关掉, 防止舵机乱动 */

    return 0;
}

/* ============================== 设置频率 ============================== */

/**
  * @brief  设置 PWM 频率
  * @param  freq : 频率(Hz), 模拟舵机用 50
  * @note   手册公式: 分频值 = 25000000 / (4096 × 频率) - 1
  *         50Hz 算出来是 121。
  *         注意: 改分频值之前必须先让芯片休眠, 改完再唤醒。
  */
void PCA9685_SetFreq(uint16_t freq)
{
    uint8_t old_mode;
    uint8_t prescale;
    float   value;

    if (freq < 24)
    {
        freq = 24;                      /* 芯片能出的最低频率 */
    }

    /* 分频值 = 25MHz / (4096 × 频率) - 1, 加 0.5 再取整就是四舍五入 */
    value = 25000000.0f / 4096.0f / freq;
    value = value - 1.0f;
    prescale = (uint8_t)(value + 0.5f);

    if (prescale < 3)
    {
        prescale = 3;                   /* 手册要求最小是 3 */
    }

    old_mode = ReadReg(PCA9685_MODE1);

    WriteReg(PCA9685_MODE1, (uint8_t)((old_mode & 0x7F) | 0x10));   /* 0x10 = 休眠 */
    WriteReg(PCA9685_PRESCALE, prescale);                           /* 写分频值 */
    WriteReg(PCA9685_MODE1, (uint8_t)(old_mode | 0x20));            /* 0x20 = 地址自增 + 唤醒 */

    HAL_Delay(5);                                                   /* 等振荡器稳定 */

    WriteReg(PCA9685_MODE1, (uint8_t)(old_mode | 0x20 | 0x80));     /* 0x80 = 重启 PWM 输出 */
}

/* ============================== 设置输出 ============================== */

/**
  * @brief  设置某一路的 PWM
  * @param  ch  : 通道号 0~15(对应模块上的 PWM0~PWM15)
  * @param  off : 0~4095 是占空比; 4096 表示这一路一直输出低电平(不发脉冲)
  * @note   PCA9685 每一路用 4 个寄存器: ON_L、ON_H、OFF_L、OFF_H
  *         我们固定从 0 开始计数, 所以两个 ON 寄存器写 0 就行
  */
void PCA9685_SetPWM(uint8_t ch, uint16_t off)
{
    uint8_t buf[4];

    if (ch > 15)
    {
        return;                         /* 只有 0~15 这 16 路 */
    }

    buf[0] = 0;                         /* ON_L  : 第 0 个计数就打开 */
    buf[1] = 0;                         /* ON_H  */
    buf[2] = (uint8_t)(off & 0xFF);     /* OFF_L */
    buf[3] = (uint8_t)(off >> 8);       /* OFF_H : 高字节 bit4 = 1 表示恒低 */

    /* 一次写 4 个字节到这一路的起始寄存器(芯片会自动往后写) */
    I2C_WriteRegs((uint8_t)(PCA9685_LED0_ON_L + 4 * ch), buf, 4);
}

/**
  * @brief  16 路全部关闭(舵机不再受力)
  */
void PCA9685_AllOff(void)
{
    uint8_t buf[4];

    buf[0] = 0;                         /* ON_L  */
    buf[1] = 0;                         /* ON_H  */
    buf[2] = 0;                         /* OFF_L */
    buf[3] = 0x10;                      /* OFF_H 的 bit4 = 1 -> 全部恒低 */

    I2C_WriteRegs(PCA9685_ALL_LED_ON_L, buf, 4);
}
