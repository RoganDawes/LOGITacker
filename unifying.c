#include <string.h> // for memcpy
#include "nrf.h"
#include "flash_device_info.h"
#include "fds.h"
#include "unifying.h"
#include "nrf_esb_illegalmod.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "radio.h"
#include "app_timer.h"
#include "timestamp.h"
#include "app_scheduler.h"

APP_TIMER_DEF(m_timer_next_action);

typedef enum {
    REPLAY_SUBSTATE_FRAME_WAIT_TX,
    REPLAY_SUBSTATE_FRAME_TX,
    REPLAY_SUBSTATE_PING_TX,
    REPLAY_SUBSTATE_REPLAY_FAILED
} replay_substate_t;

typedef enum {
    REPLAY_EVENT_TIMER,
    REPLAY_EVENT_TX_SUCEEDED,
    REPLAY_EVENT_ACK_PAY_FOR_LAST_TX,
    REPLAY_EVENT_TX_FAILED
} replay_event_t;

typedef struct {
    bool running;
    uint8_t pipe_num;
    unifying_rf_record_set_t* p_record_set;
    uint8_t read_pos;

    bool    replay_realtime;         // tries to replay with same delays as recorded (fill gaps with 8ms keep alives)
    uint8_t keep_alives_to_insert; // how many keep alives (8ms) should be inserted between replays
    uint8_t keep_alives_needed; // how many keep alives are needed to fullfill keep_alives_to_insert before next record TX
    uint8_t replay_loops_remaining; // how often a replay has to be repeated

    replay_substate_t substate;
    nrf_esb_payload_t tx_payload;
} unifying_replay_state_t;

static unifying_replay_state_t m_replay_state;

typedef struct {
    unifying_rf_record_set_t record_sets[NRF_ESB_PIPE_COUNT];
    unifying_rf_record_t records_from_sets[NRF_ESB_PIPE_COUNT][UNIFYING_MAX_STORED_REPORTS_PER_PIPE];
    radio_rf_mode_t radio_mode_before_replay;
    unifying_replay_ack_payload_handler_t ack_handler; //handler for ack PAYLOADS DURING REPLAY
    unifying_event_handler_t event_handler; //handler for unifying events
} unifying_state_t;


APP_TIMER_DEF(m_timer_tx_record);
static unifying_state_t m_state_local;
/*
static unifying_rf_record_set_t m_record_sets[NRF_ESB_PIPE_COUNT];
static unifying_rf_record_t m_records_from_sets[NRF_ESB_PIPE_COUNT][UNIFYING_MAX_STORED_REPORTS_PER_PIPE];
static radio_rf_mode_t m_radio_mode_before_replay = RADIO_MODE_SNIFF;
*/

unifying_evt_t event;

uint32_t restoreDeviceInfoFromFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_INFO | deviceRecordIndex;

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret != FDS_SUCCESS) { return ret; }

    /* A config file is in flash. Let's update it. */
    fds_flash_record_t record_state = {0};

    // Open the record and read its contents. */
    ret = fds_record_open(&desc, &record_state);
    if (ret != FDS_SUCCESS) { return ret; }

    // restore state from flash into state (copy)
    memcpy(deviceInfo, record_state.p_data, sizeof(device_info_t));

    return fds_record_close(&desc);
}

uint32_t restoreDeviceWhitenedReportsFromFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_WHITENED_KEY_REPORTS | deviceRecordIndex;

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret != FDS_SUCCESS) { return ret; }

    /* A config file is in flash. Let's update it. */
    fds_flash_record_t record_state = {0};

    // Open the record and read its contents. */
    ret = fds_record_open(&desc, &record_state);
    if (ret != FDS_SUCCESS) { return ret; }

    // restore state from flash into state (copy)
    memcpy(reports, record_state.p_data, sizeof(whitened_replay_frames_t));

    return fds_record_close(&desc);
}

uint32_t updateDeviceInfoOnFlash(uint16_t deviceRecordIndex, device_info_t *deviceInfo) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_INFO | deviceRecordIndex;
    
    // flash record for device info
    fds_record_t flash_record = {
        .file_id           = FLASH_FILE_DEVICES,
        .key               = recordIdx,
        .data.p_data       = deviceInfo,
        // The length of a record is always expressed in 4-byte units (words).
        .data.length_words = (sizeof(*deviceInfo) + 3) / sizeof(uint32_t),
    };
    

    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret == FDS_SUCCESS) { 
        // Write the updated record to flash.
        return fds_record_update(&desc, &flash_record);
    } else {
        // no found, create new flash record
        return fds_record_write(&desc, &flash_record);
    }
}

uint32_t updateDeviceWhitenedReportsOnFlash(uint16_t deviceRecordIndex, whitened_replay_frames_t *reports) {
    ret_code_t ret;
    uint16_t recordIdx = FLASH_RECORD_PREFIX_DEVICES_DEVICE_WHITENED_KEY_REPORTS | deviceRecordIndex;

    // flash record for whitened reports
    fds_record_t flash_record = {
        .file_id           = FLASH_FILE_DEVICES,
        .key               = recordIdx,
        .data.p_data       = reports,
        // The length of a record is always expressed in 4-byte units (words).
        .data.length_words = (sizeof(*reports) + 3) / sizeof(uint32_t),
    };
    
    //Try to load first state record from flash, create if not existing
    fds_record_desc_t desc = {0}; //used as ref to current record
    fds_find_token_t  tok  = {0}; //used for find functions if records with same ID exist and we want to find next one

    ret = fds_record_find(FLASH_FILE_DEVICES, recordIdx, &desc, &tok);
    if (ret == FDS_SUCCESS) { 
        // Write the updated record to flash.
        return fds_record_update(&desc, &flash_record);
    } else {
        // no found, create new flash record
        return fds_record_write(&desc, &flash_record);
    }
}

uint8_t unifying_calculate_checksum(uint8_t * p_array, uint8_t paylen) {
/*
	chksum := byte(0xff)
	for i := 0; i < len(payload)-1; i++ {
		chksum = (chksum - payload[i]) & 0xff
	}
	chksum = (chksum + 1) & 0xff

	payload[len(payload)-1] = chksum

*/

    uint8_t checksum = 0x00;
    for (int i = 0; i < paylen; i++) {
        checksum -= p_array[i];
    }
    //checksum++;

    return checksum;
}

bool unifying_validate_payload(uint8_t * p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = unifying_calculate_checksum(p_array, paylen);
    p_array[0] = chksum;

    return true;
}

bool unifying_payload_update_checksum(uint8_t * p_array, uint8_t paylen) {
    if (paylen < 1) return false;
    uint8_t chksum = unifying_calculate_checksum(p_array, paylen-1);
    p_array[paylen-1] = chksum;

    return true;
}

uint32_t unifyingExtractCounterFromEncKbdFrame(nrf_esb_payload_t frame, uint32_t *p_counter) {
    // assure frame is encrypted keyboard
    if (frame.length != 22) return NRF_ERROR_INVALID_LENGTH;
    if ((frame.data[1] & 0x1f) != UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) return NRF_ERROR_INVALID_DATA;
    *p_counter = frame.data[10] << 24 | frame.data[11] << 16 | frame.data[12] << 8 | frame.data[13];
    return NRF_SUCCESS;
}


bool validate_record_buf_successive_keydown_keyrelease(uint8_t pipe) {
    unifying_rf_record_set_t *p_rs = &m_state_local.record_sets[pipe];
    // walk buffer backwards starting at last recorded frame
    int end_pos = (int) p_rs->write_pos - 1;
    if (end_pos < 0) end_pos += UNIFYING_MAX_STORED_REPORTS_PER_PIPE;
    int check_pos = end_pos;
    bool wantKeyRelease = true;
    uint32_t wantedCounter = p_rs->records[check_pos].counter; 
    unifying_rf_record_t* p_current_record;
    int valid_count = 0;
    for (int i=0; i<UNIFYING_MAX_STORED_REPORTS_PER_PIPE; i++) {
        check_pos = end_pos - i;
        if (check_pos < 0) check_pos += UNIFYING_MAX_STORED_REPORTS_PER_PIPE;
        
        p_current_record = &p_rs->records[check_pos];

        //check if encrypted keyboard frame, skip otherwise
        if (p_current_record->reportType != UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) continue;
        NRF_LOG_DEBUG("frame at pos %d has valid report type", check_pos);

        //check if key up/release in alternating order
        if (p_current_record->isEncrytedKeyRelease != wantKeyRelease) break;
        // toggle key release check for next iteration
        wantKeyRelease = !wantKeyRelease;
        NRF_LOG_DEBUG("frame at pos %d has valid key down/release type (key release %d)", check_pos, wantKeyRelease);

        //check counter
        if (p_current_record->counter != wantedCounter) break;
        NRF_LOG_DEBUG("frame at pos %d has valid counter %08x", check_pos, wantedCounter);
        // decrement counter for next iteration
        if (wantedCounter == 0) return false; //<--- should barely happen
        wantedCounter--;

        //if here, the recorded frame is valid
        valid_count = i;

        //success condition
        if (valid_count >= UNIFYING_MIN_STORED_REPORTS_VALID_PER_PIPE) {
            int start_pos = (end_pos - valid_count) + 1;
            if (start_pos < 0) start_pos += UNIFYING_MAX_STORED_REPORTS_PER_PIPE; //account for ring buffer style of record buffer

            // update record set
            p_rs->first_pos = (uint8_t) start_pos;
            p_rs->last_pos = (uint8_t) end_pos;
            //m_replay_state.read_pos = p_rs->first_pos;
            NRF_LOG_INFO("found enough (%d) valid frames, start %d end %d", valid_count, start_pos, end_pos);
            return true;
        }
    }

    if (valid_count != 0) NRF_LOG_INFO("found %d valid key down/up frames, starting from buffer end at %d", valid_count, end_pos);

    return false;
}


uint32_t timestamp_last = 0;
uint32_t timestamp = 0;
uint32_t timestamp_delta = 0;
// Purpose of the method is to store captured RF frames and return true, once enough frames for
// replay are recorded
// The method needs to keep track of frame sequences, to different distinguish record types
// (f.e. a encrypted key-release frame is succeeded by set-keep-alive frames, while a key down frame
// isn't)

// ToDo: fix .. timestamp_delta doesn't account for changing pipes between successive method calls
bool unifying_record_rf_frame(nrf_esb_payload_t frame) {
    bool result = false;
    if (frame.length < 5) return false; //ToDo: with error
    
    unifying_rf_record_set_t *p_rs = &m_state_local.record_sets[frame.pipe];

    if (p_rs->disallowWrite) {
        return false;
    }



    uint8_t frameType;
    bool isKeepAlive;
    unifying_frame_classify(frame, &frameType, &isKeepAlive);
    
    bool storeFrame = false;
    // in case the RF frame is of set-keep-alive type, we don't store the report, but mark a possible successive
    // encrypted keyboard report as "key release" frame
    switch (frameType) {
        case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
        {
            // we have a set keep alive frame, if previous RF frame was an encrypted keyboard report, mark as key release
            uint8_t previous_pos = p_rs->write_pos == 0 ? UNIFYING_MAX_STORED_REPORTS_PER_PIPE-1 : p_rs->write_pos - 1;
            unifying_rf_record_t* p_previous_record = &p_rs->records[previous_pos];
            if (!p_previous_record->isEncrytedKeyRelease && p_previous_record->reportType == UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) {
                p_previous_record->isEncrytedKeyRelease = true;
                NRF_LOG_INFO("last recorded encrypted keyboard frame marked as key release frame")
            } 
            
            break;
        }
        
        case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
        {
            storeFrame = true;
            break;
        }
        default:
            return false; //ignore frame
    }
    // ignore successive SetKeepAlive frames
    if (frameType == UNIFYING_RF_REPORT_SET_KEEP_ALIVE && m_state_local.record_sets[frame.pipe].lastRecordedReportType == UNIFYING_RF_REPORT_SET_KEEP_ALIVE) return false;

    m_state_local.record_sets[frame.pipe].lastRecordedReportType = frame.data[1] & UNIFYING_RF_REPORT_TYPE_MSK;
    

    if (storeFrame) {
        uint8_t pos = p_rs->write_pos;
        unifying_rf_record_t* p_record = &p_rs->records[pos];
        p_record->length = frame.length;
        p_record->reportType = frame.data[1] & UNIFYING_RF_REPORT_TYPE_MSK;
        p_record->isEncrytedKeyRelease = false;
        memcpy(p_record->data, frame.data, frame.length);
        if (p_record->reportType == UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD) {
            unifyingExtractCounterFromEncKbdFrame(frame, &p_record->counter);
        }

        timestamp_last = timestamp;
        timestamp = timestamp_get();
        timestamp_delta = timestamp - timestamp_last;
        p_record->pre_delay_ms = timestamp_delta;
        if (pos == 0) p_record->pre_delay_ms = 0; //no delay for first record

        NRF_LOG_INFO("Frame recorded stored at timer count %d, last %d, diff %d", timestamp, timestamp_last, timestamp_delta);

        pos++;
        if (pos >= UNIFYING_MAX_STORED_REPORTS_PER_PIPE) pos = 0;
        p_rs->write_pos = pos;
    }

    // ToDo: make validation method argument to this function, to allow other types of record validation
    result = validate_record_buf_successive_keydown_keyrelease(frame.pipe);

    // send event
    if (result && m_state_local.event_handler != NULL) {
        event.evt_id = UNIFYING_EVENT_STORED_SUFFICIENT_ENCRYPTED_KEY_FRAMES;
        event.pipe = frame.pipe;
        m_state_local.event_handler(&event);
    } 
    return result;
}

void unifying_frame_classify(nrf_esb_payload_t frame, uint8_t *p_outRFReportType, bool *p_outHasKeepAliveSet) { 
    //filter out frames < 5 byte length (likely ACKs)
    if (frame.length < 5) {
        p_outRFReportType = UNIFYING_RF_REPORT_INVALID;
        p_outHasKeepAliveSet = false;
        return;
    }

    *p_outRFReportType = frame.data[1]; // byte 1 is rf report type, byte 0 is device prefix
    *p_outHasKeepAliveSet = (frame.data[1] & 0x40) != 0;
    *p_outRFReportType &= UNIFYING_RF_REPORT_TYPE_MSK; //mask report type

    return;
}

#define UNIFYING_CLASSIFY_LOG_PREFIX "Unifying RF frame: "
void unifying_frame_classify_log(nrf_esb_payload_t frame) {     
    bool logKeepAliveEmpty = false;

    //filter out frames < 5 byte length (likely ACKs)
    if (frame.length < 5) {
        NRF_LOG_DEBUG("Invalid Unifying RF frame (wrong length or empty ack)");
        return;
    }

    uint8_t reportType = frame.data[1]; // byte 1 is rf report type, byte 0 is device prefix
    bool keepAliveSet = (reportType & 0x40) != 0;
    reportType &= UNIFYING_RF_REPORT_TYPE_MSK; //mask report type
    uint32_t counter;

    

    //filter out Unifying keep alive
    switch (reportType) {
        case UNIFYING_RF_REPORT_PLAIN_KEYBOARD:
            NRF_LOG_INFO("%sUnencrypted keyboard", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_MOUSE:
            NRF_LOG_INFO("%sUnencrypted mouse", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_MULTIMEDIA:
            NRF_LOG_INFO("%sUnencrypted multimedia key", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_PLAIN_SYSTEM_CTL:
            NRF_LOG_INFO("%sUnencrypted system control key", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_LED:
            NRF_LOG_INFO("%sLED (outbound)", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_SET_KEEP_ALIVE:
            NRF_LOG_INFO("%sSet keep-alive", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_HIDPP_SHORT:
            NRF_LOG_INFO("%sHID++ short", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_HIDPP_LONG:
            NRF_LOG_INFO("%sHID++ long", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case UNIFYING_RF_REPORT_ENCRYPTED_KEYBOARD:
            //counter = frame.data[10] << 24 | frame.data[11] << 16 | frame.data[12] << 8 | frame.data[13];
            counter = 0;
            if (unifyingExtractCounterFromEncKbdFrame(frame, &counter) == NRF_SUCCESS) {
                NRF_LOG_INFO("%sEncrypted keyboard, counter %08x", UNIFYING_CLASSIFY_LOG_PREFIX, counter);

            }

            return;
        case UNIFYING_RF_REPORT_PAIRING:
            NRF_LOG_INFO("%sPairing", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        case 0x00:
            if (keepAliveSet && logKeepAliveEmpty) NRF_LOG_INFO("%sEmpty keep alive", UNIFYING_CLASSIFY_LOG_PREFIX);
            return;
        default:
            NRF_LOG_INFO("%sUnknown frame type %02x, keep alive %s", UNIFYING_CLASSIFY_LOG_PREFIX, frame.data[1], keepAliveSet ? "set" : "not set");
            return;
    }
}

uint8_t keep_alive_8ms[5] = {0x00, 0x40, 0x00, 0x08, 0xb8}; 
void test_ack_handler(unifying_rf_record_set_t *p_rs, nrf_esb_payload_t const *p_ack_payload) {
    NRF_LOG_INFO("test ack handler pipe %d tx frame %d ack pay byte2 %02x", m_replay_state.pipe_num, m_replay_state.read_pos, p_ack_payload->data[1]);
    return;
}

void process_next_replay_step(replay_event_t replay_event) {
    switch (m_replay_state.substate) {
        case REPLAY_EVENT_TX_FAILED:
        {
            // change to next channel and transmit again

            // if all channels failed, change state to replay failed

            // TEST abort replay
            
            m_replay_state.substate = REPLAY_SUBSTATE_REPLAY_FAILED;
            NRF_LOG_INFO("Replay: transmission failed (timestamp %d)", timestamp_get());
        }
        break;
        case REPLAY_EVENT_TIMER:
        {
            if (m_replay_state.substate != REPLAY_SUBSTATE_FRAME_WAIT_TX) {
                NRF_LOG_WARNING("Received REPLAY_EVENT_TIMER while not in FRAME_WAIT_TX state (timestamp %d)", timestamp_get());
                return;
            }

            unifying_rf_record_t cur_rec = m_replay_state.p_record_set->records[m_replay_state.read_pos];
            memcpy(m_replay_state.tx_payload.data, cur_rec.data, cur_rec.length);
            m_replay_state.tx_payload.length = cur_rec.length;
            m_replay_state.tx_payload.pipe = m_replay_state.pipe_num;
            m_replay_state.tx_payload.noack = false;
            
            NRF_LOG_INFO("Replay: transmitting frame %d (timestamp %d)", m_replay_state.read_pos, timestamp_get());
            uint32_t err = nrf_esb_write_payload(&m_replay_state.tx_payload);
            if (err != NRF_SUCCESS) {
                NRF_LOG_WARNING("Error sending frame: %d", err);
                m_replay_state.substate = REPLAY_SUBSTATE_REPLAY_FAILED;
            } else {
                m_replay_state.substate = REPLAY_SUBSTATE_FRAME_TX;
            }
        }
        break;
        case REPLAY_EVENT_ACK_PAY_FOR_LAST_TX:
        {
            NRF_LOG_INFO("ACK PAY DURING REPLAY");
        }
        break;
        case REPLAY_EVENT_TX_SUCEEDED:
        {
            NRF_LOG_INFO("Replay frame transmission succeeded (timestamp %d)", timestamp_get());
            // schedule next frame for transmission, with proper delay

            //next read pos
            if (++m_replay_state.read_pos >= UNIFYING_MAX_STORED_REPORTS_PER_PIPE) m_replay_state.read_pos -= UNIFYING_MAX_STORED_REPORTS_PER_PIPE;

            if (m_replay_state.read_pos >= m_replay_state.p_record_set->last_pos) {
                // last frame has been transmitted, replay done

                NRF_LOG_INFO("Replay finished (timestamp %d)", timestamp_get());
                m_replay_state.p_record_set->disallowWrite = false; 
                
                // disable global replay indicator
                m_state_local.ack_handler = NULL;

                // restore old radio mode if it wasn't PTX already
                if (m_state_local.radio_mode_before_replay != RADIO_MODE_PTX) {
                    radioSetMode(m_state_local.radio_mode_before_replay);
                    nrf_esb_start_rx();
                }

                // send event
                if (m_state_local.event_handler != NULL) {
                    event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_FINISHED;
                    event.pipe = m_replay_state.pipe_num;
                    m_state_local.event_handler(&event);
                } 

                m_replay_state.running = false;
            }

            // sleep before next transmission
            uint32_t sleep_delay = 8;
            app_timer_start(m_timer_next_action, APP_TIMER_TICKS(sleep_delay), NULL);
            m_replay_state.substate = REPLAY_SUBSTATE_FRAME_WAIT_TX;
            NRF_LOG_INFO("Replay: sleeping %d ms (timestamp %d)", sleep_delay, timestamp_get());
            

        }
        break;
        default:
           NRF_LOG_INFO("process_next_replay_step ... unhandled event")
    }

    if (m_replay_state.substate == REPLAY_SUBSTATE_REPLAY_FAILED) {
        // TEST abort replay
        NRF_LOG_INFO("Replay frame transmit failed (timestamp %d)", timestamp_get());
        if (m_state_local.event_handler != NULL) {
            event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_FAILED;
            event.pipe = m_replay_state.pipe_num;
            m_state_local.event_handler(&event);
        } 
        m_replay_state.running = false;
    }
}

// returns true if the given esb event was consumed by unifying module
bool unifying_process_esb_event(nrf_esb_evt_t *p_event) {
    if (m_replay_state.running) {
        switch (p_event->evt_id)
        {
            case NRF_ESB_EVENT_TX_SUCCESS:
                process_next_replay_step(REPLAY_EVENT_TX_SUCEEDED);
                break;
            case NRF_ESB_EVENT_RX_RECEIVED:
                process_next_replay_step(REPLAY_EVENT_ACK_PAY_FOR_LAST_TX);
                break;
            case NRF_ESB_EVENT_TX_FAILED:
                process_next_replay_step(REPLAY_EVENT_TX_FAILED);
                break;
            default:
                return false;
        }
        return true;
    }
    return false;
}

void timer_next_action_handler(void* p_context) {
    //NRF_LOG_INFO("Forward replay timer event to scheduler");
    // schedule as event to main  (not isr) instead of executing directly
    
    process_next_replay_step(REPLAY_EVENT_TIMER);
}


void unifying_replay_records2(uint8_t pipe_num, bool replay_realtime, uint8_t keep_alives_to_insert, uint8_t loop_count) {
    if (m_replay_state.running) {
        NRF_LOG_WARNING("attempt to start replay, while replay is already running on pipe %d", m_replay_state.pipe_num);
        return;
    }

    m_replay_state.pipe_num = pipe_num;
    m_replay_state.p_record_set = &m_state_local.record_sets[pipe_num];
    if (replay_realtime) m_replay_state.keep_alives_to_insert = 0;
    else m_replay_state.keep_alives_to_insert = keep_alives_to_insert;
    m_replay_state.replay_loops_remaining = loop_count;

    m_replay_state.p_record_set->disallowWrite = true;
    m_replay_state.replay_realtime = replay_realtime;
    m_replay_state.read_pos = m_replay_state.p_record_set->first_pos;
    NRF_LOG_INFO("Replay: first_pos %d, read_pos %d, last_pos %d", m_replay_state.p_record_set->first_pos, m_replay_state.read_pos, m_replay_state.p_record_set->last_pos);
    m_replay_state.running = true;

    m_state_local.ack_handler = test_ack_handler;

     //store current mode and set to PTX to avoid mode toggling on every single TX (would flush RX'ed ack payloads otherwise)
    m_state_local.radio_mode_before_replay = radioGetMode();
    if (m_state_local.radio_mode_before_replay != RADIO_MODE_PTX) {
        nrf_esb_stop_rx();
        radioSetMode(RADIO_MODE_PTX);
    }

    // transmit first record
    unifying_rf_record_t cur_rec = m_replay_state.p_record_set->records[m_replay_state.read_pos];
    memcpy(m_replay_state.tx_payload.data, cur_rec.data, cur_rec.length);
    m_replay_state.tx_payload.length = cur_rec.length;
    m_replay_state.tx_payload.pipe = m_replay_state.pipe_num;
    m_replay_state.tx_payload.noack = false;
    nrf_esb_write_payload(&m_replay_state.tx_payload);
    m_replay_state.substate = REPLAY_SUBSTATE_FRAME_TX;

    // send event
    if (m_state_local.event_handler != NULL) {
        event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_STARTED;
        event.pipe = m_replay_state.pipe_num;
        m_state_local.event_handler(&event);
    } 
}


void timer_tx_record_from_scheduler(void *p_event_data, uint16_t event_size) {
    // process scheduled event for this handler in main loop during scheduler processing
    static nrf_esb_payload_t tx_payload; 
    unifying_rf_record_set_t* p_rs = m_replay_state.p_record_set;
    uint8_t current_record_pos = m_replay_state.read_pos;
    unifying_rf_record_t current_record = p_rs->records[current_record_pos];
    uint8_t keep_alives_needed = m_replay_state.keep_alives_needed;
    bool send_keep_alive = false;
    
    static nrf_esb_payload_t rx_ack_payload; 
    static bool ack_pay_received;

    //transmit RF frame
    if (keep_alives_needed == 0) {
        NRF_LOG_INFO("%d TX: Replaying frame %d", timestamp_get(), current_record_pos);
        memcpy(tx_payload.data, current_record.data, current_record.length);
        tx_payload.length = current_record.length;
        tx_payload.pipe = m_replay_state.pipe_num;
        tx_payload.noack = false;
    } else {
        NRF_LOG_INFO("%d TX: Sending 8ms keep-alive frame", timestamp_get());
        memcpy(tx_payload.data, keep_alive_8ms, sizeof(keep_alive_8ms));
        tx_payload.length = sizeof(keep_alive_8ms);
        tx_payload.pipe = m_replay_state.pipe_num;
        tx_payload.noack = false;
        send_keep_alive = true;
    }

    if (radioTransmitCollectAck(&tx_payload, true, &ack_pay_received, &rx_ack_payload)) {
        //NRF_LOG_INFO("TX succeeded")
        // call callback for ack payload handling
        //NRF_LOG_INFO("TX success, ACK PAY bool %d", ack_pay_received);
        if (ack_pay_received && m_state_local.ack_handler != NULL) m_state_local.ack_handler(p_rs, &rx_ack_payload);

        
        // update keep-alives which have to be sent before next record
        if (send_keep_alive) {
            m_replay_state.keep_alives_needed--;
        } else {
            m_replay_state.keep_alives_needed = m_replay_state.keep_alives_to_insert;
        }
    } else {
        // ToDo: channel sweep for PRX (receiver), abort if not found
        NRF_LOG_INFO("%d TX: failed, starting ping sweep for PRX...", timestamp_get());
        uint8_t ch_idx;
        bool prx_found = radioPingSweepPRX(tx_payload.pipe, &ch_idx) == NRF_SUCCESS;
        if (prx_found) {
            uint32_t ch;
            radioGetRfChannel(&ch);
            NRF_LOG_INFO("...PRX found on channel idx %d (channel %d), re-scheduling frame for sending...", ch_idx, ch);
            app_timer_start(m_timer_tx_record, APP_TIMER_TICKS(1), p_rs);
            return; // no further processing
        } else {
            NRF_LOG_INFO("...PRX lost, aborting replay!");
        }

        // replay failed
        // restore old radio mode if it wasn't PTX already
        if (m_state_local.radio_mode_before_replay != RADIO_MODE_PTX) {
            radioSetMode(m_state_local.radio_mode_before_replay);
            nrf_esb_start_rx();
        }

        // send event
        if (m_state_local.event_handler != NULL) {
            event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_FAILED;
            event.pipe = m_replay_state.pipe_num;
            m_state_local.event_handler(&event);
        } 

        m_replay_state.read_pos = p_rs->first_pos;

        // if no other pipe is replaying, disable global replay indicator
        
        m_replay_state.running = false;
        m_state_local.ack_handler = NULL;

        return; // don't restart timer

    }
    
    
    //check if follow up record has to be scheduled
    if (!send_keep_alive) {
        if (current_record_pos != p_rs->last_pos) {
            // advance read_pos to next record
            m_replay_state.read_pos = current_record_pos+1; 
            if (m_replay_state.read_pos >= UNIFYING_MAX_STORED_REPORTS_PER_PIPE) m_replay_state.read_pos -= UNIFYING_MAX_STORED_REPORTS_PER_PIPE;

            uint32_t next_record_pre_delay_ms = p_rs->records[m_replay_state.read_pos].pre_delay_ms;

            if (m_replay_state.replay_realtime && next_record_pre_delay_ms > 7) m_replay_state.keep_alives_needed = next_record_pre_delay_ms / 8; //zero delay wouldn't triger timer at all
            NRF_LOG_INFO("Next replay frame %d schedule for TX in %d ms", m_replay_state.read_pos, UNIFYING_MIN_REPLAY_DELAY_MS + m_replay_state.keep_alives_needed*UNIFYING_MIN_REPLAY_DELAY_MS);
            app_timer_start(m_timer_tx_record, APP_TIMER_TICKS(UNIFYING_MIN_REPLAY_DELAY_MS), p_rs);

        } else {
            if (m_replay_state.replay_loops_remaining > 0) {
                NRF_LOG_INFO("Restarting replay ...");
                m_replay_state.running = false;
                unifying_replay_records(m_replay_state.pipe_num, m_replay_state.replay_realtime, m_replay_state.keep_alives_to_insert, --m_replay_state.replay_loops_remaining);
            } else {
                // don't restart timer
                NRF_LOG_INFO("Replay finished");
                p_rs->disallowWrite = false; 
                
                // disable global replay indicator
                m_replay_state.running = false;
                m_state_local.ack_handler = NULL;

                // restore old radio mode if it wasn't PTX already
                if (m_state_local.radio_mode_before_replay != RADIO_MODE_PTX) {
                    radioSetMode(m_state_local.radio_mode_before_replay);
                    nrf_esb_start_rx();
                }

                // send event
                if (m_state_local.event_handler != NULL) {
                    event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_FINISHED;
                    event.pipe = m_replay_state.pipe_num;
                    m_state_local.event_handler(&event);
                } 
            }
        }
    } else {
        // restart timer to send next keep-alive
        app_timer_start(m_timer_tx_record, APP_TIMER_TICKS(UNIFYING_MIN_REPLAY_DELAY_MS), p_rs);
    }
}

void timer_tx_record_to_scheduler(void* p_context) {
    //NRF_LOG_INFO("Forward replay timer event to scheduler");
    // schedule as event to main  (not isr) instead of executing directly
    
    
    uint32_t err = app_sched_event_put(p_context, sizeof(unifying_rf_record_set_t), timer_tx_record_from_scheduler);
    if (err != NRF_SUCCESS) NRF_LOG_WARNING("FAILED TO SCHEDULE TX: %d", err);
    NRF_LOG_INFO("TX SCHEDULED %d (queue space %d)", timestamp_get(), app_sched_queue_space_get());
}

void unifying_replay_records(uint8_t pipe_num, bool replay_realtime, uint8_t keep_alives_to_insert, uint8_t loop_count) {
    if (m_replay_state.running) {
        NRF_LOG_WARNING("attempt to start replay, while replay is already running on pipe %d", m_replay_state.pipe_num);
        return;
    }

    m_replay_state.pipe_num = pipe_num;
    m_replay_state.p_record_set = &m_state_local.record_sets[pipe_num];
    //unifying_rf_record_set_t* p_rs = &m_state_local.record_sets[pipe_num];
    if (replay_realtime) m_replay_state.keep_alives_to_insert = 0;
    else m_replay_state.keep_alives_to_insert = keep_alives_to_insert;
    m_replay_state.replay_loops_remaining = loop_count;


    m_replay_state.p_record_set->disallowWrite = true;
    m_replay_state.replay_realtime = replay_realtime;
    m_replay_state.read_pos = m_replay_state.p_record_set->first_pos;
    NRF_LOG_INFO("Replay: first_pos %d, read_pos %d", m_replay_state.p_record_set->first_pos, m_replay_state.read_pos);
    m_replay_state.running = true;

    m_state_local.ack_handler = test_ack_handler;

     //store current mode and set to PTX to avoid mode toggling on every single TX (would flush RX'ed ack payloads otherwise)
    m_state_local.radio_mode_before_replay = radioGetMode();
    if (m_state_local.radio_mode_before_replay != RADIO_MODE_PTX) {
        nrf_esb_stop_rx();
        radioSetMode(RADIO_MODE_PTX);
    }

    /*
    uint8_t current_record = p_rs->first_pos;
    uint32_t delay_ms = p_rs->records[current_record].pre_delay_ms;
    if (delay_ms == 0) delay_ms++; //zero delay wouldn't triger timer at all
    app_timer_start(m_timer_tx_record, APP_TIMER_TICKS(delay_ms), p_rs);
    */

    // don't delay first tx
    app_timer_start(m_timer_tx_record, APP_TIMER_TICKS(1), m_replay_state.p_record_set);
    NRF_LOG_INFO("Replay timer started");

    // send event
    if (m_state_local.event_handler != NULL) {
        event.evt_id = UNIFYING_EVENT_REPLAY_RECORDS_STARTED;
        event.pipe = m_replay_state.pipe_num;
        m_state_local.event_handler(&event);
    } 
}

void unifying_init(unifying_event_handler_t event_handler){
    app_timer_create(&m_timer_tx_record, APP_TIMER_MODE_SINGLE_SHOT, timer_tx_record_to_scheduler);
    app_timer_create(&m_timer_next_action, APP_TIMER_MODE_SINGLE_SHOT, timer_next_action_handler);
    for (int i=0; i<NRF_ESB_PIPE_COUNT; i++) {
        m_state_local.record_sets[i].records = m_state_local.records_from_sets[i];
    }
    m_state_local.radio_mode_before_replay = RADIO_MODE_SNIFF;
    m_state_local.event_handler = event_handler;
}
