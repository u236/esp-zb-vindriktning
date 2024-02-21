#ifndef FAN_H
#define FAN_H

#include <stdint.h>

void    fan_init(void);
void    fan_set_mode(uint8_t value);
uint8_t fan_mode(void);

#endif