#include "ringbuf.h"
#include "app_util_platform.h"
#include "nrf_assert.h"

#define WR_OFFSET 0
#define RD_OFFSET 1

void ringbuf_reset(nrf_ringbuf_t const *p_ringbuf)
{
    p_ringbuf->p_cb->wr_idx = 0;
    p_ringbuf->p_cb->rd_idx = 0;
    p_ringbuf->p_cb->peek_rd_idx = 0;
    p_ringbuf->p_cb->rd_flag   = 0;
    p_ringbuf->p_cb->wr_flag   = 0;
}

ret_code_t ringbuf_push_data(nrf_ringbuf_t const *p_ringbuf,
                             uint8_t const *p_data,
                             size_t *p_length)
{
    ASSERT(p_data);
    ASSERT(p_length);

    if (nrf_atomic_flag_set_fetch(&p_ringbuf->p_cb->wr_flag))
    {
        return NRF_ERROR_BUSY;
    }

    uint32_t available = p_ringbuf->bufsize_mask + 1 -
                         (p_ringbuf->p_cb->wr_idx -  p_ringbuf->p_cb->rd_idx);
    *p_length = available > *p_length ? *p_length : available;
    size_t   length        = *p_length;
    uint32_t masked_wr_idx = (p_ringbuf->p_cb->wr_idx & p_ringbuf->bufsize_mask);
    uint32_t trail         = p_ringbuf->bufsize_mask + 1 - masked_wr_idx;

    if (length > trail)
    {
        memcpy(&p_ringbuf->p_buffer[masked_wr_idx], p_data, trail);
        length -= trail;
        masked_wr_idx = 0;
        p_data += trail;
    }
    memcpy(&p_ringbuf->p_buffer[masked_wr_idx], p_data, length);
    p_ringbuf->p_cb->wr_idx += *p_length;

    UNUSED_RETURN_VALUE(nrf_atomic_flag_clear(&p_ringbuf->p_cb->wr_flag));

    return NRF_SUCCESS;
}

ret_code_t ringbuf_fetch_data(nrf_ringbuf_t const *p_ringbuf,
                              uint8_t *p_data,
                              size_t *p_length)
{
    ASSERT(p_data);
    ASSERT(p_length);

    if (nrf_atomic_flag_set_fetch(&p_ringbuf->p_cb->rd_flag))
    {
        return NRF_ERROR_BUSY;
    }

    uint32_t available = p_ringbuf->p_cb->wr_idx -  p_ringbuf->p_cb->rd_idx;
    *p_length = available > *p_length ? *p_length : available;
    size_t   length        = *p_length;
    uint32_t masked_rd_idx = (p_ringbuf->p_cb->rd_idx & p_ringbuf->bufsize_mask);
    uint32_t masked_wr_idx = (p_ringbuf->p_cb->wr_idx & p_ringbuf->bufsize_mask);
    uint32_t trail         = (masked_wr_idx > masked_rd_idx) ? masked_wr_idx - masked_rd_idx :
                             p_ringbuf->bufsize_mask + 1 - masked_rd_idx;

    if (length > trail)
    {
        memcpy(p_data, &p_ringbuf->p_buffer[masked_rd_idx], trail);
        length -= trail;
        masked_rd_idx = 0;
        p_data += trail;
    }
    memcpy(p_data, &p_ringbuf->p_buffer[masked_rd_idx], length);
    p_ringbuf->p_cb->rd_idx += *p_length;

    UNUSED_RETURN_VALUE(nrf_atomic_flag_clear(&p_ringbuf->p_cb->rd_flag));

    return NRF_SUCCESS;
}


ret_code_t ringbuf_peek_rewind(nrf_ringbuf_t const *p_ringbuf) {
    p_ringbuf->p_cb->peek_rd_idx = p_ringbuf->p_cb->rd_idx;
    return NRF_SUCCESS;
}

ret_code_t ringbuf_peek_data(nrf_ringbuf_t const *p_ringbuf,
                            uint8_t *p_data,
                            size_t *p_length)
{
    ASSERT(p_data);
    ASSERT(p_length);

    if (nrf_atomic_flag_set_fetch(&p_ringbuf->p_cb->rd_flag))
    {
        return NRF_ERROR_BUSY;
    }

    uint32_t available = p_ringbuf->p_cb->wr_idx -  p_ringbuf->p_cb->peek_rd_idx; //available data from peek_rd_index to wr_idx
    *p_length = available > *p_length ? *p_length : available;
    size_t   length        = *p_length;
    uint32_t masked_peek_rd_idx = (p_ringbuf->p_cb->peek_rd_idx & p_ringbuf->bufsize_mask);
    uint32_t masked_wr_idx = (p_ringbuf->p_cb->wr_idx & p_ringbuf->bufsize_mask);
    uint32_t trail         = (masked_wr_idx > masked_peek_rd_idx) ? masked_wr_idx - masked_peek_rd_idx : p_ringbuf->bufsize_mask + 1 - masked_peek_rd_idx;

    if (length > trail)
    {
        memcpy(p_data, &p_ringbuf->p_buffer[masked_peek_rd_idx], trail);
        length -= trail;
        masked_peek_rd_idx = 0;
        p_data += trail;
    }
    memcpy(p_data, &p_ringbuf->p_buffer[masked_peek_rd_idx], length);
    p_ringbuf->p_cb->peek_rd_idx += *p_length;

    UNUSED_RETURN_VALUE(nrf_atomic_flag_clear(&p_ringbuf->p_cb->rd_flag));

    return NRF_SUCCESS;}


uint32_t ringbuf_available_fetch(nrf_ringbuf_t const *p_ringbuf) {
    return p_ringbuf->bufsize_mask + 1 - (p_ringbuf->p_cb->wr_idx -  p_ringbuf->p_cb->rd_idx);
}

uint32_t ringbuf_available_peek(nrf_ringbuf_t const * p_ringbuf) {
    return p_ringbuf->bufsize_mask + 1 - (p_ringbuf->p_cb->wr_idx -  p_ringbuf->p_cb->peek_rd_idx);
}

