#include "hall_sensor.hpp"
#include <iostream>
#include <wiringPi.h>
#include <wiringPiSPI.h>

using namespace std;


float T_low    = 1.86f;   // 강한 제한 시작점
float T_high   = 3.0f;   // 완전 해제 시점
float cap_min  = 20.0f;  // 최소 토크
float cap_max  = 100.0f; // 최대 토크

MCP3208::MCP3208(int spi_channel, int spi_speed, int cs_pin)
    : spi_channel_(spi_channel), cs_pin_(cs_pin)

{
    // SPI 초기화
    if (wiringPiSPISetup(spi_channel_, spi_speed) == -1) {
        cerr << "SPI setup failed." << endl;
    }

    pinMode(cs_pin_, OUTPUT);
    digitalWrite(cs_pin_, HIGH);    // CS 핀을 비활성(HIGH) 상태로 시작
}


// ADC 값 읽기
int  MCP3208::readadc(unsigned char adc_channel)
{
    unsigned char buff[3];

    // MCP3208 데이터시트에 따른 SPI 통신 명령어 구성
    buff[0] = 0x06 | ((adc_channel & 0x07) >> 2);  // Start + SGL/DIFF + D2
    buff[1] = (adc_channel & 0x07) << 6;           // D1, D0
    buff[2] = 0x00;

    digitalWrite(cs_pin_, LOW);            // Chip Select Active (Low)
    
    // 3바이트 데이터 전송 및 수신
    wiringPiSPIDataRW(spi_channel_, buff, 3);  // SPI 통신
    
    digitalWrite(cs_pin_, HIGH);    // CS Inactive

    buff[1] = buff[1] & 0x0F;
    int adcValue = (buff[1] << 8) | buff[2]; // 12bit 값 (0~4095), CH0 입력 읽기
    
    return adcValue;
}

float MCP3208::readRawThrottle(unsigned char adc_channel)
{
    int adc_val = readadc(adc_channel);
    
    float voltage = (adc_val / 4095.0f) * 3.3f;      // 전압으로 변환
    
    return voltage;
}

float MCP3208::computeCap(float TTC)
{
    // 1) 위험 영역: TTC <= T_low → 최소 토크
    if (TTC <= T_low) {
        std::cout << "TTC <= " << T_low << std::endl;
        return cap_min;        
    }

    // 2) 완화 영역: T_low < TTC <= T_high 
    else if (TTC <= T_high) {
        std::cout << "TTC <= " << T_high << std::endl;

        // 선형 보간 (linear interpolation)
        float cap = cap_min + (cap_max - cap_min) * (TTC - T_low) / (T_high - T_low);

        return cap;            
    }

    // 3) 안전 영역: TTC > T_high 
    else {
        return cap_max;        
    }
}


float MCP3208::computeThrottleCmd(float throttle_raw, float cap)
{
    return std::max(0.0f, std::min(throttle_raw, cap));
}



 

