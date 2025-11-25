#ifndef LCD_HPP
#define LCD_HPP

#include <string>

class LCD {
private:
    int fd;

    void lcd_toggle_enable(int bits);
    void lcd_byte(int bits, int mode);
    void lcd_init();

public:
    explicit LCD(int addr = 0x27);

    void lcd_clear();
    void lcd_display_string(const std::string& text, int line);

    void displayStatus(const std::string& line1, const std::string& line2);

    void backlight(int on);
};

#endif
