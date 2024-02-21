#ifndef RESET_H
#define RESET_H

#include <stdint.h>

void    reset_init(void);
void    reset_update_count(uint8_t count);
void    reset_to_factory(void);
uint8_t reset_pending(void);

#endif
