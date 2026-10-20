/*
 * Host-side test for csi_motion (no ESP32 needed).
 *
 * Build and run from the project root:
 *   gcc -O2 -Wall -Wextra -o /tmp/test_motion tools/test_motion.c esp32/main/csi_motion.c -Iesp32/main -lm
 *   /tmp/test_motion
 *
 * Simulates 64 subcarriers: a static room with small noise, then "motion"
 * (strong random per-packet perturbation), then a static room again.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "csi_motion.h"

#define N 64

static double frand(void) {
    return (double)rand() / (double)RAND_MAX;
}

/* One synthetic CSI packet. `perturb` = relative amplitude disturbance (0 = still). */
static void make_packet(int8_t* out, double perturb) {
    for(int k = 0; k < N; k++) {
        double base = 22.0 + 10.0 * sin(k / 6.0);
        double a = base * (1.0 + 0.01 * (frand() - 0.5) * 2.0); /* 1% noise */
        if(perturb > 0.0) a *= 1.0 + perturb * (frand() - 0.5) * 2.0;
        double ph = k * 0.3 + 0.02 * (frand() - 0.5);
        out[2 * k] = (int8_t)lround(a * sin(ph));     /* imag */
        out[2 * k + 1] = (int8_t)lround(a * cos(ph)); /* real */
    }
}

static int failures = 0;
#define CHECK(cond, msg)                                  \
    do {                                                  \
        if(!(cond)) {                                     \
            printf("  FAIL: %s\n", msg);                  \
            failures++;                                   \
        } else {                                          \
            printf("  ok:   %s\n", msg);                  \
        }                                                 \
    } while(0)

int main(void) {
    srand(1234);
    csi_motion_t m;
    csi_motion_init(&m);
    int8_t pkt[2 * N];

    float max_still = 0.0f;
    int moving_during_still = 0;

    printf("Phase 1: still room, 400 packets\n");
    for(int i = 0; i < 400; i++) {
        make_packet(pkt, 0.0);
        float s = csi_motion_update(&m, pkt, sizeof(pkt), 0);
        if(i > 100) {
            if(s > max_still) max_still = s;
            if(csi_motion_state(&m) == CSI_STATE_MOVING) moving_during_still++;
        }
    }
    printf("  max score while still: %.4f (threshold %.3f)\n", max_still, m.threshold);
    CHECK(max_still < m.threshold, "still score stays below threshold");
    CHECK(moving_during_still == 0, "no false MOVING while still");
    CHECK(csi_motion_state(&m) == CSI_STATE_STILL, "state is STILL after settling");

    printf("Phase 2: motion, 100 packets\n");
    float max_move = 0.0f;
    int saw_moving = 0;
    for(int i = 0; i < 100; i++) {
        make_packet(pkt, 0.35);
        float s = csi_motion_update(&m, pkt, sizeof(pkt), 0);
        if(s > max_move) max_move = s;
        if(csi_motion_state(&m) == CSI_STATE_MOVING) saw_moving = 1;
    }
    printf("  max score during motion: %.4f\n", max_move);
    CHECK(max_move > m.threshold, "motion score exceeds threshold");
    CHECK(saw_moving, "state reaches MOVING");

    printf("Phase 3: still again, 200 packets\n");
    for(int i = 0; i < 200; i++) {
        make_packet(pkt, 0.0);
        csi_motion_update(&m, pkt, sizeof(pkt), 0);
    }
    CHECK(csi_motion_state(&m) == CSI_STATE_STILL, "state returns to STILL");

    printf("Phase 4: calibration command\n");
    csi_motion_calibrate(&m, 50);
    CHECK(csi_motion_state(&m) == CSI_STATE_CALIBRATING, "state is CALIBRATING after CAL");
    for(int i = 0; i < 60; i++) {
        make_packet(pkt, 0.0);
        csi_motion_update(&m, pkt, sizeof(pkt), 0);
    }
    CHECK(csi_motion_state(&m) == CSI_STATE_STILL, "calibration finishes, back to STILL");

    printf("Phase 5: skip=4 (first_word_invalid) with garbage in the first 4 bytes\n");
    csi_motion_init(&m);
    int8_t pkt2[2 * N];
    float max5 = 0.0f;
    for(int i = 0; i < 300; i++) {
        make_packet(pkt, 0.0);
        pkt2[0] = (int8_t)(rand() % 255 - 127);
        pkt2[1] = (int8_t)(rand() % 255 - 127);
        pkt2[2] = (int8_t)(rand() % 255 - 127);
        pkt2[3] = (int8_t)(rand() % 255 - 127);
        for(int j = 4; j < 2 * N; j++) pkt2[j] = pkt[j];
        float s = csi_motion_update(&m, pkt2, sizeof(pkt2), 4);
        if(i > 100 && s > max5) max5 = s;
    }
    printf("  max score with skip: %.4f\n", max5);
    CHECK(max5 < m.threshold, "skipping invalid first word keeps still score low");

    printf("\n%s\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
