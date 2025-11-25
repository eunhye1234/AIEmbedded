#include "ultrasonic.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>


Ultrasonic::Ultrasonic(int trig_pin, int echo_pin)
    :trig_(trig_pin), echo_(echo_pin)
{
    pinMode(trig_, OUTPUT);
    pinMode(echo_, INPUT);
    digitalWrite(trig_, LOW);

    //-----------------
    // v_rel 초기화
    vrel_min = 9999.0f;
    vrel_max = -9999.0f;
    vrel_avg = 0.0f;

    for (int i = 0; i < VREL_WINDOW; i++)
        vrel_buffer[i] = 0.0f;
}

float Ultrasonic::dtSeconds(unsigned long now, unsigned long prev)
{
    return (now - prev) / 1e6f;
}


float Ultrasonic::getDistance()
{
    unsigned long TX_time = 0, RX_time = 0;
    float distance = 0.0f;
    const unsigned long timeout = 50000000; // 0.5 sec (약 171m)
    unsigned long start_time = micros();  //측정 시작 순간

    // pinMode(TRIG, OUTPUT);   //초음파 발사하라고 신호 줌
    // pinMode(ECHO, INPUT);   // high=초음파 발사, low=초음파 수신

    // Ensure trigger is LOW
    digitalWrite(trig_, LOW);
    delay(50);
    // delayMicroseconds(5);

    // Send trigger pulse (10us)
    digitalWrite(trig_, HIGH);  
    delayMicroseconds(10);
    digitalWrite(trig_, LOW);

    // Wait for ECHO to go HIGH (start of echo)
    while (digitalRead(echo_) == LOW && (micros() - start_time) < timeout)
    {
        if (digitalRead(echo_) == HIGH)    //초음파 나감
            break;
    }

    // Timeout check
    if ((micros() - start_time) > timeout)
    {
        std::cout << "0. Out of range. micros=" << micros()
                  << " start_time=" << start_time << std::endl;
        return false;
    }

    TX_time = micros();   //초음파 나간 시점

    // Wait for ECHO to go LOW (end of echo)
    while (digitalRead(echo_) == HIGH && (micros() - start_time) < timeout)
    {
        if (digitalRead(echo_) == LOW)   //초음파 들어옴
            break;
    }

    // Timeout check
    if ((micros() - start_time) > timeout)
    {
        std::cout << "1. Out of range." << std::endl;
        return false;
    }
 
    RX_time = micros();   //초음파 들어온 시점

    // Calculate distance in cm
    distance = static_cast<float>(RX_time - TX_time) * 0.017f;

    // std::cout << "Range: " << distance << " cm" << std::endl;
    // return true;
    return distance;
}

float Ultrasonic::computeTTC(float distance_cm)
{
    if (distance_cm <= 0){
        std::cout << "distance_cm <= 0" << std::endl;
        return INFINITY;
    }   

    float D_now = distance_cm / 100.0f;     //m 단위
    unsigned long now = micros();

    if (prev_distance_m < 0){
        prev_distance_m = D_now;
        prev_time_us = now;
        std::cout << "prev_distance_m <= 0" << std::endl;
        return INFINITY;
    }

    float dt = dtSeconds(now, prev_time_us);
    if (dt <= 0){
        std::cout << "dt <= 0" << std::endl;
        return INFINITY;
    }       

    float v_rel = (prev_distance_m - D_now) / dt;   //ms (접근속도)
    // v_rel = std::max(0.0f, v_rel);

    // ===== v_rel 통계 업데이트 =====
    vrel_buffer[vrel_index] = v_rel;
    vrel_index = (vrel_index + 1) % VREL_WINDOW;

    float sum = 0;
    for (int i = 0; i < VREL_WINDOW; i++)
        sum += vrel_buffer[i];

    vrel_avg = sum / VREL_WINDOW;
    vrel_min = std::min(vrel_min, v_rel);
    vrel_max = std::max(vrel_max, v_rel);

    // 출력(테스트용)
    // std::cout << "[Ultrasonic] v_rel=" << v_rel
    //           << " | avg=" << vrel_avg
    //           << " | min=" << vrel_min
    //           << " | max=" << vrel_max << "\n";
    // ==============================

    
    prev_distance_m = D_now;
    prev_time_us = now;
    std::cout << "v_rel: " << v_rel << "\n";

    if (v_rel <= 0.00001f){
        std::cout << "v_rel <= 0.00001f" << std::endl;
        return INFINITY;
    }       

    float TTC = D_now / v_rel;

    // if (TTC > 10.0f)    return TTC;
    // return 10.0f;

    return TTC;
}
