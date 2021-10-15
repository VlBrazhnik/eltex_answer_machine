#include "answ_machine.h"

int main(int argc, char *argv[])
{
    pj_status_t status;
    pjsua_acc_id acc_id;
    pjsua_acc_id *p_acc_id = &acc_id;

    status = answ_phone_main_init(&status, &p_acc_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("main_init fails", status);
    }
    answ_phone_main_loop();

    return 0;
}

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(EXIT_SUCCESS);
}

static pj_status_t answ_phone_main_init(pj_status_t *status, pjsua_acc_id **acc_id)
{
    *status = answ_phone_init_pjsua();
    if (*status != PJ_SUCCESS) error_exit("app_init fails", *status);

    *status = answ_phone_init_transport();
    if (*status != PJ_SUCCESS) error_exit("Error in init_transport()", *status);

    *status = answ_phone_init_sip_acc(*acc_id);
    if (*status != PJ_SUCCESS) error_exit("Error init_sip_acc()", *status);

    return *status;
}

static pj_status_t answ_phone_main_loop(void)
{
    pj_status_t status;

    /* when init has done, start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS)
    {
        pjsua_destroy();
        pjsua_perror(THIS_FILE, "pjsua cannot start", status);
        return status;
    }

    printf("Waiting phone call\n");
    printf("For quit from wait mode print 'q'\n");

    while(1)
    {
        /*uint8_t*/char choice[10];
        if (fgets(choice, sizeof(choice), stdin) == NULL) 
        {
            printf("EOF while reading stdin, will quit now..\n");
            break;
        }

        if(choice[0] == 'q')
        {
            pjsua_destroy();
            exit(0);
        }

        if(choice[0] == 'h')
        {
            pjsua_call_hangup_all();
        }
    }

    return status;
}

/* create and init pjsua */
static pj_status_t answ_phone_init_pjsua(void)
{
    pjsua_config ua_cfg; //for user agent behavior
    pjsua_logging_config log_cfg;//for log
    pjsua_media_config media_cfg;//for media: audio msg
    pj_status_t stat;

    //create pjsua
    stat = pjsua_create();
    if (stat != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error initiate pjsua!", stat);
        return stat;
    }

    //init pjsua configs
    pjsua_config_default(&ua_cfg);
    pjsua_logging_config_default(&log_cfg);
    pjsua_media_config_default(&media_cfg);

    //fill fildes
    ua_cfg.cb.on_incoming_call = &on_incoming_call;
    ua_cfg.cb.on_call_state = &on_call_state;
    ua_cfg.cb.on_call_media_state = &on_call_media_state;

    log_cfg.console_level = 4;

    stat = pjsua_init(&ua_cfg, &log_cfg, NULL);
    if (stat != PJ_SUCCESS) error_exit("Error in pjsua_init()", stat);

    return stat;
}

/* create transport for app */
static pj_status_t answ_phone_init_transport(void)
{
    pj_status_t status;
    pjsua_transport_config udp_cfg;
    pjsua_transport_info tp_info;

    pjsua_transport_config_default(&udp_cfg);
    udp_cfg.port = 7777;

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &udp_cfg, NULL);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in pjsua_transport_create()", status);
    }
    return status;
}

/* create and init sip account for server */
static pj_status_t answ_phone_init_sip_acc(pjsua_acc_id *acc_id)
{
    pj_status_t status;
    pjsua_acc_config cfg;

    pjsua_acc_config_default(&cfg);
    cfg.register_on_acc_add = PJ_FALSE;
    
    cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
    cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
    cfg.cred_count = 1;
    cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
    cfg.cred_info[0].scheme = pj_str("digest");
    cfg.cred_info[0].username = pj_str(SIP_USER);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str(SIP_PASSWD);


    status = pjsua_acc_add(&cfg, PJ_TRUE, acc_id);
    if (status != PJ_SUCCESS) error_exit("Error adding account", status);

    return status;
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                 pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!", (int)ci.remote_info.slen,
             ci.remote_info.ptr));

    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id, (int)ci.state_text.slen,
             ci.state_text.ptr));
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    printf("Check audio devices/n");
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
    {
        // When media is active, connect call to sound device.
        pjsua_conf_connect(ci.conf_slot, 0);
        pjsua_conf_connect(0, ci.conf_slot);
    }
}
