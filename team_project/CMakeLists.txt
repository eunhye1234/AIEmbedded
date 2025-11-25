cmake_minimum_required(VERSION 3.10)
project(UltrasonicAlarm CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# wiringPi 라이브러리 포함
find_library(WIRINGPI_LIB wiringPi)
# find_library(SOFTTONE_LIB softTone)

# 실행 파일
add_executable(ultrasonic_alarm
    main.cpp
    sensors/buzzer.cpp
    sensors/ultrasonic.cpp
    sensors/hall_sensor.cpp
    sensors/lcd.cpp
)

# 링킹
target_link_libraries(ultrasonic_alarm
    ${WIRINGPI_LIB}
    #${SOFTTONE_LIB}
)



