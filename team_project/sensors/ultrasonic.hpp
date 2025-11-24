#pragma once
#include <wiringPi.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#ifndef ULTRASONIC_H
#define ULTRASONIC_H





class Ultrasonic{
public:
    Ultrasonic(int trig_pin, int echo_pin);

    float getDistance();  //cm
    float computeTTC(float distance_cm);

    float getVrelAvg() const { return vrel_avg; }
    float getVrelMin() const { return vrel_min; }
    float getVrelMax() const { return vrel_max; }

private:
    int trig_;
    int echo_;

    float prev_distance_m = -1.0f;
    unsigned long prev_time_us = 0;

    float dtSeconds(unsigned long now, unsigned long prev);


    //-------------
    float vrel_min = 9999.0f;
    float vrel_max = -9999.0f;
    float vrel_avg = 0.0f;

    static constexpr int VREL_WINDOW = 10;
    float vrel_buffer[VREL_WINDOW] = {0};
    int vrel_index = 0;

};



#endif