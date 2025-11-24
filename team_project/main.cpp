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

// === ΔDistance 평균 계산용 ===
constexpr int DELTA_WINDOW = 10;   // 최근 10개로 평균
std::vector<float> delta_buffer(DELTA_WINDOW, 0.0f);
int delta_index = 0;
float prev_distance = -1;   // 초기: 이전 데이터 없음

int main()
{
    std::cout << "HC-SR04 Ultra-sonic distance measure program" << std::endl;
    
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

    // 2. MCP3208 객체 생성 (이때 SPI와 CS핀이 설정됨)
    MCP3208 hall(SPI_CHANNEL, SPI_SPEED, CS_MCP3208);
    Ultrasonic ultra(TRIG, ECHO);
    initBuzzer();
    LCD lcd(0x27);

    std::cout << "SS49E 센서 전압을 읽기 시작합니다..." << std::endl;

    // CSV 로그 파일 준비
    std::ofstream logFile("log.csv", std::ios::out);

    // CSV에 scenario 컬럼 추가
    if (logFile.tellp() == 0) {
        logFile << "distance_cm,ttc,v_rel,voltage,raw_percent,cmd_percent,delta_thr_raw,scenario,misop_flag\n";
    }

    float prev_thr_raw = 0.0f;

    
    unsigned long lockout_start_time = 0; // 0이면 잠금 아님, 0보다 크면 잠금 시작 시간
    const unsigned long LOCKOUT_DURATION_MS = 3000; // 3초 (3000 ms)

    // ---- 루프 시작하기 전에 입력 ----
    int scenario_id;
    std::cout << "Enter scenario ID (0=normal, 1=slow, 2=fast, 3=stomp ...): ";
    std::cin >> scenario_id;


    while (true)
    {
        int misop_flag = 0;

        std::cout << "run-2" << std::endl;
        float vrel_avg = ultra.getVrelAvg();
        float vrel_min = ultra.getVrelMin();
        float vrel_max = ultra.getVrelMax();

        std::cout << "run-1" << std::endl;
        unsigned long current_time = millis(); // wiringPi의 ms 타이머
        std::cout << "run0" << std::endl;
        // float distance = ultra.getDistance();
        // float ttc = ultra.computeTTC(distance);

        // ------------------------------
        // ΔDistance 계산
        // ------------------------------
        // float delta = 0.0f;
        // if (prev_distance > 0) {
        //     delta = std::fabs(distance - prev_distance);
        // }
        // prev_distance = distance;

        // // circular buffer 에 저장
        // delta_buffer[delta_index] = delta;
        // delta_index = (delta_index + 1) % DELTA_WINDOW;

        // // delta 평균 계산
        // float delta_sum = 0;
        // for (float d : delta_buffer) delta_sum += d;
        // float delta_avg = delta_sum / DELTA_WINDOW;
        // //-------------------------------------

        std::cout << "run1" << std::endl;
        float voltage = hall.readRawThrottle(0); 
        float thr_raw = std::max(0.0f, (voltage - V_MIN) / (V_MAX - V_MIN) * 100);

        float delta_thr_raw = thr_raw - prev_thr_raw;
        if (delta_thr_raw<0) delta_thr_raw = -delta_thr_raw;
        

        // float cap = hall.computeCap(ttc);
        float thr_cmd = thr_raw; 


        std::cout << "run2" << std::endl;
        // ==================== [수정] 오조작 감지 및 3초 잠금 로직 =====================

        // 1. 현재 "잠금" 상태인지 확인
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

        // }

        // 2. "잠금" 상태가 아니라면, 정상 로직 수행 (새로운 오조작 감지 포함)
        // if (accel_detected)
        // {
        //     std::cout << ">>> CAR DETECTED (from YOLO) — Starting mis-operation detection!\n";
        //     // 파일 삭제해서 중복 감지 방지
            // std::system("rm /tmp/car_detected.flag");

            // 2a. 새로운 오조작 감지
            if (delta_thr_raw >= 70.0f) 
            {
                std::cout << "!!! PEDAL STOMP DETECTED !!! -> Engaging 3-second lockout.\n";

                misop_flag = 1;

                lockout_start_time = current_time; // 3초 잠금 타이머 시작!
                thr_cmd = 0.0f; // 즉시 0%로 고정
                playBuzzer();            
                delay(200);
                stopBuzzer();
                delay(100);
            }

            // 2b. 정상 주행 (TTC 기반)
            // else if (delta_thr_raw > 0.0f && delta_thr_raw < 70.0f) 
            // {
            //     float cap = hall.computeCap(ttc);
            //     thr_cmd = hall.computeThrottleCmd(thr_raw, cap);
            //     if (ttc <= 1.86){
            //         misop_flag = 1;
            //         playBuzzer();            
            //         delay(200);
            //         stopBuzzer();
            //         delay(100);
            //     } 

            //     else stopBuzzer();
            // }
        // }


        std::cout 
            // << "Dist: " << distance 
            // << " | ΔAvg: " << delta_avg
            // << " cm | TTC: " << ttc 
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
        // << distance << ","
        // << ttc << ","
        << vrel_avg << ","
        << voltage << ","
        << thr_raw << ","
        << thr_cmd << ","
        << delta_thr_raw << ","
        << scenario_id << ","
        << misop_flag << "\n";

    logFile.flush();

        prev_thr_raw = thr_raw;

        delay(500); // 100 ms
    }

    return 0;
}