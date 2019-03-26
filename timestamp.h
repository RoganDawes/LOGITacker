#ifndef TIMESTAMP_H__
#define TIMESTAMP_H__

#include <stdint.h>

void timestamp_init();
uint32_t timestamp_get();
void timestamp_reset();

#endif
