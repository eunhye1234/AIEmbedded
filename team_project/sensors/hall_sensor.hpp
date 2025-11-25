#ifndef MCP3208_HPP
#define MCP3208_HPP



constexpr float V_MIN_CALIBRATED = 1.7f; 
constexpr float V_MAX_CALIBRATED = 2.2f; 

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

        float throttle_raw_ = 0.0f;
        float throttle_cmd_ = 0.0f;
};

#endif
