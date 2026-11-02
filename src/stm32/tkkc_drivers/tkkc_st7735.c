#include "tkkc_st7735.h"
#include "stm32f1xx_ll_spi.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_bus.h"
#include <string.h>

/* ==========================================================================
 * ★ 屏幕引脚(按你的要求改成这一套): 数据线走 SPI2, 另外三个脚是普通 IO
 *
 *      SCL(PB13) -> SPI2_SCK      SDA(PB15) -> SPI2_MOSI
 *      CS  = PA11                 DC  = PB14                 RES = PA8
 *
 *   说明:
 *     a) SCL/SDA 是 SPI2 的复用脚, 由 MX_SPI2_Init() + 下面 lcd_hardware_init() 配置;
 *     b) CS / DC / RES 是"普通 IO 输出", 直接用 LL 库拉高拉低;
 *     c) DC 用的 PB14 原来是 SPI2_MISO(读回数据的脚), 屏幕只管发不管读, 拿来做 DC
 *        没有任何问题 —— 下面 lcd_hardware_init() 里会把它配成普通输出;
 *     d) RES 用的 PA8 原来是按键 KEY_4, 已经让给屏幕了(key.c 里删掉了这个按键);
 *     e) 原来那套脚(PA3=CS / PA4=DC / PA5=SCL / PA6=RES / PA7=SDA)现在全都空出来了
 *        (PA6/PA7 让给舵机模块 PCA9685 的软件 I2C 用了, 见 pca9685.h)。
 *
 *   ★ 要换脚: 只改下面这 7 行(端口 + 引脚), 别的地方一个字都不用动。
 * ========================================================================== */
#define LCD_SPI                     (SPI2)
#define LCD_CS_Port                 (GPIOA)
#define LCD_CS_Pin                  (LL_GPIO_PIN_11)
#define LCD_DC_Port                 (GPIOB)
#define LCD_DC_Pin                  (LL_GPIO_PIN_14)
#define LCD_RES_Port                (GPIOA)
#define LCD_RES_Pin                 (LL_GPIO_PIN_8)

#define Delay_ms(x)               	(HAL_Delay(x))
#define LCD_RES(x)                  ((x) ?  LL_GPIO_SetOutputPin(LCD_RES_Port, LCD_RES_Pin) : LL_GPIO_ResetOutputPin(LCD_RES_Port, LCD_RES_Pin))
#define LCD_DC(x)                   ((x) ?  LL_GPIO_SetOutputPin(LCD_DC_Port,  LCD_DC_Pin)  : LL_GPIO_ResetOutputPin(LCD_DC_Port,  LCD_DC_Pin))
#define LCD_CS(x)                   ((x) ?  LL_GPIO_SetOutputPin(LCD_CS_Port,  LCD_CS_Pin)  : LL_GPIO_ResetOutputPin(LCD_CS_Port,  LCD_CS_Pin))

/**
 * @brief       屏幕内部参数结构体
 */
typedef struct LCD_DataStruct {

    enum LCD_DISPLAY_DIR dir;           //屏幕方向

    uint8_t x_offset;                   //屏幕显示x方向上偏移

    uint8_t y_offset;                   //屏幕显示y方向上偏移

    uint8_t wide;                       //屏幕宽

    uint8_t high;                       //屏幕高

} LCD_DataStruct;

static LCD_DataStruct lcd_data;

/**
 * @brief       LCD发送1字节
 * 
 * @param       data        1字节数据
 * 
 * @note        内部调用
 */
inline static void lcd_SendByte(uint8_t data)
{
    LL_SPI_TransmitData8(LCD_SPI, data);
    while(!LL_SPI_IsActiveFlag_TXE(LCD_SPI));
    while(LL_SPI_IsActiveFlag_BSY(LCD_SPI));
} 

/**
 * @brief       LCD发送2字节
 * 
 * @param       data        2字节数据
 * 
 * @note        内部调用
 */
inline static void lcd_Send2Byte(uint16_t data)
{
    LL_SPI_TransmitData8(LCD_SPI, (uint8_t)((data & 0xFF00) >> 8));
    while(!LL_SPI_IsActiveFlag_TXE(LCD_SPI));
    LL_SPI_TransmitData8(LCD_SPI, (uint8_t)(data & 0x00FF));
    while(!LL_SPI_IsActiveFlag_TXE(LCD_SPI));
    while(LL_SPI_IsActiveFlag_BSY(LCD_SPI));
}

/**
 * @brief       向LCD写命令
 * 
 * @param       cmd         命令
 * 
 * @note        内部调用
 */
inline static void lcd_SendCmd(const uint8_t cmd)
{
    LCD_DC(0);
    lcd_SendByte(cmd);
    LCD_DC(1);
}

/**
 * @brief       设置显示区域
 * 
 * @param       x1          起始x坐标
 * @param       y1          起始y坐标
 * @param       x2          结束x坐标
 * @param       y2          结束y坐标
 */
static void lcd_SetDisplayArea (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	if(lcd_data.dir == LCD_DISPLAY_NORMAL || lcd_data.dir == LCD_DISPLAY_RORARE_180) {
		lcd_SendCmd(0x2a);
		lcd_SendByte(0x00);
		lcd_SendByte(x1 + lcd_data.x_offset);
		lcd_SendByte(0x00);
		lcd_SendByte(x2 + lcd_data.x_offset);

		lcd_SendCmd(0x2b);
		lcd_SendByte(0x00);
		lcd_SendByte(y1 + lcd_data.y_offset);
		lcd_SendByte(0x00);
		lcd_SendByte(y2 + lcd_data.y_offset);
	} else {
		lcd_SendCmd(0x2a);
		lcd_SendByte(0x00);
		lcd_SendByte(x1 + lcd_data.y_offset);
		lcd_SendByte(0x0);
		lcd_SendByte(x2 + lcd_data.y_offset);

		lcd_SendCmd(0x2b);
		lcd_SendByte(0x00);
		lcd_SendByte(y1 + lcd_data.x_offset);
		lcd_SendByte(0x00);
		lcd_SendByte(y2 + lcd_data.x_offset);
	}
	lcd_SendCmd(0x2c);
}

/**
 * @brief       全屏填充颜色
 * 
 * @param       color       填充颜色(RGB565)
 * 
 */
void lcd_FullColor(const uint16_t color)
{
    LCD_CS(0);
    lcd_SetDisplayArea(0, 0, lcd_data.wide - 1, lcd_data.high - 1);
    uint16_t i = lcd_data.wide * lcd_data.high;
    while(i--) {
        lcd_Send2Byte(color);
    }
    LCD_CS(1);
}

/**
 * @brief       显示字符
 * 
 * @param       x               显示起始x
 * @param       y               显示起始y
 * @param       ch              显示字符
 * @param       ft_color        字体颜色
 * @param       bg_color        背景颜色
 * @param       size            字体尺寸
 */
void lcd_ShowChar(uint8_t x, uint8_t y, const char ch, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size)
{
    LCD_CS(0);
    switch(size)
    {
        case LCD_FONT_SIZE_8x16:
        lcd_SetDisplayArea(x, y, x + 7, y + 15);
        for(int i = 0; i < 8; i++) {
            for(int j = 0; j < 8; j++) {
                uint8_t temp = ASCII_FONT_8x16[ch-32][j];
                if(temp & (0x01 << i)) {
                    lcd_Send2Byte(ft_color);
                } else {
                    lcd_Send2Byte(bg_color);
                }
                temp >>= 1;
            }
        }
        for(int i = 0; i < 8; i++) {
            for(int j = 8; j < 16; j++) {
                uint8_t temp = ASCII_FONT_8x16[ch-32][j];
                if(temp & (0x01 << i)) {
                    lcd_Send2Byte(ft_color);
                } else {
                    lcd_Send2Byte(bg_color);
                }
                temp >>= 1;
            }
        }
        break;

        case LCD_FONT_SIZE_6x8:
        lcd_SetDisplayArea(x, y, x + 5, y + 7);
        for(int i = 0; i < 8; i++) {
            for(int j = 0; j < 6; j++) {
                uint8_t temp = ASCII_FONT_6x8[ch - 32][j];
                if(temp & (0x01 << i)) {
                    lcd_Send2Byte(ft_color);
                } else {
                    lcd_Send2Byte(bg_color);
                }
                temp >>= 1;
            }
        }
        break;
        default: break;
    }
    LCD_CS(1);
}

/**
 * @brief       显示字符串
 * 
 * @param       x               显示起始x
 * @param       y               显示起始y
 * @param       ch              显示字符串
 * @param       ft_color        字体颜色
 * @param       bg_color        背景颜色
 * @param       size            字体尺寸
 */
void lcd_ShowString(uint8_t x, uint8_t y, const char ch[], uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size)
{
    uint16_t i = 0;
    while(ch[i] != '\0') 
    {
        switch(size)
        {
            case LCD_FONT_SIZE_8x16:
                lcd_ShowChar(x + 8 * i, y, ch[i], ft_color, bg_color, size);
            break;

            case LCD_FONT_SIZE_6x8:
                lcd_ShowChar(x + 6 * i, y, ch[i], ft_color, bg_color, size);
            break;

            default: break;
        }
        i++;
    }
}

/**
 * @brief       显示整数
 * 
 * @param       x               显示起始x
 * @param       y               显示起始y
 * @param       num             显示整数
 * @param       len             整数长度范围1~10
 * @param       ft_color        字体颜色
 * @param       bg_color        背景颜色
 * @param       size            字体尺寸
 * 
 */
void lcd_ShowInt(uint8_t x, uint8_t y, int num, uint8_t len, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size)
{
    static char number[11];
    int num_copy;
    uint32_t ten;
    int8_t i, num_len, minus;

    if(len < 1 || len > 10) return;

    memset(number, ' ', len);
    number[len] = '\0';

    minus = 1;
    if(num < 0) {
        minus = -1;
        num = -num;
    }

    if(num == 0) {
        num_len = 1;
    } else {
        num_len = 0;
        num_copy = num;
        while(num_copy != 0) {
            num_copy /= 10;
            num_len++;
        }
    }

    for(i = 0, ten = 1; i < num_len && i < len; i++) {
        if(i != 0)ten *= 10;
        number[len - 1 - i] = (num / ten % 10) + '0';
    }

    if(minus == -1) {
        if(num_len < len) {
            number[len - num_len - 1] = '-';
        } else {
            number[0] = '-';  // 覆盖最高位
        }
    }

    lcd_ShowString(x, y, number, ft_color, bg_color, size);
}

/**
 * @brief       显示浮点数
 * 
 * @param       x               显示起始x
 * @param       y               显示起始y
 * @param       num             显示浮点数
 * @param       integer_len     浮点数整数部分长度(1~10)
 * @param       fractional_len  浮点数小数部分长度(0~6)
 * @param       ft_color        字体颜色
 * @param       bg_color        背景颜色
 * @param       size            字体尺寸
 * 
 */
void lcd_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t integer_len, uint8_t fractional_len, uint16_t ft_color, uint16_t bg_color, LCD_FONT_SIZE size)
{
    static char str[20];
    int int_part, frac_part, temp;
    uint8_t minus = 0;
    uint8_t int_len_actual;
    int i;
    uint32_t ten;
    
    if(integer_len < 1 || integer_len > 10) return;
    if(fractional_len > 6) return;
    
    memset(str, ' ', sizeof(str));
    
    // 处理负数
    if(num < 0) {
        minus = 1;
        num = -num;
    }
    
    int_part = (int)num;
    
    for(i = 0, temp = 1; i < fractional_len; i++) temp *= 10;
    frac_part = (int)((num - int_part) * temp + 0.5);  // +0.5 实现四舍五入
    
    if(frac_part >= temp) {
        frac_part = 0;
        int_part++;
    }
    
    if(int_part == 0) {
        int_len_actual = 1;
    } else {
        int_len_actual = 0;
        temp = int_part;
        while(temp != 0) {
            temp /= 10;
            int_len_actual++;
        }
    }
    
    if(minus && int_len_actual + 1 > integer_len) {
        int_len_actual = integer_len - 1;
        for(i = 0, ten = 1; i < int_len_actual; i++) ten *= 10;
        int_part %= ten;
    }
    
    temp = int_part;
    for(i = 0; i < int_len_actual; i++) {
        str[integer_len - 1 - i] = (temp % 10) + '0';
        temp /= 10;
    }
    
    for(i = 0; i < integer_len - int_len_actual; i++) {
        str[i] = ' ';
    }
    
    if(minus) {
        if(integer_len > int_len_actual) {
            str[integer_len - int_len_actual - 1] = '-';
        } else {
            str[0] = '-';  // 覆盖最高位
        }
    }
    
    str[integer_len] = '.';
    
    for(i = 0; i < fractional_len; i++) {
        ten = 1;
        for(int j = 0; j < fractional_len - i - 1; j++) ten *= 10;
        str[integer_len + 1 + i] = (frac_part / ten % 10) + '0';
    }
    
    str[integer_len + 1 + fractional_len] = '\0';
    
    lcd_ShowString(x, y, str, ft_color, bg_color, size);
}

/**
 * @brief       LCD硬件初始化
 * 
 * @note        内部调用函数, 屏幕的引脚在这里一次配好(不用再在 CubeMX 里配)
 *              LCD Configuration
 *              PB13    ------> LCD_SCL  (SPI2_SCK)
 *              PB15    ------> LCD_SDA  (SPI2_MOSI)
 *              PA11    ------> LCD_CS
 *              PB14    ------> LCD_DC
 *              PA8     ------> LCD_RES
 */
static void lcd_hardware_init(void)
{
    LL_SPI_InitTypeDef SPI_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);      /* ★ SPI2 挂在 APB1 上(SPI1 才在 APB2) */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOB);

    /* 三个控制脚先输出高电平(片选不选中、数据模式), 免得初始化过程中屏幕收到垃圾 */
    LL_GPIO_SetOutputPin(LCD_CS_Port,  LCD_CS_Pin);
    LL_GPIO_SetOutputPin(LCD_DC_Port,  LCD_DC_Pin);
    LL_GPIO_SetOutputPin(LCD_RES_Port, LCD_RES_Pin);

    /* SPI2 的复用脚: PB13 = SCK, PB15 = MOSI */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_13|LL_GPIO_PIN_15;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CS(PA11) / DC(PB14) / RES(PA8): 普通推挽输出
     * ★ PB14 原来是 SPI2_MISO(由 MX_SPI2_Init 配成输入), 这里重新配成输出, 谁后配谁说了算,
     *   所以屏幕初始化之后 PB14 就是 DC 脚了。 */
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

    GPIO_InitStruct.Pin = LCD_CS_Pin;
    LL_GPIO_Init(LCD_CS_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_DC_Pin;
    LL_GPIO_Init(LCD_DC_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_RES_Pin;
    LL_GPIO_Init(LCD_RES_Port, &GPIO_InitStruct);

    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
    SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
    SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
    /* SPI2 挂在 APB1(36MHz) 上, DIV4 = 9MHz。杜邦线接的模块用 9MHz 更稳;
     * 屏幕本来就接得很短/画东西嫌慢, 可以改成 DIV2(18MHz) 试试。 */
    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV4;
    SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly = 10;
    LL_SPI_Init(LCD_SPI, &SPI_InitStruct);
    LL_SPI_Enable(LCD_SPI);
}

/**
 * @brief       LCD屏幕初始化
 * 
 * @param       dir         屏幕初始化方向(定义在枚举LCD_DISPLAY_DIR中)
 * @param       x_offset    屏幕显示偏移x
 * @param       y_offset    屏幕显示偏移y
 */
void lcd_init(enum LCD_DISPLAY_DIR dir, uint8_t x_offset, uint8_t y_offset)
{
    lcd_hardware_init();

    lcd_data.dir = ((int)dir >= LCD_DISPLAY_NORMAL && (int)dir <= LCD_DISPLAY_RORARE_270) ? dir : LCD_DISPLAY_NORMAL;
    if(lcd_data.dir == LCD_DISPLAY_NORMAL || lcd_data.dir == LCD_DISPLAY_RORARE_180)
    {
        lcd_data.wide = ST7735_LCD_WIDE;
        lcd_data.high = ST7735_LCD_HIGH;
    }
    else
    {
        lcd_data.wide = ST7735_LCD_HIGH;
        lcd_data.high = ST7735_LCD_WIDE;
    }
    lcd_data.x_offset = x_offset;
    lcd_data.y_offset = y_offset;

    LCD_RES(0);
    Delay_ms(10);

    LCD_RES(1);
    Delay_ms(120);
    LCD_CS(0);

    lcd_SendCmd(0x11); Delay_ms(120);

    lcd_SendCmd(0xb1); lcd_SendByte(0x01); lcd_SendByte(0x2c); lcd_SendByte(0x2d);

    lcd_SendCmd(0xb2); lcd_SendByte(0x01); lcd_SendByte(0x2c); lcd_SendByte(0x2d);

    lcd_SendCmd(0xb3); lcd_SendByte(0x01); lcd_SendByte(0x2c); lcd_SendByte(0x2d); lcd_SendByte(0x01); lcd_SendByte(0x2c); lcd_SendByte(0x2d);

    lcd_SendCmd(0xb4); lcd_SendByte(0x07);

    lcd_SendCmd(0xc0); lcd_SendByte(0xA2); lcd_SendByte(0x02); lcd_SendByte(0x84);

    lcd_SendCmd(0xc1); lcd_SendByte(0xc5);

    lcd_SendCmd(0xc2); lcd_SendByte(0x0a); lcd_SendByte(0x00);

    lcd_SendCmd(0xc3); lcd_SendByte(0x8a); lcd_SendByte(0x2a);

    lcd_SendCmd(0xc4); lcd_SendByte(0x8a); lcd_SendByte(0xee);

    lcd_SendCmd(0xc5); lcd_SendByte(0x0e); 

    lcd_SendCmd(0x36);
    switch(lcd_data.dir)
    {
        case LCD_DISPLAY_NORMAL: lcd_SendByte(1 << 7 | 1 << 6 | 0 << 5); break;
        case LCD_DISPLAY_RORARE_90: lcd_SendByte(1 << 7 | 0 << 6 | 1 << 5); break;
        case LCD_DISPLAY_RORARE_180: lcd_SendByte(0 << 7 | 0 << 6 | 0 << 5); break;
        case LCD_DISPLAY_RORARE_270: lcd_SendByte(0 << 7 | 1 << 6 | 1 << 5); break;
    }

    lcd_SendCmd(0xe0); lcd_SendByte(0x0f); lcd_SendByte(0x1a); lcd_SendByte(0x0f); lcd_SendByte(0x18); lcd_SendByte(0x2f); lcd_SendByte(0x28); 
    lcd_SendByte(0x20); lcd_SendByte(0x22); lcd_SendByte(0x1f); lcd_SendByte(0x1b); lcd_SendByte(0x23); lcd_SendByte(0x37); lcd_SendByte(0x00); 
    lcd_SendByte(0x07); lcd_SendByte(0x02); lcd_SendByte(0x10); 

    lcd_SendCmd(0xe1); lcd_SendByte(0x0f); lcd_SendByte(0x1b); lcd_SendByte(0x0f); lcd_SendByte(0x17); lcd_SendByte(0x33); lcd_SendByte(0x2c); 
    lcd_SendByte(0x29); lcd_SendByte(0x2e); lcd_SendByte(0x30); lcd_SendByte(0x30); lcd_SendByte(0x39); lcd_SendByte(0x3f); lcd_SendByte(0x00); 
    lcd_SendByte(0x07); lcd_SendByte(0x03); lcd_SendByte(0x10);

    lcd_SendCmd(0x2a); lcd_SendByte(0x00); lcd_SendByte(0x02); lcd_SendByte(0x00); lcd_SendByte(0x82);
    lcd_SendCmd(0x2b); lcd_SendByte(0x00); lcd_SendByte(0x03); lcd_SendByte(0x00); lcd_SendByte(0x83);

    lcd_SendCmd(0xf0); lcd_SendByte(0x01); lcd_SendCmd(0xf6); lcd_SendByte(0x00); 
    
    lcd_SendCmd(0x3a); lcd_SendByte(0x05); lcd_SendCmd(0x29); 

    lcd_FullColor(0xffff);

    LCD_CS(1);
}
