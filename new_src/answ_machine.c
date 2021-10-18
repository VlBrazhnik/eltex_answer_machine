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
    while(1)
    {
        printf("For quit from wait mode print 'q'\n");
        uint8_t choice[10];
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

    //change media config
    media_cfg.snd_play_latency = 3000;

    //fill fildes
    ua_cfg.cb.on_incoming_call = &on_incoming_call;
    ua_cfg.cb.on_call_media_state = &on_call_media_state;
    ua_cfg.cb.on_call_state = &on_call_state;

    log_cfg.console_level = 4;

    stat = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
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

static pj_status_t answ_phone_play_msg(pjsua_call_id *call_id)
{
    pjsua_call_info ci;
    pjsua_call_get_info(*call_id, &ci);

    pjsua_player_id player_id;
    pj_status_t status;
    const pj_str_t filename = pj_str(AUDIO_MSG);

    status = pjsua_player_create(&filename, PJMEDIA_FILE_NO_LOOP, &player_id);
    if (status != PJ_SUCCESS)
        return status;

    ci.conf_slot = pjsua_call_get_conf_port(player_id);
    status = pjsua_conf_connect(pjsua_player_get_conf_port(player_id),
                ci.conf_slot); /* why not call id? */

    return status;
}

/* doesn't work because I lost my mind with tonegen */
static pj_status_t answ_phone_play_ringtone(pjsua_call_id *call_id)
{   
    pjsua_call_info ci;
    pjsua_call_get_info(*call_id, &ci);

    pj_status_t status;
    pjmedia_tone_desc ringtone[0];
    pj_pool_t *pool_tone =  pjsua_pool_create("answ_phone", 100, 100);
    pjmedia_port *tonegen;
    pjsua_conf_port_id *p_id;

    /* set ringtone desc */
    ringtone[0].freq1 = 425;
    ringtone[0].on_msec = 10000;
    ringtone[0].off_msec = 1000;
    ringtone[0].volume = PJMEDIA_TONEGEN_VOLUME;
    ringtone[0].flags = 0;

    /* create port tonegen */
    status = pjmedia_tonegen_create(pool_tone, 425, 1, 160, 16, PJMEDIA_TONEGEN_LOOP, &tonegen);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Unable to create tone generator", status);
    }

    status = pjsua_conf_add_port(pool_tone, tonegen, p_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_add_port() error", status);
    }

    status = pjsua_conf_connect(*p_id, ci.conf_slot);
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_connect() error", status);
    }

    /* play tone */ 
    status = pjmedia_tonegen_play(tonegen, 1, ringtone, PJMEDIA_TONEGEN_LOOP);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Cannot play tone", status);
    }
    
    return status;
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                 pjsip_rx_data *rdata)
{
    pjsua_call_info ci;
    pj_status_t status;
    pjsua_call_id current_call;
    //const pjsua_msg_data msg_data = pj_str("I'm not home, call later");

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    if (current_call == PJSUA_INVALID_ID)
    {
        current_call = call_id;
    }

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!", (int)ci.remote_info.slen,
             ci.remote_info.ptr));

    /* ringing for connect hosts */
    pjsua_call_answer(call_id, 180, NULL, NULL);
    sleep(8);

    /* Answer on incoming calls with 200/OK */
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
    
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
    {
        // When media is active, connect call to sound device.
        answ_phone_play_msg(&call_id);

        /* not working */
        //answ_phone_play_ringtone(&call_id);
    }
}