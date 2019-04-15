#ifndef HELPER_H__
#define HELPER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


bool helper_array_check_crc16(uint8_t * p_array, uint8_t len);
void helper_array_shl(uint8_t *p_array, uint8_t len, uint8_t bits);
void helper_log_priority(char* source);

#endif
