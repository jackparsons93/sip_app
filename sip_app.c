#include <pjsua-lib/pjsua.h>
#include <pjmedia/wav_port.h>
#include <stdio.h>

#define THIS_FILE "SIP_APP"

/* Configurations */
#define SIP_USER "416855_123"
#define SIP_DOMAIN "chicago2.voip.ms"
#define SIP_PASSWD "INSERT PASSWORD HERE"
#define SIP_URI "sip:" SIP_USER "@" SIP_DOMAIN
#define WAV_FILE "output.wav" // Path to the WAV file

/* We'll store a global or static pool for WAV usage */
static pj_pool_t *app_pool = NULL;

/* Callback for incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata) {
    printf("Incoming call received on account %d!\n", acc_id);
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback for call state */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e) {
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);
    printf("Call %d state: %s\n", call_id, ci.state_text.ptr);
}

/* Callback for call media state */
static void on_call_media_state(pjsua_call_id call_id) {
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        printf("Media is active\n");

        /* Connect call audio to sound device */
        //pjsua_conf_connect(ci.conf_slot, 0);
        //pjsua_conf_connect(0, ci.conf_slot);

        /* --- Play WAV file over RTP --- */
        pjmedia_port *wav_player_port;
        pj_status_t status = pjmedia_wav_player_port_create(
            app_pool,              /* Our pool (created in main)           */
            WAV_FILE,              /* Path to WAV file                     */
            20,                    /* Frame duration in ms                 */
            PJMEDIA_FILE_NO_LOOP,  /* Play only once                       */
            0,                     /* Default buffer size                  */
            &wav_player_port
        );

        if (status != PJ_SUCCESS) {
            fprintf(stderr, "Failed to create WAV player port: %d\n", status);
            return;
        }

        /* Add the WAV player port to the conference bridge */
        pjsua_conf_port_id wav_port;
        status = pjsua_conf_add_port(app_pool, wav_player_port, &wav_port);
        if (status != PJ_SUCCESS) {
            fprintf(stderr, "Failed to add WAV player to conference bridge: %d\n", status);
            pjmedia_port_destroy(wav_player_port);
            return;
        }

        /* Connect the WAV port to the call's conference slot */
        pjsua_conf_connect(wav_port, ci.conf_slot);
        printf("Playing WAV file: %s\n", WAV_FILE);

        // Optional cleanup after call ends, e.g.:
        // pjsua_conf_remove_port(wav_port);
        // pjmedia_port_destroy(wav_player_port);
    }
}

int main() {
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_transport_config transport_cfg;
    pjsua_acc_config acc_cfg;
    pjsua_acc_id acc_id;
    pj_status_t status;

    /* 1) Initialize PJSUA */
    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Error initializing PJSUA\n");
        return 1;
    }

    /* 2) Configure call callbacks, etc. */
    pjsua_config_default(&cfg);
    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_call_media_state = &on_call_media_state;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 4;

    /* 3) Initialize PJSUA */
    status = pjsua_init(&cfg, &log_cfg, NULL);
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Error initializing PJSUA configurations\n");
        return 1;
    }
    
    /* 4) Disable local audio device */
    status = pjsua_set_null_snd_dev();
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Error setting null sound device"));
        return 1;  // or handle error as needed
    }
    /* 4) Create a pool for media or other allocations */
    app_pool = pjsua_pool_create("my_wav_pool", 4000, 4000);
    if (!app_pool) {
        fprintf(stderr, "Error creating application pool.\n");
        return 1;
    }

    /* 5) Create UDP transport with public IP */
    pjsua_transport_config_default(&transport_cfg);
    transport_cfg.port = 5060;  
    transport_cfg.public_addr = pj_str("3.145.79.228");  // Your public IP

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &transport_cfg, NULL);
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Error creating transport\n");
        return 1;
    }

    /* 6) Start PJSUA */
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Error starting PJSUA\n");
        return 1;
    }

    /* 7) Configure and add SIP account */
    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id = pj_str(SIP_URI);
    acc_cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].realm = pj_str("*");
    acc_cfg.cred_info[0].username = pj_str(SIP_USER);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data = pj_str(SIP_PASSWD);
    acc_cfg.rtp_cfg.port = 4000;      // start port
    acc_cfg.rtp_cfg.port_range = 10;  // e.g. 10 ports reserved
    acc_cfg.rtp_cfg.public_addr = pj_str("3.145.79.228");
    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS) {
        fprintf(stderr, "Error adding account\n");
        return 1;
    }

    printf("SIP account registered: %s\n", SIP_URI);

    /* 8) Wait for user to quit */
    printf("Press Enter to quit...\n");
    getchar();

    /* 9) Destroy PJSUA */
    pjsua_destroy();

    return 0;
}

