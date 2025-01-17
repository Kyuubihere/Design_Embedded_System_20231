
#include "stm32f4xx_hal.h"
#include "max30102.h"
#include "i2c.h"
#include "usart.h"
#define MAX30102_ADDR_WRITE 0xae
#define MAX30102_ADDR_READ 0xaf

#define RES_INTERRUPT_STATUS_1 0x00
#define RES_INTERRUPT_STATUS_2 0x01
#define RES_INTERRUPT_ENABLE_1 0x02
#define RES_INTERRUPT_ENABLE_2 0x03
#define RES_FIFO_WRITE_POINTER 0x04
#define RES_OVERFLOW_COUNTER 0x05
#define RES_FIFO_READ_POINTER 0x06
#define RES_FIFO_DATA_REGISTER 0x07
#define RES_FIFO_CONFIGURATION 0x08
#define RES_MODE_CONFIGURATION 0x09
#define RES_SPO2_CONFIGURATION 0x0a
#define RES_LED_PULSE_AMPLITUDE_1 0x0c
#define RES_LED_PULSE_AMPLITUDE_2 0x0d
#define RES_PROXIMITY_MODE_LED_PULSE_AMPLITUDE 0x10
#define RES_MULTI_LED_MODE_CONTROL_1 0x11
#define RES_MULTI_LED_MODE_CONTROL_2 0x12
#define RES_DIE_TEMP_INTEGER 0x1f
#define RES_DIE_TEMP_FRACTION 0x20
#define RES_DIE_TEMPERATURE_CONFIG 0x21
#define RES_PROXIMITY_INTERRUPT_THRESHOLD 0x30
#define RES_REVISION_ID 0xfe
#define RES_PART_ID 0xff

extern I2C_HandleTypeDef hi2c1;

void max30102_init()
{
    uint8_t data = 0;
    /*reset*/
    data = 0x40;

    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1);
    //osDelay(1);
    do
    {
        HAL_I2C_Mem_Read_IT(&hi2c1, MAX30102_ADDR_READ, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1);
    } while (data & 0x40);

    /*新数据中断*/
    data = 0x40;
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_INTERRUPT_ENABLE_1, I2C_MEMADD_SIZE_8BIT, &data, 1);
    //    /* 温度转换完中断*/
    //    data = 0x02;
    //    HAL_I2C_Mem_Write(&, MAX30102_ADDR_WRITE, RES_INTERRUPT_ENABLE_2, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    //    /*25个采样未读中断*/
    //    data = 0x7;
    //    /* 快满中断*/
    //    data = 0x80;
    //    HAL_I2C_Mem_Write(&, MAX30102_ADDR_WRITE, RES_INTERRUPT_ENABLE_1, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    //    HAL_I2C_Mem_Write(&, MAX30102_ADDR_WRITE, RES_FIFO_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    /*16384量程 50Hz 18位adc分辨率*/
    data = 0x63;
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_SPO2_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1);
    /*灯的亮度*/
    data = 0x47;
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_LED_PULSE_AMPLITUDE_1, I2C_MEMADD_SIZE_8BIT, &data, 1);
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_LED_PULSE_AMPLITUDE_2, I2C_MEMADD_SIZE_8BIT, &data, 1);
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_PROXIMITY_MODE_LED_PULSE_AMPLITUDE, I2C_MEMADD_SIZE_8BIT, &data, 1);
    /*FIFO clear*/
    data = 0;
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_FIFO_WRITE_POINTER, I2C_MEMADD_SIZE_8BIT, &data, 1);
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_OVERFLOW_COUNTER, I2C_MEMADD_SIZE_8BIT, &data, 1);
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_FIFO_READ_POINTER, I2C_MEMADD_SIZE_8BIT, &data, 1);

    /*interrupt status clear*/
    max30102_getStatus();

    // /*转换温度*/
    // data = 1;
    // HAL_I2C_Mem_Write(&, MAX30102_ADDR_WRITE, RES_DIE_TEMPERATURE_CONFIG, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    /*SPO2 Mode*/
    data = 0x03;
    HAL_I2C_Mem_Write_IT(&hi2c1, MAX30102_ADDR_WRITE, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1);
    HAL_UART_Transmit(&huart2, (uint8_t *) "\x1B[34m[Action]: Max30102\r\n", 27, 50);
}

uint8_t max30102_getUnreadSampleCount()
{
    uint8_t wr = 0, rd = 0;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_FIFO_WRITE_POINTER, I2C_MEMADD_SIZE_8BIT, &wr, 1, 10);
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_FIFO_READ_POINTER, I2C_MEMADD_SIZE_8BIT, &rd, 1, 10);
    if ((wr - rd) < 0)
        return wr - rd + 32;
    else
        return wr - rd;
}

void max30102_getFIFO(SAMPLE *data, uint8_t sampleCount)
{
    uint8_t dataTemp[5 * 6];
    if (sampleCount > 5)
        sampleCount = 5;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_FIFO_DATA_REGISTER,
                     I2C_MEMADD_SIZE_8BIT, dataTemp,
                     6 * sampleCount, 25);
    uint8_t i;
    for (i = 0; i < sampleCount; i++)
    {
        data[i].red = (((uint32_t)dataTemp[i * 6]) << 16 | ((uint32_t)dataTemp[i * 6 + 1]) << 8 | dataTemp[i * 6 + 2]) & 0x3ffff;
        data[i].iRed = (((uint32_t)dataTemp[i * 6 + 3]) << 16 | ((uint32_t)dataTemp[i * 6 + 4]) << 8 | dataTemp[i * 6 + 5]) & 0x3ffff;
    }
}

uint8_t max30102_getStatus()
{
    uint8_t data = 0, dataTemp = 0;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_INTERRUPT_STATUS_1, I2C_MEMADD_SIZE_8BIT, &dataTemp, 1, 10);
    data = dataTemp;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_INTERRUPT_STATUS_2, I2C_MEMADD_SIZE_8BIT, &dataTemp, 1, 10);
    return data | dataTemp;
}

// 用这个要读状态确定是否转换完
// float max30102_getTemperature()
// {
//     int8_t dataTemp = 0;
//     float data;
//     HAL_I2C_Mem_Read(&, MAX30102_ADDR_READ, RES_DIE_TEMP_INTEGER, I2C_MEMADD_SIZE_8BIT, (uint8_t *)(&dataTemp), 1, 10);
//     data = dataTemp;
//     HAL_I2C_Mem_Read(&, MAX30102_ADDR_READ, RES_DIE_TEMP_FRACTION, I2C_MEMADD_SIZE_8BIT, (uint8_t *)(&dataTemp), 1, 10);
//     data += dataTemp * 0.0625;
//     // 启动下一次转换
//     dataTemp = 1;
//     HAL_I2C_Mem_Write(&, MAX30102_ADDR_WRITE, RES_DIE_TEMPERATURE_CONFIG, I2C_MEMADD_SIZE_8BIT, (uint8_t *)(&dataTemp), 1, 10);
//     return data;
// }

void max30102_OFF()
{
    uint8_t data = 0;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    data |= 0x80;
    HAL_I2C_Mem_Write(&hi2c1, MAX30102_ADDR_WRITE, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
}

void max30102_ON()
{
    uint8_t data = 0;
    HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR_READ, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    data &= ~(0x80);
    HAL_I2C_Mem_Write(&hi2c1, MAX30102_ADDR_WRITE, RES_MODE_CONFIGURATION, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
}

/******************************计算******************************/

#define BUFF_SIZE 50
SAMPLE sampleBuff[BUFF_SIZE];

uint8_t heartRate = 0;
uint8_t spo2 = 0;

uint16_t redAC = 0;
uint32_t redDC = 0;
uint16_t iRedAC = 0;
uint32_t iRedDC = 0;

#define FILTER_LEVEL 8 /*滤波等级*/
void filter(SAMPLE *s)
{
    uint8_t i;
    uint32_t red = 0;
    uint32_t ired = 0;
    for (i = 0; i < FILTER_LEVEL - 1; i++)
    {
        red += sampleBuff[i].red;
        ired += sampleBuff[i].iRed;
    }
    s->red = (red + s->red) / FILTER_LEVEL;
    s->iRed = (ired + s->iRed) / FILTER_LEVEL;
}

void buffInsert(SAMPLE s)
{
    uint8_t i;
    for (i = BUFF_SIZE - 1; i > 0; i--)
    {
        sampleBuff[i].red = sampleBuff[i - 1].red;
        sampleBuff[i].iRed = sampleBuff[i - 1].iRed;
    }
    sampleBuff[0].red = s.red;
    sampleBuff[0].iRed = s.iRed;
}

void calAcDc(uint16_t *rac, uint32_t *rdc, uint16_t *iac, uint32_t *idc)
{
    uint32_t rMax = sampleBuff[0].red;
    uint32_t rMin = sampleBuff[0].red;
    uint32_t iMax = sampleBuff[0].iRed;
    uint32_t iMin = sampleBuff[0].iRed;

    uint8_t i;
    for (i = 0; i < BUFF_SIZE; i++)
    {
        if (sampleBuff[i].red > rMax)
            rMax = sampleBuff[i].red;
        if (sampleBuff[i].red < rMin)
            rMin = sampleBuff[i].red;
        if (sampleBuff[i].iRed > iMax)
            iMax = sampleBuff[i].iRed;
        if (sampleBuff[i].iRed < iMin)
            iMin = sampleBuff[i].iRed;
    }
    *rac = rMax - rMin;
    *rdc = (rMax + rMin) / 2;
    *iac = iMax - iMin;
    *idc = (iMax + iMin) / 2;
}

int16_t eachSampleDiff = 0; //和上一个样本相差了多少
void max30102_cal()
{
    uint8_t unreadSampleCount = max30102_getUnreadSampleCount();
    SAMPLE sampleBuffTemp[5];
    max30102_getFIFO(sampleBuffTemp, unreadSampleCount);
    static uint8_t eachBeatSampleCount = 0;    //这次心跳历经了多少个样本
    static uint8_t lastTenBeatSampleCount[10]; //过去十次心跳每一次的样本数
    static uint32_t last_iRed = 0;             //上一次红外的值，过滤后的
    uint8_t i, ii;
    for (i = 0; i < unreadSampleCount; i++)
    {
        if (sampleBuffTemp[i].iRed < 40000) //无手指不计算，跳过
        {
            heartRate = 0;
            spo2 = 0;
            eachSampleDiff = 0;
            continue;
        }
        buffInsert(sampleBuffTemp[i]);
        calAcDc(&redAC, &redDC, &iRedAC, &iRedDC);
        filter(&sampleBuffTemp[i]);
        //计算spo2
        float R = (((float)(redAC)) / ((float)(redDC))) / (((float)(iRedAC)) / ((float)(iRedDC)));
        if (R >= 0.36 && R < 0.66)
            spo2 = (uint8_t)(107 - 20 * R);
        else if (R >= 0.66 && R < 1)
            spo2 = (uint8_t)(129.64 - 54 * R);
        //计算心率,30-250ppm  count:200-12
        eachSampleDiff = last_iRed - sampleBuffTemp[i].iRed;
        if (eachSampleDiff > 50 && eachBeatSampleCount > 12)
        {
            for (ii = 9; ii > 0; ii--)
                lastTenBeatSampleCount[i] = lastTenBeatSampleCount[i - 1];
            lastTenBeatSampleCount[0] = eachBeatSampleCount;
            uint32_t totalTime = 0;
            for (ii = 0; ii < 10; ii++)
                totalTime += lastTenBeatSampleCount[i];
            heartRate = (uint8_t)(60.0 * 10 / 0.02 / ((float)totalTime));
            eachBeatSampleCount = 0;
        }
        last_iRed = sampleBuffTemp[i].iRed;
        eachBeatSampleCount++;
    }
}

uint8_t max30102_getHeartRate() { return heartRate; }
uint8_t max30102_getSpO2() { return spo2; }
int16_t max30102_getDiff() { return eachSampleDiff; }
