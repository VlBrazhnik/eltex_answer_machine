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

    //init tonegen for dial tone and ring
    answ_phone_init_ring();
    answ_phone_init_dial_tone();
    answ_phone_init_aud_player();

    /* init all call_id as INVALID */
    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        app_cfg.call_id[i] = PJSUA_INVALID_ID;
    }

    //player

    //tonegen ring

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

        if (choice[0] == 'q')
        {
            PJ_LOG(3, (THIS_FILE, "Quitting"));
            break;
        }
    }

    // status = pjsua_player_destroy(app_cfg.aud_player_id);
    // if (status != PJ_SUCCESS)
    // {
    //     error_exit("Error in pjsua_player_destroy", status);
    // }

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

    status = pjsua_create();
    if (status != PJ_SUCCESS)
    {
        pjsua_perror(THIS_FILE, "Error initiate pjsua!", status);
        return status;
    }

    app_cfg.pool = pjsua_pool_create("app_pool", 1000, 1000);
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

    //set max calls 
    ua_cfg.max_calls = MAX_CALLS;

    //set callback 
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
    cfg.cred_info[0].scheme = pj_str("call");
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

static void answ_phone_init_ring(void)
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

static void answ_phone_play_ring(pjsua_call_id call_id)
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

    /* set arg to void */
    PJ_UNUSED_ARG(acc_id); 
    PJ_UNUSED_ARG(rdata);

    /* add incoming call_id to array call_ids */
    for (pj_int32_t i = 0; i < MAX_CALLS; i++)
    {
        /* add call_id if array call id is empty */
        if (app_cfg.call_id[i] == PJSUA_INVALID_ID)
        {
            app_cfg.call_id[i] = call_id;
            current_call = app_cfg.call_id[i];
            pjsua_call_get_info(current_call, &app_cfg.ci[current_call]);
            PJ_LOG(3,(THIS_FILE,
                  "Incoming call for account %d! "
                  "Media count: %d audio!"
                  LOG_TAB"From: %.*s "
                  LOG_TAB"To: %.*s ",
                  current_call,
                  app_cfg.ci[current_call].rem_aud_cnt,
                  (int)app_cfg.ci[current_call].remote_info.slen,
                  app_cfg.ci[current_call].remote_info.ptr,
                  (int)app_cfg.ci[current_call].local_info.slen,
                  app_cfg.ci[current_call].local_info.ptr));

            status = pjsua_call_answer(current_call, 180, NULL, NULL);
            if (status != PJ_SUCCESS)
                error_exit("Error call_answer_180", status);
            answ_phone_delay_answer(current_call);
            break;
        }
    }
    /* check for out of calls array */
    /* get info about current call */
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    PJ_UNUSED_ARG(e);
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        // /* cancel answer timer */
        // if (app_cfg.call_data[call_id].answer_timer.id != PJSUA_INVALID_ID)
        // {
        //     app_call_data *cd = &app_cfg.call_data[call_id ];
        //     pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
        //     cd->answer_timer.id = PJSUA_INVALID_ID;
        //     pjsip_endpt_cancel_timer(endpt, &cd->answer_timer);
        // }
        // /* cancel release time */
        // if (app_cfg.call_data[call_id].release_timer.id != PJSUA_INVALID_ID)
        // {
        //     app_call_data *cd = &app_cfg.call_data[call_id];
        //     pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
        //     cd->release_timer.id = PJSUA_INVALID_ID;
        //     pjsip_endpt_cancel_timer(endpt, &cd->release_timer);
        // }

        PJ_LOG(3, (THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]",
            call_id, 
            ci.last_status,
            (int)ci.last_status_text.slen,
            ci.last_status_text.ptr));
    }
}

static void on_call_media_state(pjsua_call_id call_id)
{

    for (pj_int32_t i = 0; i < MAX_CALLS && 
        (app_cfg.call_id[i] != PJSUA_INVALID_ID); i++)
    {
        if (app_cfg.call_id[i] == call_id)
        {
            PJ_LOG(3, (THIS_FILE, "Media status changed %d", 
                        app_cfg.ci[i].media_status));
            // answ_phone_play_dial_tone(call_id);
            answ_phone_play_aud_msg(call_id);
            break;
        }
    }

    // When media is active, connect call to sound device.
    //answ_phone_play_ring();
    // answ_phone_play_dial_tone(call_id);
    // //answ_phone_play_aud_msg();

    // PJ_LOG(3, (THIS_FILE,   "pName: %s, "
    //                         "pCap AF_RELEASE: %u, " 
    //                         "pBlockSize: %u",
    //                         app_cfg.pool->obj_name, 
    //                         app_cfg.pool->capacity,
    //                         app_cfg.pool->increment_size));
}

static void answ_phone_delay_answer(pjsua_call_id call_id)
{
    pj_status_t status;
    pj_time_val delay;
    app_cfg.duration_ms = PJSUA_DELAY_TIME_MS;

    app_cfg.call_data[call_id].answer_timer.id = PJSUA_INVALID_ID;
    app_cfg.call_data[call_id].answer_timer.cb = &answer_timer_cb;
    
    delay.sec = 0;
    delay.msec = app_cfg.duration_ms;
    pj_time_val_normalize(&delay);

    app_call_data *cd = &app_cfg.call_data[call_id];
    cd->answer_timer.id = call_id;

    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    status = pjsip_endpt_schedule_timer(endpt, &cd->answer_timer, &delay);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error answer scheduler", status);
    }
}

static void answer_timer_cb(pj_timer_heap_t *h, pj_timer_entry *entry)
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

    status = pjsua_call_answer(call_id, 200, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error call_answer_200() ", status);
    }

    answer_phone_release_answer(call_id);
}

static void answer_phone_release_answer(pjsua_call_id call_id)
{
    pj_status_t status;
    pj_time_val delay;
    app_cfg.duration_ms = PJSUA_RELEASE_TIME_MS;

    for (pj_int32_t i = 0; i < PJ_ARRAY_SIZE(app_cfg.call_data); i++)
    {
        app_cfg.call_data[i].release_timer.id = PJSUA_INVALID_ID;
        app_cfg.call_data[i].release_timer.cb = &answer_release_cb;
    }

    delay.sec = 0;
    delay.msec = app_cfg.duration_ms;
    pj_time_val_normalize(&delay);
 
    app_call_data *cd = &app_cfg.call_data[call_id];
    cd->release_timer.id = call_id;

    pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
    status = pjsip_endpt_schedule_timer(endpt, &cd->release_timer, &delay);
    if (status != PJ_SUCCESS)
    {
        error_exit("Error scheduler", status);
    }
}

static void answer_release_cb(pj_timer_heap_t *h, pj_timer_entry *entry)
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

    entry->id = PJSUA_INVALID_ID;

    PJ_LOG(3, (THIS_FILE, "Call %d is terminated", call_id));
    status = pjsua_call_hangup(call_id, 200, &reason, NULL);
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