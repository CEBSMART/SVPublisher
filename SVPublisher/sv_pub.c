#include <libiec61850/sv_publisher.h>
#include <MQTTClient.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constants
#define PI 3.14159265358979323846
#define SAMPLE_RATE 80           
#define TOTAL_SAMPLES 4000       
#define INTERVAL_NS 250000       
#define V_SCALE 100              
#define I_SCALE 1000             

// MQTT Settings
#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "SV_Publisher_Pi"

// Global Control Variables (Volatile for thread safety)
volatile int g_run = 0;
volatile double g_vA_rms = 110000.0, g_vB_rms = 110000.0, g_vC_rms = 110000.0;
volatile double g_iA_rms = 900.0,    g_iB_rms = 900.0,    g_iC_rms = 900.0;
volatile double g_phiIa = 0.0, g_phiIb = -120.0, g_phiIc = 120.0;
char g_svid[64] = "AA1E1Q04MU_SMVID";

// Add a flag to trigger a reset
volatile int g_needs_update = 0;

// MQTT Message Callback
int on_message(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char* payload = (char*)message->payload;
    double val = atof(payload);

    int len = message->payloadlen;
    
    printf("MQTT [Topic: %s] | Value: %.*s\n", topicName, len, payload);

    if (strcmp(topicName, "sv/control/state") == 0) {
        if (strncmp(payload, "START", 5) == 0) g_run = 1;
        else if (strncmp(payload, "STOP", 4) == 0) g_run = 0;
    }
    else if (strcmp(topicName, "sv/va/rms") == 0) g_vA_rms = val;
    else if (strcmp(topicName, "sv/vb/rms") == 0) g_vB_rms = val;
    else if (strcmp(topicName, "sv/vc/rms") == 0) g_vC_rms = val;
    else if (strcmp(topicName, "sv/ia/rms") == 0) g_iA_rms = val;
    else if (strcmp(topicName, "sv/ib/rms") == 0) g_iB_rms = val;
    else if (strcmp(topicName, "sv/ic/rms") == 0) g_iC_rms = val;
    else if (strcmp(topicName, "sv/ia/phi") == 0) g_phiIa = val;
    else if (strcmp(topicName, "sv/control/svid") == 0){
         strncpy(g_svid, payload, 63); 
         g_needs_update = 1; // Trigger the reset in the main loop
     }
    

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main() {
    printf("Initializing SV Publisher with MQTT Control...\n");

    // 1. Setup MQTT
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, on_message, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker.\n");
        return -1;
    }
    // Subscribe to all control topics
    MQTTClient_subscribe(client, "sv/control/#", 1);
    MQTTClient_subscribe(client, "sv/+/rms", 1);
    MQTTClient_subscribe(client, "sv/+/phi", 1);

    // 2. Setup SV Publisher
    SVPublisher publisher = SVPublisher_create(NULL, "eth0");
    SVPublisher_ASDU asdu = SVPublisher_addASDU(publisher, g_svid, NULL, 1);

    // Initialize slots to support index 60
    for (int i = 0; i < 16; i++) SVPublisher_ASDU_addINT32(asdu);
    SVPublisher_setupComplete(publisher);

    uint16_t smpCnt = 0;
    double angle = 0;
    uint32_t qGood = 0;

    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);

    printf("Ready. Use Node-RED to send START to sv/control/state\n");

    while (1) {
        
        if (g_needs_update) {
    printf("Updating SVID to: %s. Re-initializing Publisher...\n", g_svid);
    
    // 1. Cleanup existing objects
    SVPublisher_destroy(publisher);
    
    // 2. Re-create Publisher and ASDU with the NEW g_svid
    publisher = SVPublisher_create(NULL, "eth0");
    asdu = SVPublisher_addASDU(publisher, g_svid, NULL, 1);
    
    // 3. Re-add your data slots (the 64 slots we defined earlier)
    for (int i = 0; i < 64; i++) {
        SVPublisher_ASDU_addINT32(asdu);
    }
    
    SVPublisher_setupComplete(publisher);
    
    g_needs_update = 0;
    printf("Publisher Re-initialized successfully.\n");
}
        
        // High-precision math
        int32_t vA = (int32_t)(V_SCALE * g_vA_rms * 1.41421356 * sin(angle));
        int32_t vB = (int32_t)(V_SCALE * g_vB_rms * 1.41421356 * sin(angle - 2.0 * PI / 3.0));
        int32_t vC = (int32_t)(V_SCALE * g_vC_rms * 1.41421356 * sin(angle + 2.0 * PI / 3.0));

        // Current phase angles are dynamic from MQTT
        int32_t iA = (int32_t)(I_SCALE * g_iA_rms * 1.41421356 * sin(angle + (g_phiIa * PI / 180.0)));
        int32_t iB = (int32_t)(I_SCALE * g_iB_rms * 1.41421356 * sin(angle + (g_phiIb * PI / 180.0)));
        int32_t iC = (int32_t)(I_SCALE * g_iC_rms * 1.41421356 * sin(angle + (g_phiIc * PI / 180.0)));

        // Apply your offsets
        SVPublisher_ASDU_setINT32(asdu, 0, vA);   SVPublisher_ASDU_setINT32(asdu, 4, qGood);
        SVPublisher_ASDU_setINT32(asdu, 8, vB);   SVPublisher_ASDU_setINT32(asdu, 12, qGood);
        SVPublisher_ASDU_setINT32(asdu, 16, vC);  SVPublisher_ASDU_setINT32(asdu, 20, qGood);
        SVPublisher_ASDU_setINT32(asdu, 24, 0);   SVPublisher_ASDU_setINT32(asdu, 28, qGood);

        SVPublisher_ASDU_setINT32(asdu, 32, iA);  SVPublisher_ASDU_setINT32(asdu, 36, qGood);
        SVPublisher_ASDU_setINT32(asdu, 40, iB);  SVPublisher_ASDU_setINT32(asdu, 44, qGood);
        SVPublisher_ASDU_setINT32(asdu, 48, iC);  SVPublisher_ASDU_setINT32(asdu, 52, qGood);
        SVPublisher_ASDU_setINT32(asdu, 56, 0);   SVPublisher_ASDU_setINT32(asdu, 60, qGood);

        // Precise Timing
        next_tick.tv_nsec += INTERVAL_NS;
        if (next_tick.tv_nsec >= 1000000000) {
            next_tick.tv_nsec -= 1000000000;
            next_tick.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL);

        if (g_run) {
            SVPublisher_ASDU_setSmpCnt(asdu, smpCnt++);
            if (smpCnt >= TOTAL_SAMPLES) smpCnt = 0;
            SVPublisher_publish(publisher);
        }
        
 

        angle += (2.0 * PI) / (double)SAMPLE_RATE;
        if (angle >= 2.0 * PI) angle -= 2.0 * PI;
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    SVPublisher_destroy(publisher);
    return 0;
}
