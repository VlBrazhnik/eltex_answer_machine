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

#define THIS_FILE   "MY_PHONE"
#define AUDIO_MSG   "/home/vlbrazhnikov/Local_Rep/eltex_answer_machine/audio_msg/female.wav"

#define SIP_DOMAIN  "yourbakery"
#define SIP_USER    "martin"
// #define SIP_PASSWD  "cookie"

#define N_TRANSPORT_IDS 3

#define PJSUA_TRANSPORT_RESTART_DELAY_TIME 10

struct app_confg_t
{
    pjsua_call_info ci;
    pjsua_call_id call_id;
    pjsua_conf_port_id conf_mslot;
    pjmedia_port *tone_gen;
    pj_pool_t *tmp_pool;
};

static pj_status_t answ_phone_main_init(void);
static pj_status_t answ_phone_init_pjsua(void);
static pj_status_t answ_phone_init_transport(void);
static pj_status_t answ_phone_init_sip_acc(void);
static pj_status_t answ_phone_main_loop(void);
static pj_status_t answ_phone_play_ringtone(struct app_confg_t *app_cfg);
static pj_status_t answ_phone_play_msg(pjsua_call_id call_id);

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                            pjsip_rx_data *rdata);
static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
static void on_call_media_state(pjsua_call_id call_id);
static void error_exit(const char *title, pj_status_t status);