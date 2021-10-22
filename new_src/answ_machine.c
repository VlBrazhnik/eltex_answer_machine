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

    /*+++++++++++++++++++++++++++++++++++++++++++++++++++ */
    /* should I free media port? */
    // error with pelease pool

    // pj_pool_safe_release(&app_cfg.pool);
    // // app_cfg.pool = NULL;

    status = pjsua_destroy();
    if (status != PJ_SUCCESS)
    {
        error_exit("Error in pjsua_destroy()", status);
    }

    return status;
}


static pj_status_t answ_phone_init_pjsua(void)
{
    pjsua_config ua_cfg; //for user agent behavior
    pjsua_logging_config log_cfg;//for log
    pjsua_media_config media_cfg;//for media
    pj_status_t stat;
    // pj_time_val delay;

    //create pjsua
    stat = pjsua_create();
    if (stat != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error initiate pjsua!", stat);
        return stat;
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

    media_cfg.snd_clock_rate = 0;
    media_cfg.snd_rec_latency = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    media_cfg.snd_play_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    // stat = answ_phone_init_ring();
    // if (stat != PJ_SUCCESS) error_exit("Error in init_ring()", stat);

    // /* Initialize calls data */
    // for (int i = 0; i < PJ_ARRAY_SIZE(app_cfg.call_data); i++) 
    // {
    //     app_cfg.call_data[i].timer.id = PJSUA_INVALID_ID;
    //     app_cfg.call_data[i].timer.cb = &call_timeout_callback;
    // }

    app_cfg.call_data[0].timer.id = PJSUA_INVALID_ID;
    app_cfg.call_data[0].timer.cb = &call_timeout_callback;

    //set callback 
    ua_cfg.cb.on_call_state = &on_call_state;
    ua_cfg.cb.on_call_media_state = &on_call_media_state;
    ua_cfg.cb.on_incoming_call = &on_incoming_call;

    // /* set timer to hangup calls */
    // if (app_cfg.hangup_timer.id == 1)
    // {
    //     return;
    // }

    // app_cfg.hangup_timer.id = 1;
    // delay.sec = 0;
    // delay.msec = 200;
    // pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(), 
    //                         &app_cfg.hangup_timer, &delay);

    pj_timer_entry_init(&app_cfg.hangup_timer, 0, NULL,
                        &hangup_timeout_callback);

    log_cfg.console_level = 4;

    stat = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
    if (stat != PJ_SUCCESS) error_exit("Error in pjsua_init()", stat);

    stat = answ_phone_init_ring();
    if (stat != PJ_SUCCESS) error_exit("Error in init_ring()", stat);

    return stat;
}

/* Auto hangup timer callback */
static void hangup_timeout_callback(pj_timer_heap_t *timer_heap,
                    struct pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(timer_heap);
    PJ_UNUSED_ARG(entry);

    app_cfg.hangup_timer.id = 0;
    pjsua_call_hangup_all();
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
    // cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    // cfg.cred_info[0].data = pj_str(SIP_PASSWD);

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
    pj_status_t status;
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    PJ_UNUSED_ARG(rdata);

    // PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!", (int)ci.remote_info.slen,
    //          ci.remote_info.ptr));

    /* Start ringing*/
    if (ci.rem_aud_cnt)
    {
        ring_start(call_id);
    }

    if (app_cfg.auto_answer < 200)
    {
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
    }
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    PJ_UNUSED_ARG(e);

    if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        ring_stop(call_id);

        /* cancel duration timer, if any */
        if (app_cfg.call_data[call_id].timer.id != PJSUA_INVALID_ID)
        {
            app_call_data *cd = &app_cfg.call_data[call_id];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();

            cd->timer.id = PJSUA_INVALID_ID;
            pjsip_endpt_cancel_timer(endpt, &cd->timer);
        }

        PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]",
            call_id, 
            ci.last_status,
            (int)ci.last_status_text.slen,
            ci.last_status_text.ptr));

        /* there is check on curent calling */

        // /* Dump media state upon disconnected */
        // if (1)
        // {
        //     PJ_LOG(5, (THIS_FILE, "Call %d disconnected, dump media state..",
        //         call_id));
        //     log_call_dump(call_id);
        // }
    }
    else
    {
        if (/*app_cfg.duration != PJSUA_APP_NO_LIMIT_DURATION && */
            ci.state ==  PJSIP_INV_STATE_CONFIRMED)
        {
            // Schedule timer to hangup call after duration
            app_call_data *cd = &app_cfg.call_data[call_id];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            pj_time_val delay;

            cd->timer.id = call_id;
            delay.sec = app_cfg.duration;
            delay.msec = 0;
            pjsip_endpt_schedule_timer(endpt, &cd->timer, &delay);
        }

        if (ci.state == PJSIP_INV_STATE_EARLY)
        {
            int code = 0;
            pj_str_t reason;
            pjsip_msg *msg;
            /* This can only occur because of TX or RX message */
            pj_assert(e->type == PJSIP_EVENT_TSX_STATE);
            if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
            {
                msg = e->body.tsx_state.src.rdata->msg_info.msg;
            } 
            else 
            {
                msg = e->body.tsx_state.src.tdata->msg;
            }
            code = msg->line.status.code;
            reason = msg->line.status.reason;

            /* Start ringing for 180 */
            if (ci.role == PJSIP_ROLE_UAC && code == 180 && 
                msg->body == NULL && ci.media_status == PJSUA_CALL_MEDIA_NONE)
            {
                ring_start(call_id);
            }

            PJ_LOG(3,(THIS_FILE, "Call %d state changed to %.*s (%d %.*s)", 
              call_id, (int)ci.state_text.slen, 
                      ci.state_text.ptr, code, 
                      (int)reason.slen, reason.ptr));
        }
        else
        {
            PJ_LOG(3,(THIS_FILE, "Call %d state changed to %.*s", 
              call_id,
              (int)ci.state_text.slen,
              ci.state_text.ptr));
        }
        // if (current_call == PJSUA_INVALID_ID)
        // current_call = call_id;
    }
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pj_status_t status;
    app_cfg.call_id = call_id;
    pjsua_call_get_info(app_cfg.call_id, &app_cfg.ci);

    PJ_LOG(3, (THIS_FILE, "Media status changed %d", app_cfg.ci.media_status));
    ring_stop(call_id);

    // if (app_cfg.ci.media_status == PJSUA_CALL_MEDIA_ACTIVE)
    // {
    //     // When media is active, connect call to sound device.
    //     status = answ_phone_play_lbeep();
    //     if (status != PJ_SUCCESS)
    //     {
    //         pjsua_perror(THIS_FILE, "Error in play_lbeep", status);
    //     }
    //     //answ_phone_play_msg(app_cfg.all_id);
    // }

    //pj_pool_release(app_cfg.pool);
    PJ_LOG(3, (THIS_FILE,   "pName: %s, "
                            "pCap AF_RELEASE: %u, " 
                            "pBlockSize: %u",
                            app_cfg.pool->obj_name, 
                            app_cfg.pool->capacity,
                            app_cfg.pool->increment_size));
}

static void call_timeout_callback(pj_timer_heap_t *timer_heap, 
                                struct pj_timer_entry *entry)
{
    pjsua_call_id call_id = entry->id;
    pjsua_msg_data msg_data_;
    pjsip_generic_string_hdr warn;
    pj_str_t hname = pj_str("Warning");
    pj_str_t hvalue = pj_str("399 pjsua \"Call duration exceeded\"");

    PJ_UNUSED_ARG(timer_heap);

    if (call_id == PJSUA_INVALID_ID) {
    PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
    return;
    }
    
    /* Add warning header */
    pjsua_msg_data_init(&msg_data_);
    pjsip_generic_string_hdr_init2(&warn, &hname, &hvalue);
    pj_list_push_back(&msg_data_.hdr_list, &warn);

    /* Call duration has been exceeded; disconnect the call */
    PJ_LOG(3,(THIS_FILE, "Duration (%d seconds) has been exceeded "
             "for call %d, disconnecting the call",
             app_cfg.duration, call_id));
    entry->id = PJSUA_INVALID_ID;
    pjsua_call_hangup(call_id, 200, NULL, &msg_data_);
}

static void ring_start(pjsua_call_id call_id)
{
    if (app_cfg.no_tones) return;

    if (app_cfg.call_data[call_id].ring_on) return;

    app_cfg.call_data[call_id].ring_on = PJ_TRUE;

    pjsua_conf_connect(app_cfg.ring_slot, 0);
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
                                        160, 16, 0, &app_cfg.ring_port);
        if (status != PJ_SUCCESS)
        {
            error_exit("Error in init_ring() tone create", status);
        }

        /* describe tone */
        tone[0].freq1 = 850;
        tone[0].freq2 = 100;
        tone[0].on_msec = 500;
        tone[0].off_msec = 1000;
        tone[0].volume = PJMEDIA_TONEGEN_VOLUME;
        tone[0].flags = 0;

        status = pjmedia_tonegen_play(app_cfg.ring_port, 1, tone, 
                                    PJMEDIA_TONEGEN_LOOP);
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