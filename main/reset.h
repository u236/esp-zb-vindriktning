#ifndef RESET_H
#define RESET_H

#include <stdint.h>

void    reset_init(void);
void    reset_count_update(uint8_t count);
uint8_t reset_pending(void);

#endif
