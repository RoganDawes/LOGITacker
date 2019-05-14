#include "logitacker_flash.h"
#include "sdk_common.h"
#include "fds.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_FLASH
#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();

static bool m_fds_initialized = false;

static void wait_for_fds_ready(void)
{
    while (!m_fds_initialized) __WFE();
}

static void fds_callback(fds_evt_t const * p_evt)
{
    // runs in thread mode
    //helper_log_priority("fds_evt_handler");
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            NRF_LOG_INFO("FDS_EVENT_INIT");
            if (p_evt->result == FDS_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        case FDS_EVT_WRITE:
        {
            NRF_LOG_INFO("FDS_EVENT_WRITE");
            if (p_evt->result == FDS_SUCCESS)
            {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->write.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->write.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->write.record_key);
            }
        } break;

        case FDS_EVT_DEL_RECORD:
        {
            NRF_LOG_INFO("FDS_EVENT_DEL_RECORD");
            if (p_evt->result == FDS_SUCCESS)
            {
                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->del.record_id);
                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->del.file_id);
                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->del.record_key);
            }
            //m_delete_all.pending = false;
        } break;

        default:
            break;
    }
}

uint32_t logitacker_flash_init() {
    uint32_t ret = fds_register(fds_callback);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to initialize flash-storage, event handler registration failed: %d", ret);
        return ret;
    }

    ret = fds_init();
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("failed to initialize flash-storage: %d", ret);
        return ret;
    }

    wait_for_fds_ready();
    NRF_LOG_INFO("flash-storage initialized");
    return NRF_SUCCESS;
}
