#pragma once
/* PJSUA framework */
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsua.h>
#include <pjmedia-codec.h>
#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjsip_ua.h>
#include <pjsua-lib/pjsua.h>

#define THIS_FILE    "MY_PHONE"
#define AUDIO_MSG    "/home/vlbrazhnikov/Local_Rep/eltex_answer_machine/audio_msg/female.wav"
#define SIP_DOMAIN  "yourbakery"
#define SIP_USER    "martin"
#define LOG_TAB     "\n\t\t\t"

#define MAX_TONES                   2
#define MAX_CALLS                   3

#define CLOCK_RATE                  8000
#define CHANNEL_COUNT               2
#define SAMPLES_PER_FRAME           160
#define BITS_PER_SAMPLE             16

#define FREQUENCY_1                 425
#define FREQUENCY_2                 0
#define DIAL_TONE_ON_MSEC           3000
#define DIAL_TONE_OFF_MSEC          0
#define RING_ON_MSEC                1000
#define RING_OFF_MSEC               4000
#define TONEGEN_FLAGS               0

#define PJSUA_DELAY_TIME_MS         4000
#define PJSUA_RELEASE_TIME_MS       5000

/* table of phone number */

/* Call specific data */
typedef struct app_call_data
{
    pj_timer_entry          answer_timer;//rename as answer_timer
    pj_timer_entry          release_timer;
} app_call_data;

struct app_confg_t
{
    pj_pool_t               *pool;
    pjsua_call_info         ci[MAX_CALLS];
    pjsua_call_id           call_id[MAX_CALLS];
    app_call_data           call_data[MAX_CALLS];
    unsigned                duration_ms;

    /* decribsions tonegens for dial tone & ring */
    pjmedia_tone_desc       tones[MAX_TONES];

    /*for dial tone*/
    pjsua_conf_port_id      dial_tone_slot;
    pjmedia_port            *dial_tone_port;

    /*for ring */
    pjsua_conf_port_id      ring_slot;
    pjmedia_port            *ring_port;

    pjsua_player_id         aud_player_id;
} app_cfg;

/* for all application */
extern struct app_confg_t app_cfg;

static pj_status_t answ_phone_main_init(void);
static pj_status_t answ_phone_init_pjsua(void);
static pj_status_t answ_phone_init_transport(void);
static pj_status_t answ_phone_init_sip_acc(void);
static pj_status_t answ_phone_main_loop(void);

static void answ_phone_init_dial_tone(void);
static void answ_phone_init_ring(void);
static void answ_phone_init_aud_player(void);

static void answ_phone_play_ring(pjsua_call_id call_id);
static void answ_phone_play_dial_tone(pjsua_call_id call_id);
static void answ_phone_play_aud_msg(pjsua_call_id call_id);

static void answer_phone_release_answer(pjsua_call_id call_id);
static void answer_release_cb(pj_timer_heap_t *h, pj_timer_entry *entry);
static void answ_phone_delay_answer(pjsua_call_id call_id);
static void answer_timer_cb(pj_timer_heap_t *h, pj_timer_entry *entry);

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                            pjsip_rx_data *rdata);
static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
static void on_call_media_state(pjsua_call_id call_id);
static void error_exit(const char *title, pj_status_t status);
