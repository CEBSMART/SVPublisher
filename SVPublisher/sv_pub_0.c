#include <libiec61850/sv_publisher.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define PI 3.14159265358979323846
#define SAMPLE_RATE 80           // Samples per cycle
#define FREQUENCY 50             // Hz
#define TOTAL_SAMPLES 4000       // Total samples per second (80 * 50)
#define SLEEP_TIME_US 250        // 1,000,000us / 4000 samples
#define V_SCALE 100              // 10mV
#define I_SCALE 1000             // 1mA

int main() {
    printf("Starting SV Publisher (110V, 1A)...\n");
    
    double v_rms = 110.0 ; //V Secondary - LL 
    double i_rms = 1; //A Secondary - Phase

    /* 1. Create Publisher on eth0 (Change to your Pi's interface if different) */
    SVPublisher publisher = SVPublisher_create(NULL, "eth0");
    if (publisher == NULL) {
        printf("Error: Could not create publisher. Try: sudo ./program\n");
        return -1;
    }

    /* 2. Add ASDU (Standard 9-2LE Setup) */
    SVPublisher_ASDU asdu = SVPublisher_addASDU(publisher, "AA1E1Q04MU_SMVID", NULL, 1);

    /* 3. Add 16 channels: 8 for Values (INT32) and 8 for Qualities */
    for (int i = 0; i < 16; i++) {
        SVPublisher_ASDU_addINT32(asdu);
        //SVPublisher_ASDU_addQuality(asdu);
    }

    SVPublisher_setupComplete(publisher);

    uint16_t smpCnt = 0;
    double angle = 0;
    Quality qGood = 0; // QUALITY_VALIDITY_GOOD;

    // Scaling constants
    
    const double v_scale = V_SCALE * v_rms * 1.4142; 
    const double i_scale = I_SCALE * i_rms * 1.4142; 
    
    

    while (1) {
        /* --- Waveform Math --- */
        int32_t vA = (int32_t)(v_scale * sin(angle));
        int32_t vB = (int32_t)(v_scale * sin(angle - 2.0 * PI / 3.0));
        int32_t vC = (int32_t)(v_scale * sin(angle + 2.0 * PI / 3.0));
        int32_t vN = 0;

        int32_t iA = (int32_t)(i_scale * sin(angle));
        int32_t iB = (int32_t)(i_scale * sin(angle - 2.0 * PI / 3.0));
        int32_t iC = (int32_t)(i_scale * sin(angle + 2.0 * PI / 3.0));
        int32_t iN = 0;
        
        //~ int32_t vA = 15556;
        //~ int32_t vB = -7778; 
        //~ int32_t vC = -7778;
        //~ int32_t vN = 0;

        //~ int32_t iA = 1414;
        //~ int32_t iB = -707;
        //~ int32_t iC = -707;
        //~ int32_t iN = 0;
        
        
        //~ // Force a recognizable pattern: 0x12345678
        //~ int32_t testValue = 0x12345678; 

        //~ // In your loop:
        //~ SVPublisher_ASDU_setINT32(asdu, 0, testValue); SVPublisher_ASDU_setINT32(asdu, 4, qGood);

        /* --- Map Data to ASDU --- */
        // Index: 0=Va, 1=q, 2=Vb, 3=q, 4=Vc, 5=q, 6=Vn, 7=q
        SVPublisher_ASDU_setINT32(asdu, 0, vA);  SVPublisher_ASDU_setINT32(asdu, 4, qGood);
        SVPublisher_ASDU_setINT32(asdu, 8, vB);  SVPublisher_ASDU_setINT32(asdu, 12, qGood);
        SVPublisher_ASDU_setINT32(asdu, 16, vC);  SVPublisher_ASDU_setINT32(asdu, 20, qGood);
        SVPublisher_ASDU_setINT32(asdu, 24, vN);  SVPublisher_ASDU_setINT32(asdu, 28, qGood);

        // Index: 8=Ia, 9=q, 10=Ib, 11=q, 12=Ic, 13=q, 14=In, 15=q
        SVPublisher_ASDU_setINT32(asdu, 32, iA);   SVPublisher_ASDU_setINT32(asdu, 36, qGood);
        SVPublisher_ASDU_setINT32(asdu, 40, iB);  SVPublisher_ASDU_setINT32(asdu, 44, qGood);
        SVPublisher_ASDU_setINT32(asdu, 48, iC);  SVPublisher_ASDU_setINT32(asdu, 52, qGood);
        SVPublisher_ASDU_setINT32(asdu, 56, iN);  SVPublisher_ASDU_setINT32(asdu, 60, qGood);

        /* --- Timing and Publish --- */
        SVPublisher_ASDU_setSmpCnt(asdu, smpCnt++);
        if (smpCnt >= TOTAL_SAMPLES) smpCnt = 0;

        SVPublisher_publish(publisher);

        /* Print to console every 1/10th of a second for monitoring */
        if (smpCnt % 400 == 0) {
            printf("[TX] SmpCnt: %04d | Va: %6d | Ia: %6d\n", smpCnt, vA, iA);
        }

        /* Update angle for next sample */
        angle += (2.0 * PI) / (double)SAMPLE_RATE;
        if (angle >= 2.0 * PI) angle -= 2.0 * PI;

        usleep(SLEEP_TIME_US);
    }

    SVPublisher_destroy(publisher);
    return 0;
}
