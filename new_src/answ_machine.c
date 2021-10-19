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
        char choice[10];
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
    pjsua_media_config media_cfg;//for media
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

    media_cfg.snd_clock_rate = 0;
    media_cfg.snd_rec_latency = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    media_cfg.snd_play_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    //set callback 
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
    cfg.cred_info[0].scheme = pj_str("call");
    cfg.cred_info[0].username = pj_str(SIP_USER);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str(SIP_PASSWD);

    status = pjsua_acc_add(&cfg, PJ_TRUE, acc_id);
    if (status != PJ_SUCCESS) error_exit("Error adding account", status);

    return status;
}

static pj_status_t answ_phone_play_msg(pjsua_call_id call_id)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    pjsua_player_id player_id;
    pj_status_t status;
    const pj_str_t filename = pj_str(AUDIO_MSG);

    status = pjsua_player_create(&filename, PJMEDIA_FILE_NO_LOOP, &player_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_player_create() error", status);
    }

    status = pjsua_conf_connect(pjsua_player_get_conf_port(player_id),
                pjsua_call_get_conf_port(call_id));
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_connect() error", status);
    }

    pjsua_player_destroy(player_id);
    return status;
}

/* doesn't work because I lost my mind with tonegen */
static pj_status_t answ_phone_play_ringtone(pjsua_call_id call_id, struct app_confg_t *app_cfg)
{
    pj_status_t status;
    pjmedia_port *tone_gen;
    pjsua_conf_port_id p_id;

    /* set param of tone */
    pjmedia_tone_desc tones[1];
    tones[0].freq1 = 425;
    tones[0].freq2 = 0;
    tones[0].on_msec = 2000;
    tones[0].off_msec = 0;
    tones[0].volume = PJMEDIA_TONEGEN_VOLUME;
    tones[0].flags = 0;

    // pj_pool_t *tone_pool;
    // tone_pool = pjsua_pool_create("tone-pool", 200, 200);
    app_cfg->tmp_pool = pjsua_pool_create("tone-pool", 200, 200);

    /* create port tonegen */
    status = pjmedia_tonegen_create(app_cfg->tmp_pool, 8000, 1, 160, 16, 0, &tone_gen);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Unable to create tone generator", status);
    }

    /* play tone */
    status = pjmedia_tonegen_play(tone_gen, 1, tones, 0);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Cannot play tone", status);
    }

    /* add media port of tone to conf bridge */
    status = pjsua_conf_add_port(app_cfg->tmp_pool, tone_gen, &p_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_add_port() error", status);
    }

    /* connect media port with call port */
    status = pjsua_conf_connect(p_id, pjsua_call_get_conf_port(call_id));
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_connect() error", status);
    }

    PJ_LOG(3, (THIS_FILE, "pool capacity: %u", app_cfg->tmp_pool->capacity));

    return status;
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                 pjsip_rx_data *rdata)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    PJ_UNUSED_ARG(rdata);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!", (int)ci.remote_info.slen,
             ci.remote_info.ptr));

    /* ringing for connect hosts */
    // pjsua_call_answer(call_id, 180, NULL, NULL);
    // sleep(8); 

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
    struct app_confg_t app_cfg;
    app_cfg.call_id = call_id;
    pjsua_call_get_info(app_cfg.call_id, &app_cfg.ci);

    PJ_LOG(3, (THIS_FILE, "Media status changed %d", app_cfg.ci.media_status));
    app_cfg.already_cb = PJ_FALSE;

    if (app_cfg.ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
    {
        // When media is active, connect call to sound device.
        answ_phone_play_ringtone(app_cfg.call_id, &app_cfg);
        //answ_phone_play_msg(app_cfg.call_id);
    }
    pj_pool_release(app_cfg.tmp_pool);
    PJ_LOG(3, (THIS_FILE, "pool capacity: %u", app_cfg.tmp_pool->capacity));
}