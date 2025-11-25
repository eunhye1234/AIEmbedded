#include "sensors/ultrasonic.hpp"
#include "sensors/buzzer.hpp"
#include "sensors/hall_sensor.hpp"
#include "sensors/lcd.hpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>


// 상수 정의
constexpr int SPI_CHANNEL = 0;       // SPI0
constexpr int SPI_SPEED   = 1000000; // 1 MHz
constexpr int CS_MCP3208  = 10;      // GPIO8 (WiringPi 기준)
constexpr int ADC_CHANNEL = 0;       // SS49E 센서가 연결된 채널
constexpr float V_MIN = 1.7;
constexpr float V_MAX = 2.2;

constexpr float ULTRA_THRESHOLD = 30.0;   //cm 이하일 때 경고음
constexpr float HALL_THRESHOLD = 1.9;

// 초음파 센서 핀 설정
constexpr int TRIG = 29;  // GPIO 21 (물리 핀 40)
constexpr int ECHO = 28;  // GPIO 20 (물리 핀 38)

constexpr int DELTA_WINDOW = 10;   // 최근 10개로 평균
std::vector<float> delta_buffer(DELTA_WINDOW, 0.0f);
int delta_index = 0;
float prev_distance = -1;   

int main()
{
    std::cout << "Measurement start" << std::endl;
    
    // wiringPi 초기화
    if (wiringPiSetup() == -1)
    {
        std::cerr << "Failed to initialize wiringPi!" << std::endl;
        return EXIT_FAILURE;
    }
 

    if (setuid(getuid()) < 0)
    {
        perror("Dropping privileges failed");
        return EXIT_FAILURE;
    }

    MCP3208 hall(SPI_CHANNEL, SPI_SPEED, CS_MCP3208);
    Ultrasonic ultra(TRIG, ECHO);
    initBuzzer();
    LCD lcd(0x27);

    std::ofstream logFile("log.csv", std::ios::out);

    if (logFile.tellp() == 0) {
       logFile << "distance_cm,ttc,v_rel,voltage,raw_percent,cmd_percent,delta_thr_raw,scenario,accel_detected,brake_detected,misop_flag\n";
   }

    float prev_thr_raw = 0.0f;

    
    unsigned long lockout_start_time = 0; // 0이면 잠금 아님, 0보다 크면 잠금 시작 시간
    const unsigned long LOCKOUT_DURATION_MS = 3000; // 3초 (3000 ms)

    // ---- 시나리오 입력
    int scenario_id;
    std::cout << "Enter scenario ID (0=normal, 1=slow, 2=fast, 3=stomp ...): ";
    std::cin >> scenario_id;


    while (true)
    {
        int misop_flag = 0;

        float vrel_avg = ultra.getVrelAvg();
        float vrel_min = ultra.getVrelMin();
        float vrel_max = ultra.getVrelMax();

        unsigned long current_time = millis(); // wiringPi의 ms 타이머
        float distance = ultra.getDistance();
        float ttc = ultra.computeTTC(distance);

        // ------------------------------
        // ΔDistance 계산
        float delta = 0.0f;
        if (prev_distance > 0) {
            delta = std::fabs(distance - prev_distance);
        }
        prev_distance = distance;

        // circular buffer 에 저장
        delta_buffer[delta_index] = delta;
        delta_index = (delta_index + 1) % DELTA_WINDOW;

        // delta 평균 계산
        float delta_sum = 0;
        for (float d : delta_buffer) delta_sum += d;
        float delta_avg = delta_sum / DELTA_WINDOW;
        //-------------------------------------

        float voltage = hall.readRawThrottle(0); 
        float thr_raw = std::max(0.0f, (voltage - V_MIN) / (V_MAX - V_MIN) * 100);

        float delta_thr_raw = thr_raw - prev_thr_raw;
        float thr_cmd = thr_raw; 


        bool accel_detected = false;
        bool brake_detected = false;
        // // YOLO flag check
        // ACCEL 체크  
        std::ifstream accelFile("/tmp/accel_detected.flag");

        // BRAKE 체크
        {
            std::ifstream brakeFile("/tmp/brake_detected.flag");
            if (brakeFile.good()) {
                double ts;
                brakeFile >> ts;
                double now = millis() / 1000.0;

                if (now - ts <= 1.5) {
                    brake_detected = true;
                    std::cout << ">>> BRAKE detected\n";
                }

                system("rm /tmp/brake_detected.flag");
            }
        }

        if (accelFile.good()) 
        {
            double ts;
            accelFile >> ts;
            double now = millis() / 1000.0;
            if (now - ts <= 1.5) {     // 최근 1.5초 이내 감지
                    accel_detected = true;
                    std::cout << ">>> ACCEL detected\n";
                }
            // 파일 삭제해서 중복 감지 방지
            std::system("rm /tmp/accel_detected.flag");

        }
        // ==================== 오조작 감지 및 3초 잠금 로직 =====================

        // 1. 현재 잠금 상태인지 확인
        if (lockout_start_time > 0) 
        {
            misop_flag = 1;

            // 3초가 지났는지 확인
            if (current_time - lockout_start_time < LOCKOUT_DURATION_MS)
            {
                // [상태 1: 잠금 활성화]
                // 3초가 아직 안 지났으면, 스로틀을 0%로 강제 고정
                thr_cmd = 0.0f;
                std::cout << "!!! LOCKOUT ACTIVE !!! -> Throttle disabled (" 
                          << (LOCKOUT_DURATION_MS - (current_time - lockout_start_time)) / 1000.0f
                          << "s left)\n";
                playBuzzer();            
                delay(200);
                stopBuzzer();
                delay(100);

                lcd.displayStatus("LOCKOUT ACTIVE", "Accelerate: 0%");
            }
            else
            {
                // [상태 2: 잠금 해제]
                // 3초가 지났으므로 잠금 해제
                lockout_start_time = 0;
                std::cout << "Lockout expired. Resuming normal control.\n";
                stopBuzzer();

                lcd.backlight(0);   // 백라이트 OFF
                lcd.lcd_clear();
            }
        }


        // 2. 잠금 상태가 아니라면, 정상 로직 수행 (새로운 오조작 감지)
        else{
            if (accel_detected)
            {
                // std::cout << ">>> CAR DETECTED (from YOLO) — Starting mis-operation detection!\n";
                

                // 2a. 새로운 오조작 감지
                if (delta_thr_raw >= 70.0f) 
                {
                    std::cout << "!!! PEDAL STOMP DETECTED !!! -> Engaging 3-second lockout.\n";

                    misop_flag = 1;

                    lockout_start_time = current_time; // 3초 잠금 타이머 시작
                    thr_cmd = 0.0f; // 즉시 0%로 고정
                    playBuzzer();            
                    delay(200);
                    stopBuzzer();
                }

                // 2b. 정상 주행 (TTC 기반)
                else if (delta_thr_raw > 0.0f && delta_thr_raw < 70.0f) 
                {
                    float cap = hall.computeCap(ttc);
                    thr_cmd = hall.computeThrottleCmd(thr_raw, cap);
                    if (ttc <= 1.86){
                        misop_flag = 1;
                        playBuzzer();            
                        delay(200);
                        stopBuzzer();
                    } 

                    else stopBuzzer();
                }

            }

        }

        if (brake_detected){
            if (delta_thr_raw >= 70.0f){
                system("python3 /home/pi/AIEmbedded/AIEmbedded/team_project/send_speed.py --speed 0");
                playBuzzer();            
                delay(200);
                stopBuzzer();   
                lcd.displayStatus("Hard Brake Detected", "Assist Mode Activated"); 
            }  
        
        }
        

        std::cout 
            << "Dist: " << distance 
            << " | ΔAvg: " << delta_avg
            << " cm | TTC: " << ttc 
            << " | Vrel_avg: " << vrel_avg
            << " | voltage: " << voltage
            << " s | raw: " << thr_raw 
            << " % | cmd: " << thr_cmd 
            << " % | delta_thr_raw: " << delta_thr_raw 
            << " | misop_flag: " << misop_flag
            << "%\n"
            << "=========================="
            << "%\n";

        logFile 
        << distance << ","
        << ttc << ","
        << vrel_avg << ","
        << voltage << ","
        << thr_raw << ","
        << thr_cmd << ","
        << delta_thr_raw << ","
        << scenario_id << ","
        << accel_detected << ","      
        << brake_detected << ","      
        << misop_flag << "\n";

    logFile.flush();

        prev_thr_raw = thr_raw;

        delay(500); // 100 ms
    }

    return 0;
}