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

/* standart directives */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define THIS_FILE   "APP"

#define SIP_DOMAIN  "example.com"
#define SIP_USER    "martin"
#define SIP_PASSWD  "cookie"

#define N_TRANSPORT_IDS 3

static pj_status_t answ_phone_main_init(pj_status_t *status, pjsua_acc_id *acc_id);
static pj_status_t answ_phone_init_pjsua(void);
static pj_status_t answ_phone_init_transport(void);
static pj_status_t answ_phone_init_sip_acc(pjsua_acc_id *acc_id);

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                            pjsip_rx_data *rdata);
static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
static void on_call_media_state(pjsua_call_id call_id);
static void error_exit(const char *title, pj_status_t status);