#ifndef BUZZER_H
#define BUZZER_H

#include <wiringPi.h>
#include <softTone.h>

constexpr int SPKR = 25;    //GPIO26
constexpr int NOTE = 440;   //440Hz(A4, 라)

//부저 함수 선언
void initBuzzer();
void playBuzzer();
void stopBuzzer();


#endif