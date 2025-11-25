#ifndef MCP3208_HPP
#define MCP3208_HPP

// #include <wiringPi.h>
// #include <wiringPiSPI.h>

// constexpr int SPI_CHANNEL = 0;      // SPI0
// constexpr int SPI_SPEED   = 1000000; // 1 MHz
// constexpr int CS_MCP3208  = 10;       // GPIO8 (WiringPi 기준)


constexpr float V_MIN_CALIBRATED = 1.7f; // 예시 값 (0%)
constexpr float V_MAX_CALIBRATED = 2.2f; // 예시 값 (100%)

class MCP3208{
    public:
        MCP3208(int spi_channel, int spi_speed, int cs_pin);

        int readadc(unsigned char adc_channel);

        float readRawThrottle(unsigned char adc_channel);

        float getRaw() const { return throttle_raw_; }
        float getCmd() const { return throttle_cmd_; }

        float computeCap(float TTC);
        float computeThrottleCmd(float throttle_raw, float cap);

    
    private:
        int spi_channel_;
        int cs_pin_;

        // const float ttc_danger_;
        // const float ttc_safe_;
        // const float ttc_range_;

        float throttle_raw_ = 0.0f;
        float throttle_cmd_ = 0.0f;
};

#endif
