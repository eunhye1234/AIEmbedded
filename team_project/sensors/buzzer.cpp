#include "buzzer.hpp"
#include <iostream>
#include <cstdlib>


// 부저 제어 함수
void initBuzzer()
{
    if (softToneCreate(SPKR) != 0)
    {
        std::cerr << "Failed to initialize softTone on pin " << SPKR << std::endl;
    }
    else
    {
        std::cout << "Buzzer initialized on pin " << SPKR << std::endl;
    }
}

void playBuzzer()
{
    softToneWrite(SPKR, NOTE);
}

void stopBuzzer()
{
    softToneWrite(SPKR, 0);
}

