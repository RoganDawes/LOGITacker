#ifndef HELPER_H__
#define HELPER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


bool helper_array_check_crc16(uint8_t * p_array, uint8_t len);
void helper_array_shl(uint8_t *p_array, uint8_t len, uint8_t bits);
void helper_log_priority(char* source);
void helper_addr_to_base_and_prefix(uint8_t *out_base_addr, uint8_t *out_prefix, uint8_t const *in_addr, uint8_t in_addr_len);
void helper_base_and_prefix_to_addr(uint8_t *out_addr, uint8_t const *in_base_addr, uint8_t in_prefix, uint8_t in_addr_len);
void helper_addr_to_hex_str(char * p_result, uint8_t len, uint8_t const * const p_addr);
uint32_t helper_hex_str_to_addr(uint8_t * p_result_addr, uint8_t len, char const * const addr_str);
uint32_t helper_hex_str_to_bytes(uint8_t * p_result, uint8_t len, char const * const hex_str);
char *helper_strsep (char **stringp, const char *delim);

#endif
