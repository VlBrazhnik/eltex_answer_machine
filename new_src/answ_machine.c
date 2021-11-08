#include "answ_machine.h"

int main(int argc, char *argv[])
{
    pj_status_t status;

    status = answ_phone_main_init();
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in main_init()", status);
    }

    status = answ_phone_main_loop();
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in main_loop()", status);
    }

    return 0;
}

static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(EXIT_SUCCESS);
}

static pj_status_t answ_phone_main_init(void)
{
    pj_status_t status = PJ_FALSE;

    status = answ_phone_init_pjsua();
    if (status != PJ_SUCCESS) error_exit("Error in init_pjsua()", status);

    status = answ_phone_init_transport();
    if (status != PJ_SUCCESS) error_exit("Error in init_transport()", status);
    
    status = answ_phone_init_sip_acc();
    if (status != PJ_SUCCESS) error_exit("Error init_sip_acc()", status);

    //init tonegen for dial tone, ring and audio player
    answ_phone_init_ring_tone();
    answ_phone_init_dial_tone();
    answ_phone_init_aud_player();

    /* init all call_id as INVALID */
    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        app_cfg.call_data[i].call_id = PJSUA_INVALID_ID;
    }
    return status;
}

static pj_status_t answ_phone_main_loop(void)
{
    pj_status_t status;

    /* when init has done, start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS)
    {
        pjsua_destroy();
        pjsua_perror(THIS_FILE, "Error in pjsua`_start()", status);
        return status;
    }

    PJ_LOG(3, (THIS_FILE, "Waiting phone call"));

    while(1)
    {
        char choice[10];
        PJ_LOG(3, (THIS_FILE, "Press 'q' for quit!"));
        if (fgets(choice, sizeof(choice), stdin) == NULL) 
        {
            PJ_LOG(3, (THIS_FILE, "Error in fgets()"));
            break;
        }

        if (choice[0] == 'q')
        {
            PJ_LOG(3, (THIS_FILE, "Quitting"));
            break;
        }
    }
    
    status = answ_phone_destroy();
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in answ_phone_destroy()", status);
    }

    return status;
}

static pj_status_t answ_phone_destroy(void)
{
    pj_status_t status;
    status = pjsua_player_destroy(app_cfg.aud_player_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in pjsua_player_destroy", status);
    }

    /* cancel any timers */
    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        if (app_cfg.call_data[i].answer_timer.id != PJSUA_INVALID_ID)
        {
            app_call_data *cd = &app_cfg.call_data[i];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            cd->answer_timer.id = PJSUA_INVALID_ID;
            pjsip_endpt_cancel_timer(endpt, &cd->answer_timer);
        }

        if (app_cfg.call_data[i].release_timer.id != PJSUA_INVALID_ID)
        {
            app_call_data *cd = &app_cfg.call_data[i];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            cd->release_timer.id = PJSUA_INVALID_ID;
            pjsip_endpt_cancel_timer(endpt, &cd->release_timer);
        }
        PJ_LOG(3, (THIS_FILE, "Timers is canceled!"));
    }

    status = pjsua_destroy();
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in pjsua_destroy()", status);
    }

    pj_pool_safe_release(&app_cfg.pool);
    app_cfg.pool = NULL;

    return status;
}

static pj_status_t answ_phone_init_pjsua(void)
{
    pjsua_config ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;
    pj_status_t status;

    //must pjsua_create before do anything
    status = pjsua_create();
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error initiate pjsua!", status);
        return status;
    }

    //create pool for application
    app_cfg.pool = pjsua_pool_create("app_pool", 1000, 1000);
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_INIT: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));

    //init default pjsua configs
    pjsua_config_default(&ua_cfg);
    pjsua_logging_config_default(&log_cfg);
    pjsua_media_config_default(&media_cfg);

    //set max calls (MAX_CALL or PJSUA_MAX_CALLS)
    ua_cfg.max_calls = MAX_CALLS;

    //set fundamental callbacks
    ua_cfg.cb.on_call_state = &on_call_state;
    ua_cfg.cb.on_call_media_state = &on_call_media_state;
    ua_cfg.cb.on_incoming_call = &on_incoming_call;

    status = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);

    return status;
}

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

static pj_status_t answ_phone_init_sip_acc(void)
{
    pj_status_t status;
    pjsua_acc_config cfg;

    pjsua_acc_config_default(&cfg);

    cfg.register_on_acc_add = PJ_FALSE;
    cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
    cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
    cfg.cred_count = 1;
    cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
    cfg.cred_info[0].scheme = pj_str("tel");
    cfg.cred_info[0].username = pj_str(SIP_USER);

    status = pjsua_acc_add(&cfg, PJ_TRUE, NULL);
    if (status != PJ_SUCCESS) error_exit("Error adding account", status);

    return status;
}

static void answ_phone_init_dial_tone(void)
{
    pj_status_t status;
    pjmedia_tone_desc tone[MAX_TONES];

    pj_str_t name = pj_str("DIAL_TONE");
    status = pjmedia_tonegen_create2(app_cfg.pool, &name, CLOCK_RATE, CHANNEL_COUNT, 
                                    SAMPLES_PER_FRAME, BITS_PER_SAMPLE, 
                                    0, &app_cfg.dial_tone_port);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() tone create", status);
    }

    /* describe tone */
    tone[0].freq1 = FREQUENCY_1;
    tone[0].freq2 = FREQUENCY_2;
    tone[0].on_msec = DIAL_TONE_ON_MSEC;
    tone[0].off_msec = DIAL_TONE_OFF_MSEC;
    tone[0].volume = PJMEDIA_TONEGEN_VOLUME;
    tone[0].flags = TONEGEN_FLAGS;

    status = pjmedia_tonegen_play(app_cfg.dial_tone_port, 1, tone, 0);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() tone play", status);
    }

    status = pjsua_conf_add_port(app_cfg.pool, app_cfg.dial_tone_port, &app_cfg.dial_tone_slot);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() add port", status);
    }
}

static void answ_phone_init_ring_tone(void)
{
    pj_status_t status;
    pjmedia_tone_desc tone[MAX_TONES];

    pj_str_t name = pj_str("RING");
    status = pjmedia_tonegen_create2(app_cfg.pool, &name, CLOCK_RATE, CHANNEL_COUNT, 
                                    SAMPLES_PER_FRAME, BITS_PER_SAMPLE, 
                                    0, &app_cfg.ring_port);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() tone create", status);
    }

    /* describe tone */
    tone[0].freq1 = FREQUENCY_1;
    tone[0].freq2 = FREQUENCY_2;
    tone[0].on_msec = RING_ON_MSEC;
    tone[0].off_msec = RING_OFF_MSEC;
    tone[0].volume = PJMEDIA_TONEGEN_VOLUME;
    tone[0].flags = TONEGEN_FLAGS;

    status = pjmedia_tonegen_play(app_cfg.ring_port, 1, tone, PJMEDIA_TONEGEN_LOOP);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() tone play", status);
    }

    status = pjsua_conf_add_port(app_cfg.pool, app_cfg.ring_port, &app_cfg.ring_slot);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in init_ring() add port", status);
    }
}

static void answ_phone_play_ring_tone(pjsua_call_id call_id)
{
    pj_status_t status;

    status = pjsua_conf_connect(app_cfg.ring_slot, pjsua_call_get_conf_port(call_id));
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_connect() error", status);
    }
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_CONNECT: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));
}

static void answ_phone_play_dial_tone(pjsua_call_id call_id)
{
    pj_status_t status;

    status = pjsua_conf_connect(app_cfg.dial_tone_slot, 
            pjsua_call_get_conf_port(call_id)); //call_ids instead call_id
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_connect() error", status);
    }
}

static void answ_phone_play_aud_msg(pjsua_call_id call_id)
{
    pj_status_t status;

    status = pjsua_conf_connect(pjsua_player_get_conf_port(app_cfg.aud_player_id),
            pjsua_call_get_conf_port(call_id));
    if (status != PJ_SUCCESS)
    {
        error_exit("Error pjsua_conf_connect()", status);
    }
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                 pjsip_rx_data *rdata)
{   
    pj_status_t status;
    pjsua_call_id current_call;
    pjsua_call_info ci[MAX_CALLS];
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    //PJ_UNUSED_ARG(rdata);

    /* add incoming call_id to array call_ids */

    for (pj_int32_t i = 0; i <= MAX_CALLS; i++)
    {
        /* add call_id if array call id is empty */
        if (app_cfg.call_data[i].call_id == PJSUA_INVALID_ID)
        {
            app_cfg.call_data[i].call_id = call_id;
            current_call = app_cfg.call_data[i].call_id;
            pjsua_call_get_info(current_call, &ci[i]);

            /* get username from recive data uri */
            uri = pjsip_uri_get_uri(rdata->msg_info.to->uri);
            if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
                error_exit("Error in get_uri()", PJ_FALSE);
            sip_uri = (pjsip_sip_uri *)uri;
            answ_phone_print_uri("Recive SIP URI ", uri);

            /* save username to arrays */
            app_cfg.call_data[i].username.ptr = (char *) pj_pool_alloc(app_cfg.pool, 256);
            if (!app_cfg.call_data[i].username.ptr)
            {
                error_exit("Error pj_pool_alloc()", PJ_FALSE);
            }
            pj_strcpy(&app_cfg.call_data[i].username, &sip_uri->user);

            PJ_LOG(2, (THIS_FILE, "SAVED USERNAME: %.*s", 
                        (int)app_cfg.call_data[i].username.slen, 
                        app_cfg.call_data[i].username.ptr));

            PJ_LOG(3,(THIS_FILE,
                  "Incoming call for account %d! "
                  "Media count: %d audio!"
                  LOG_TAB"From: %.*s "
                  LOG_TAB"To: %.*s ",
                  acc_id,
                  ci[i].rem_aud_cnt,
                  (int)ci[i].remote_info.slen,
                  ci[i].remote_info.ptr,
                  (int)ci[i].local_info.slen,
                  ci[i].local_info.ptr));

            status = pjsua_call_answer(current_call, PJSIP_SC_RINGING, NULL, NULL); //const
            if (status != PJ_SUCCESS)
            {
                error_exit("Error call_answer_180", status);
            }
            answ_phone_delay_answer(current_call);
            break;
        }
        if (i >= MAX_CALLS)
        {
            const pj_str_t reason = pj_str("TO MANY CALLS");
            status = pjsua_call_hangup(call_id, PJSIP_SC_TEMPORARILY_UNAVAILABLE, &reason, NULL);
            if (status != PJ_SUCCESS)
            {
                error_exit("Error in call_hangup()", status);
            }
        }
    }
}

static void answ_phone_print_uri(const char *title, pjsip_uri *uri)
{
    char buf[80];
    int len;

    len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, uri, buf, sizeof(buf) - 1);
    if (len < 0)
    {
        error_exit("Not enough buffer to print URI", PJ_FALSE);
    }
    buf[len] = '\0';
    PJ_LOG(3, (THIS_FILE, "%s: %s", title, buf));
}


static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);
    PJ_UNUSED_ARG(e);

    if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]",
            call_id, 
            ci.last_status,
            (int)ci.last_status_text.slen,
            ci.last_status_text.ptr));

        /* cancel answer, release timers  & clean id after answer */
        for (pj_int32_t i = 0; i < MAX_CALLS; i++)
        {
            if (app_cfg.call_data[i].call_id == call_id)
            {
                if (app_cfg.call_data[i].answer_timer.id != PJSUA_INVALID_ID)
                {
                    PJ_LOG(3, (THIS_FILE, "Free AnswerTimers: %d", call_id));
                    PJ_LOG(3, (THIS_FILE, "Free ReleaseTimer: %d", call_id));
                    app_call_data *cd = &app_cfg.call_data[i];
                    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
                    cd->answer_timer.id = PJSUA_INVALID_ID;
                    cd->release_timer.id = PJSUA_INVALID_ID;
                    pjsip_endpt_cancel_timer(endpt, &cd->answer_timer);
                    pjsip_endpt_cancel_timer(endpt, &cd->release_timer);
                }
                app_cfg.call_data[i].call_id = PJSUA_INVALID_ID;
                break;
            }
        }
    }
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pj_str_t first_name = pj_str(TEL_MARTI);
    pj_str_t second_name = pj_str(TEL_GLORI);
    pj_str_t third_name = pj_str(TEL_LENI);
    pjsua_call_info ci;
    pj_str_t tmp_name;

    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        if (app_cfg.call_data[i].call_id == call_id)
        {
            pjsua_call_get_info(call_id, &ci);
            PJ_LOG(3, (THIS_FILE, "Media status changed %d", ci.media_status));

            /* get info about input number */
            tmp_name = app_cfg.call_data[i].username;
            PJ_LOG(3, (THIS_FILE, "RECIVE USERNAME: %.*s", tmp_name.slen, tmp_name.ptr));

            /* run subscribers handler */
            if (0 == pj_strncmp(&tmp_name, &first_name, pj_strlen(&first_name)))
            {
                PJ_LOG(3, (THIS_FILE, "Call to: %.*s ", 
                                        (int)tmp_name.slen, 
                                        tmp_name.ptr));
                answ_phone_play_aud_msg(app_cfg.call_data[i].call_id);
            }

            if (0 == pj_strncmp(&tmp_name, &second_name, pj_strlen(&second_name)))
            {
                PJ_LOG(3, (THIS_FILE, "Call to: %.*s ", 
                                        (int)tmp_name.slen, 
                                        tmp_name.ptr));
                answ_phone_play_dial_tone(app_cfg.call_data[i].call_id);
            }

            if (0 == pj_strncmp(&tmp_name, &third_name, pj_strlen(&third_name)))
            {
                PJ_LOG(3, (THIS_FILE, "Call to: %.*s ", 
                                        (int)tmp_name.slen, 
                                        tmp_name.ptr));
                answ_phone_play_ring_tone(app_cfg.call_data[i].call_id);
            }
            break;
        }
    }
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_RELEASE: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));
}

static void answ_phone_delay_answer(pjsua_call_id call_id)
{
    pj_status_t status;
    pj_time_val delay;
    app_cfg.duration_ms = PJSUA_DELAY_TIME_MS;
    delay.sec = 0;
    delay.msec = app_cfg.duration_ms;
    pj_time_val_normalize(&delay);

    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        if (app_cfg.call_data[i].call_id == call_id)
        {
            app_cfg.call_data[i].answer_timer.id = PJSUA_INVALID_ID;
            app_cfg.call_data[i].answer_timer.cb = &answ_timer_cb;
            app_call_data *cd = &app_cfg.call_data[i];
            cd->answer_timer.id = call_id;

            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            status = pjsip_endpt_schedule_timer(endpt, &cd->answer_timer, &delay);
            if (status != PJ_SUCCESS)
            {
                error_exit("Error answer scheduler", status);
            }
            break;
        }
    }
}

static void answ_timer_cb(pj_timer_heap_t *h, pj_timer_entry *entry)
{
    pj_status_t status;
    pjsua_call_id call_id = entry->id;

    PJ_UNUSED_ARG(h);

    if (call_id == PJSUA_INVALID_ID) 
    {
        PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
        status = PJ_FALSE;
        error_exit("Error Invalid call id", status);
    }

    status = pjsua_call_answer(call_id, PJSIP_SC_OK, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error call_answer_200() ", status);
    }
    /*  
        after answer start release timer for hangup 
        set up release answer for hangup call 
    */
    answ_phone_release_answer(call_id);
}

static void answ_phone_release_answer(pjsua_call_id call_id)
{
    pj_status_t status;
    pj_time_val delay;
    app_cfg.release_ms = PJSUA_RELEASE_TIME_MS;

    delay.sec = 0;
    delay.msec = app_cfg.release_ms;
    pj_time_val_normalize(&delay);
    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        if (app_cfg.call_data[i].call_id == call_id)
        {
            app_cfg.call_data[i].release_timer.id = PJSUA_INVALID_ID;
            app_cfg.call_data[i].release_timer.cb = &answ_release_cb;
            app_call_data *cd = &app_cfg.call_data[i];
            cd->release_timer.id = call_id;

            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            status = pjsip_endpt_schedule_timer(endpt, &cd->release_timer, &delay);
            if (status != PJ_SUCCESS)
            {
                error_exit("Error answer scheduler", status);
            }
            break;
        }
    }
}

static void answ_release_cb(pj_timer_heap_t *h, pj_timer_entry *entry)
{
    pj_status_t status;
    pj_str_t reason = pj_str("Times out");
    pjsua_call_id call_id = entry->id;

    PJ_UNUSED_ARG(h);

    if (call_id == PJSUA_INVALID_ID) 
    {
        PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
        status = PJ_FALSE;
        error_exit("Error Invalid call id", status);
    }

    PJ_LOG(3, (THIS_FILE, "Call %d is terminated", call_id));
    status = pjsua_call_hangup(call_id, PJSIP_SC_OK, &reason, NULL);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error pjsua_call_hangup()", status);
    }
}

static void answ_phone_init_aud_player(void)
{
    pj_status_t status;
    const pj_str_t filename = pj_str(AUDIO_MSG);

    status = pjsua_player_create(&filename, PJMEDIA_FILE_NO_LOOP, &app_cfg.aud_player_id);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error pjsua_player_create()", status);
    }
}