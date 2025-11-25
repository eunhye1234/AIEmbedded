#include "lcd.hpp"
#include <wiringPiI2C.h>
#include <unistd.h>

#define LCD_CHR  1
#define LCD_CMD  0

#define LCD_LINE_1 0x80
#define LCD_LINE_2 0xC0

#define ENABLE 0b00000100


LCD::LCD(int addr) {
    fd = wiringPiI2CSetup(addr);
    lcd_init();
}

void LCD::lcd_toggle_enable(int bits) {
    usleep(500);
    wiringPiI2CWrite(fd, bits | ENABLE);
    usleep(500);
    wiringPiI2CWrite(fd, bits & ~ENABLE);
    usleep(500);
}

void LCD::lcd_byte(int bits, int mode) {
    int high = mode | (bits & 0xF0) | 0x08;  
    int low  = mode | ((bits << 4) & 0xF0) | 0x08;

    wiringPiI2CWrite(fd, high);
    lcd_toggle_enable(high);

    wiringPiI2CWrite(fd, low);
    lcd_toggle_enable(low);
}

void LCD::lcd_init() {
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD);
    usleep(5000);
}

void LCD::lcd_clear() {
    lcd_byte(0x01, LCD_CMD);
    usleep(2000);
}

void LCD::lcd_display_string(const std::string& text, int line) {
    if (line == 1)
        lcd_byte(LCD_LINE_1, LCD_CMD);
    else if (line == 2)
        lcd_byte(LCD_LINE_2, LCD_CMD);

    for (char c : text) {
        lcd_byte(c, LCD_CHR);
    }
}

void LCD::displayStatus(const std::string& line1, const std::string& line2) {
    lcd_clear();
    lcd_display_string(line1, 1);
    lcd_display_string(line2, 2);
}

void LCD::backlight(int on) {
    if (on) wiringPiI2CWrite(fd, 0x08);
    else    wiringPiI2CWrite(fd, 0x00);
}
