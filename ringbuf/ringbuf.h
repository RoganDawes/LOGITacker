#ifndef RINGBUF_H
#define RINGBUF_H

/**
* @defgroup nrf_ringbuf Ring buffer
* @{
* @ingroup app_common
* @brief Functions for controlling the ring buffer.
*/

#include <stdint.h>
#include "nrf_atomic.h"
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Ring buffer instance control block.
 * */
typedef struct
{
    nrf_atomic_flag_t   wr_flag;   //!< Protection flag.
    nrf_atomic_flag_t   rd_flag;   //!< Protection flag.
    uint32_t            wr_idx;     //!< Write index (updated when putting).
    uint32_t            rd_idx;     //!< Read index (updated when freeing).
    uint32_t            peek_rd_idx; //!< Temporary read index (updated when getting).
} nrf_ringbuf_cb_t;

/**
 * @brief Ring buffer instance structure.
 * */
typedef struct
{
    uint8_t           * p_buffer;     //!< Pointer to the memory used by the ring buffer.
    uint32_t            bufsize_mask; //!< Buffer size mask (buffer size must be a power of 2).
    nrf_ringbuf_cb_t  * p_cb;         //!< Pointer to the instance control block.
} nrf_ringbuf_t;

/**
 * @brief Macro for defining a ring buffer instance.
 *
 * @param _name Instance name.
 * @param _size Size of the ring buffer (must be a power of 2).
 * */
#define RINGBUF_DEF(_name, _size)                                         \
    STATIC_ASSERT(IS_POWER_OF_TWO(_size));                                    \
    static uint8_t CONCAT_2(_name,_buf)[_size];                               \
    static nrf_ringbuf_cb_t CONCAT_2(_name,_cb);                              \
    static const nrf_ringbuf_t _name = {                                      \
            .p_buffer = CONCAT_2(_name,_buf),                                 \
            .bufsize_mask = _size - 1,                                        \
            .p_cb         = &CONCAT_2(_name,_cb),                             \
    }


void ringbuf_reset(nrf_ringbuf_t const *p_ringbuf);
ret_code_t ringbuf_push_data(nrf_ringbuf_t const *p_ringbuf,
                             uint8_t const *p_data,
                             size_t *p_length);
ret_code_t ringbuf_fetch_data(nrf_ringbuf_t const *p_ringbuf,
                              uint8_t *p_data,
                              size_t *p_length);

ret_code_t ringbuf_peek_data(nrf_ringbuf_t const *p_ringbuf,
                            uint8_t *p_data,
                            size_t *p_length);
ret_code_t ringbuf_peek_rewind(nrf_ringbuf_t const *p_ringbuf);
uint32_t ringbuf_available_fetch(nrf_ringbuf_t const *p_ringbuf);
uint32_t ringbuf_available_peek(nrf_ringbuf_t const * p_ringbuf);

#ifdef __cplusplus
}
#endif

#endif //RINGBUF_H
/** @} */
