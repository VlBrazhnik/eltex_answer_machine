#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsua.h>
#include <pjmedia-codec.h>
#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjsip_ua.h>
#include <pjsua-lib/pjsua.h>

#define THIS_FILE   "APP"

#define SIP_DOMAIN  "example.com"
#define SIP_USER    "martin"
#define SIP_PASSWD  "cookie"

static void error_exit(const char *title, pj_status_t status);
static pj_status_t app_init(void);
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                            pjsip_rx_data *rdata);