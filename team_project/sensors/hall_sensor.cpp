#include "hall_sensor.hpp"
#include <iostream>
#include <wiringPi.h>
#include <wiringPiSPI.h>

using namespace std;

// constexpr int SPI_CHANNEL = 0;      // SPI0
// constexpr int SPI_SPEED   = 1000000; // 1 MHz
// constexpr int CS_MCP3208  = 10;       // GPIO8 (WiringPi 기준)

float T_low    = 1.86f;   // 강한 제한 시작점
float T_high   = 3.0f;   // 완전 해제 시점
float cap_min  = 20.0f;  // 최소 토크
float cap_max  = 100.0f; // 최대 토크

//생성자: SPI 및 CS 핀 초기화
MCP3208::MCP3208(int spi_channel, int spi_speed, int cs_pin)
    : spi_channel_(spi_channel), cs_pin_(cs_pin)
        // v_min_(v_min),
        // v_range_(v_max - v_min)

{
    // SPI 초기화
    // if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1) {
    if (wiringPiSPISetup(spi_channel_, spi_speed) == -1) {
        cerr << "SPI setup failed." << endl;
        // return -1;
    }

    // pinMode(CS_MCP3208, OUTPUT);
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

    digitalWrite(cs_pin_, LOW); 
    // digitalWrite(CS_MCP3208, LOW);             // Chip Select Active (Low)
    
    // 3바이트 데이터 전송 및 수신
    wiringPiSPIDataRW(spi_channel_, buff, 3);
    // wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);   // SPI 통신
    
    // digitalWrite(CS_MCP3208, HIGH);            // CS Inactive
    digitalWrite(cs_pin_, HIGH);  

    buff[1] = buff[1] & 0x0F;
    int adcValue = (buff[1] << 8) | buff[2]; // 12bit 값 (0~4095), CH0 입력 읽기
    
    return adcValue;
}

float MCP3208::readRawThrottle(unsigned char adc_channel)
{
    int adc_val = readadc(adc_channel);
    
    float voltage = (adc_val / 4095.0f) * 3.3f;      // 전압으로 변환

    // 2. 공식 적용
    // float raw_percent = (voltage - v_min_) / v_range_ * 100.0f;

    // 3. (중요) 값 고정 (Clamping): 0% ~ 100% 사이로 보정
    // raw_percent = std::max(0.0f, std::min(100.0f, raw_percent));
    
    // return raw_percent;
    
    return voltage;
}

float MCP3208::computeCap(float TTC)
{
    // 1) 위험 영역: TTC <= T_low → 최소 토크(cap_min)
    if (TTC <= T_low) {
        std::cout << "TTC <= " << T_low << std::endl;
        return cap_min;        // ex) 20%
    }

    // 2) 완화 영역: T_low < TTC <= T_high → 선형 증가
    else if (TTC <= T_high) {
        std::cout << "TTC <= " << T_high << std::endl;

        // 선형 보간 (linear interpolation)
        float cap = cap_min + (cap_max - cap_min) * (TTC - T_low) / (T_high - T_low);

        return cap;            // ex) 20% → 100%
    }

    // 3) 안전 영역: TTC > T_high → 제한 없음
    else {
        return cap_max;        // ex) 100%
    }
}


float MCP3208::computeThrottleCmd(float throttle_raw, float cap)
{
    return std::max(0.0f, std::min(throttle_raw, cap));
}



 

