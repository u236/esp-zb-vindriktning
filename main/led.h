#ifndef LED_H
#define LED_H

void    led_init(void);
void    led_set_enabled(uint8_t value);
void    led_set_brightness(uint8_t value);
void    led_set_co2(uint16_t value);
void    led_set_pm25(uint16_t value);
uint8_t led_enabled(void);
uint8_t led_brightness(void);

#endif
