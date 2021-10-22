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
#include <unistd.h>

#define THIS_FILE    "MY_PHONE"
#define AUDIO_MSG    "/home/vlbrazhnikov/Local_Rep/eltex_answer_machine/audio_msg/female.wav"
#define MAX_TONES    2

#define SIP_DOMAIN  "yourbakery"
#define SIP_USER    "martin"
// #define SIP_PASSWD  "cookie"

#define N_TRANSPORT_IDS 3

#define PJSUA_TRANSPORT_RESTART_DELAY_TIME 10

#define PJSUA_APP_NO_LIMIT_DURATION (int)0xFF

/* Call specific data */
typedef struct app_call_data
{
    pj_timer_entry      timer;
    pj_bool_t           ringback_on;
    pj_bool_t           ring_on;
} app_call_data;

struct app_confg_t
{
    pjsua_call_info ci;
    pjsua_call_id call_id;
    pjsua_conf_port_id conf_mslot;

    pj_pool_t *pool;
    pjmedia_port *lbeep;

    pjmedia_tone_desc tones[1];

    pj_bool_t no_tones;
    // pjsua_conf_port_id ringback_slot;
    // int ringback_cnt;
    // pjmedia_port *ringback_port;

    pjsua_conf_port_id ring_slot;
    // int ring_cnt;
    pjmedia_port *ring_port;
    app_call_data call_data[PJSUA_MAX_CALLS];
    unsigned duration;
    unsigned auto_answer;
    pj_timer_entry hangup_timer;

} app_cfg;

/* for all application */
extern struct app_confg_t app_cfg;
extern pjsua_call_id current_call;

static pj_status_t answ_phone_main_init(void);
static pj_status_t answ_phone_init_pjsua(void);
static pj_status_t answ_phone_init_transport(void);
static pj_status_t answ_phone_init_sip_acc(void);
static pj_status_t answ_phone_main_loop(void);
static pj_status_t answ_phone_play_lbeep(void);
// static pj_status_t answ_phone_play_msg(pjsua_call_id call_id);

/* for 180 ringing */
static pj_status_t answ_phone_init_ring(void);
static void ringback_start(pjsua_call_id call_id);
static void ring_start(pjsua_call_id call_id);
static void ring_stop(pjsua_call_id call_id);

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                            pjsip_rx_data *rdata);
static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
static void on_call_media_state(pjsua_call_id call_id);
static void error_exit(const char *title, pj_status_t status);
static void call_timeout_callback(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry);
static void hangup_timeout_callback(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry);