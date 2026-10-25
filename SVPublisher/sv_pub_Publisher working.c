#include <libiec61850/sv_publisher.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define PI 3.14159265358979323846
#define SAMPLE_RATE 80           // Samples per cycle
#define FREQUENCY 50             // Hz
#define TOTAL_SAMPLES 4000       // Samples per second
#define INTERVAL_NS 250000       // 250us in nanoseconds
#define V_SCALE 100              // 10mV
#define I_SCALE 1000             // 1mA

int main() {
    printf("Starting High-Precision SV Publisher (50Hz)...\n");
    
    double v_rms = 110.0 *1000;  //Primary   VTR = 1000
    double i_rms = 1.0 * 900; // Primary      CTR = 900:1

    SVPublisher publisher = SVPublisher_create(NULL, "eth0");
    if (publisher == NULL) {
        printf("Error: Could not create publisher. Try: sudo ./program\n");
        return -1;
    }

    SVPublisher_ASDU asdu = SVPublisher_addASDU(publisher, "AA1E1Q04MU_SMVID", NULL, 1);

    /* Add 16 entries: 8 for Values and 8 for Qualities interleaved */
    for (int i = 0; i < 16; i++) {
        SVPublisher_ASDU_addINT32(asdu);
    }

    SVPublisher_setupComplete(publisher);

    uint16_t smpCnt = 0;
    double angle = 0;
    uint32_t qGood = 0;

    const double v_amplitude = V_SCALE * v_rms * 1.41421356; 
    const double i_amplitude = I_SCALE * i_rms * 1.41421356; 

    // Timing structures
    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);

    while (1) {
        /* 1. Pre-calculate values (Pre-trigger) */
        int32_t vA = (int32_t)(v_amplitude * sin(angle));
        int32_t vB = (int32_t)(v_amplitude * sin(angle - 2.0 * PI / 3.0));
        int32_t vC = (int32_t)(v_amplitude * sin(angle + 2.0 * PI / 3.0));
        int32_t vN = 0;

        int32_t iA = (int32_t)(i_amplitude * sin(angle));
        int32_t iB = (int32_t)(i_amplitude * sin(angle - 2.0 * PI / 3.0));
        int32_t iC = (int32_t)(i_amplitude * sin(angle + 2.0 * PI / 3.0));
        int32_t iN = 0;

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

        

        SVPublisher_ASDU_setSmpCnt(asdu, smpCnt++);
        if (smpCnt >= TOTAL_SAMPLES) smpCnt = 0;

        /* 3. Wait for the precise 250us mark */
        next_tick.tv_nsec += INTERVAL_NS;
        if (next_tick.tv_nsec >= 1000000000) {
            next_tick.tv_nsec -= 1000000000;
            next_tick.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL);

        /* 4. FIRE (Publish instantly) */
        SVPublisher_publish(publisher);

        /* Update state */
        angle += (2.0 * PI) / (double)SAMPLE_RATE;
        if (angle >= 2.0 * PI) angle -= 2.0 * PI;

        if (smpCnt % 800 == 0) {
            printf("[TX] 50Hz Stable | Va: %d | Ia: %d\n", vA, iA);
        }
    }

    SVPublisher_destroy(publisher);
    return 0;
}
