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

    status = answ_phone_init_ring();
    if (status != PJ_SUCCESS) error_exit("Error in init_ring()", status);

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
        pjsua_perror(THIS_FILE, "Error in pjsua_start()", status);
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

        if(choice[0] == 'q')
        {
            PJ_LOG(3, (THIS_FILE, "Quitting"));
            break;
        }
    }

    if (app_cfg.no_tones == PJ_TRUE)
    {
        status = pjsua_conf_disconnect(app_cfg.conf_mslot, pjsua_call_get_conf_port(app_cfg.call_id));
        if (status != PJ_SUCCESS)
        {
            error_exit("conf_disconnect", status);
        }

        status = pjsua_conf_remove_port(app_cfg.conf_mslot);
        if (status != PJ_SUCCESS)
        {
            error_exit("Error remove_app.conf_mslot()", status);
        }
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
    pjsua_config ua_cfg; //for user agent behavior
    pjsua_logging_config log_cfg;//for log
    pjsua_media_config media_cfg;//for media
    pj_status_t status;

    //create pjsua
    status = pjsua_create();
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error initiate pjsua!", status);
        return status;
    }

    /* pool for all application */
    app_cfg.pool = pjsua_pool_create("tone-pool", 100, 100);
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_INIT: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));

    //init pjsua configs
    pjsua_config_default(&ua_cfg);
    pjsua_logging_config_default(&log_cfg);
    pjsua_media_config_default(&media_cfg);

    //set callback 
    ua_cfg.cb.on_call_state = &on_call_state;
    ua_cfg.cb.on_call_media_state = &on_call_media_state;
    ua_cfg.cb.on_incoming_call = &on_incoming_call;

    log_cfg.console_level = 4;

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
    cfg.cred_info[0].scheme = pj_str("call");
    cfg.cred_info[0].username = pj_str(SIP_USER);

    status = pjsua_acc_add(&cfg, PJ_TRUE, NULL);
    if (status != PJ_SUCCESS) error_exit("Error adding account", status);

    return status;
}

static pj_status_t answ_phone_play_lbeep(void)
{
    pj_status_t status;

    /* set param of long beep */
    pjmedia_tone_desc tones[1];
    tones[0].freq1 = 425;
    tones[0].freq2 = 0;
    tones[0].on_msec = 2000;
    tones[0].off_msec = 0;
    tones[0].volume = PJMEDIA_TONEGEN_VOLUME;
    tones[0].flags = 0;

    /* create port for long beep */
    status = pjmedia_tonegen_create(app_cfg.pool, 8000, 1, 160, 16, 0, &app_cfg.lbeep);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Unable to create tone generator", status);
    }

    /* play tone */
    status = pjmedia_tonegen_play(app_cfg.lbeep, 1, tones, 0);
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Cannot play tone", status);
    }

    /* add media port of tone to conf bridge */
    status = pjsua_conf_add_port(app_cfg.pool, app_cfg.lbeep, &app_cfg.conf_mslot);
    if (status != PJ_SUCCESS)
    {
        error_exit("pjsua_conf_add_port() error", status);
    }

    /* connect media port with call port */
    status = pjsua_conf_connect(app_cfg.conf_mslot, pjsua_call_get_conf_port(app_cfg.call_id));
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

    /* Start ringing*/
    // if (ci.rem_aud_cnt)
    // {
    //     ring_start(call_id);
    // }

    PJ_LOG(3,(THIS_FILE,
      "Incoming call for account %d!\n"
      "Media count: %d audio!\n"
      "From: %.*s\n"
      "To: %.*s\n",
      acc_id,
      ci.rem_aud_cnt,
      (int)ci.remote_info.slen,
      ci.remote_info.ptr,
      (int)ci.local_info.slen,
      ci.local_info.ptr));

    answ_phone_delay_answer(call_id);
}

static void answ_phone_delay_answer(pjsua_call_id call_id)
{
    pj_status_t status;
    app_cfg.delay = PJSUA_DELAY_TIME;

    if (app_cfg.delay)
    {
        pj_time_val delay;
        for (pj_int32_t i = 0; i < PJ_ARRAY_SIZE(app_cfg.call_data); i++)
        {
            app_cfg.call_data[i].timer.id = (pj_timer_id_t)i + 1;
            app_cfg.call_data[i].timer.cb = &answer_timer_cb;
        }
        delay.sec = 0;
        delay.msec = app_cfg.delay;
        pj_time_val_normalize(&delay);

        app_call_data *cd = &app_cfg.call_data[call_id];
        cd->timer.id = call_id;
        pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
        status = pjsip_endpt_schedule_timer(endpt, &cd->timer, &delay);
        if (status != PJ_SUCCESS)
        {
            error_exit("Error scheduler", status);
        }
    }
    else
    {
        status = pjsua_call_answer(call_id, 200, NULL, NULL);
        if (status != PJ_SUCCESS)
        {
            error_exit("Error call_answer_200()", status);
        }
    }
}

static void answer_timer_cb(pj_timer_heap_t *h, pj_timer_entry *entry)
{
    pj_status_t status;
    pjsua_call_id call_id;
    PJ_UNUSED_ARG(h);

    call_id = entry->id;
    if (call_id == PJSUA_INVALID_ID) 
    {
        PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
        status = PJ_FALSE;
        error_exit("Error Invalid call id", status);
    }

    status = pjsua_call_answer(call_id, 200, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error call_answer_200() ", status);
    }
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
    }
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pj_status_t status;
    app_cfg.call_id = call_id;
    pjsua_call_get_info(app_cfg.call_id, &app_cfg.ci);

    PJ_LOG(3, (THIS_FILE, "Media status changed %d", app_cfg.ci.media_status));

    /* stop ringing */
    //ring_stop(call_id);

    // When media is active, connect call to sound device.
    status = answ_phone_play_lbeep();
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error in play_lbeep", status); 
    }
    // pj_pool_release(app_cfg.pool);
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_RELEASE: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));
}

static void ring_start(pjsua_call_id call_id)
{
    if (app_cfg.no_tones) return;

    if (app_cfg.call_data[call_id].ring_on) return;

    app_cfg.call_data[call_id].ring_on = PJ_TRUE;

    pjsua_conf_connect(app_cfg.ring_slot, 0); /* pjsua_call_get_conf_port(call_id) */
}

static void ring_stop(pjsua_call_id call_id)
{
    pj_status_t status;

    if (app_cfg.no_tones) return;

    if (app_cfg.call_data[call_id].ringback_on)
    {
        app_cfg.call_data[call_id].ringback_on = PJ_FALSE;
    }

    status = pjsua_conf_disconnect(app_cfg.ring_slot, 0); 
    if (status != PJ_SUCCESS)
    {
        error_exit("ring_stop", status);
    }

    status = pjsua_conf_remove_port(app_cfg.ring_slot);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error remove_ring_slot()", status);
    }

    pjmedia_tonegen_rewind(app_cfg.ring_port);
}

static pj_status_t answ_phone_init_ring(void)
{
    pj_status_t status;
    pjmedia_tone_desc tone[1];

    if (app_cfg.no_tones == PJ_FALSE)
    {
        pj_str_t name = pj_str("ring_on");
        status = pjmedia_tonegen_create2(app_cfg.pool, &name, 8000, 1, 
                                        160, 16, PJMEDIA_TONEGEN_LOOP, &app_cfg.ring_port);
        if (status != PJ_SUCCESS)
        {
            error_exit("Error in init_ring() tone create", status);
        }

        /* describe tone */
        tone[0].freq1 = 425;
        tone[0].freq2 = 0;
        tone[0].on_msec = 1000;
        tone[0].off_msec = 4000;
        tone[0].volume = PJMEDIA_TONEGEN_VOLUME;
        tone[0].flags = 0;

        status = pjmedia_tonegen_play(app_cfg.ring_port, 1, tone, 0);
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
    return status;
}

/*static void sleep_start(void)
{
    pj_status_t status;
    app_cfg.duration = 4000;
    pj_uint32_t msec;
    enum { MIS = 20};

    PJ_LOG(3, (THIS_FILE, "...sleep"));

    //mark start of sleep
    status = pj_gettimeofday(&app_cfg.start);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in sleep_start()", status);
    }

    //sleep
    status = pj_thread_sleep(app_cfg.duration);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in thread_sleep()", status);
    }
    //mark end of sleep
    status = pj_gettimeofday(&app_cfg.stop);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in sleep_stop()", status);
    }
    PJ_TIME_VAL_SUB(app_cfg.stop, app_cfg.start);
    msec = PJ_TIME_VAL_MSEC(app_cfg.stop);

     // Check if it's within range. 
    if (msec < app_cfg.duration * (100-MIS)/100 ||
        msec > app_cfg.duration * (100+MIS)/100)
    {
        PJ_LOG(3,(THIS_FILE, 
              "...error: slept for %d ms instead of %d ms "
              "(outside %d%% err window)",
              msec, app_cfg.duration, MIS));
        error_exit("Error in time_range", status);
    }
}*/
