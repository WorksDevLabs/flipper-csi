#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "csi_motion.h"
#include "csi_protocol.h"
static void fixture(int8_t* p,int gain) {
    for(int i=0;i<64;i++) { p[2*i]=(int8_t)(gain*(10+i%5)); p[2*i+1]=(int8_t)(gain*(12+i%7)); }
}
static void motion_tests(void) {
    int8_t p[128]; fixture(p,1); csi_motion_t m;
    csi_motion_init(&m);
    for(unsigned i=0;i<400;i++) {
        p[0]=(int8_t)i; p[1]=(int8_t)(255-i); p[2]=(int8_t)(i*3); p[3]=(int8_t)(i*7);
        assert(csi_motion_feed(&m,p,128,i%2 ? 4 : 0,i*50));
    }
    assert(csi_motion_state(&m)==CSI_STATE_STILL);
    assert(m.score<0.00001f); /* invalid bytes never enter the feature vector */
    csi_motion_calibrate(&m,100);
    for(unsigned i=0;i<99;i++) assert(csi_motion_feed(&m,p,128,0,20000+i*50));
    assert(m.cal_left==1 && csi_motion_progress(&m)==99);
    assert(csi_motion_feed(&m,p,128,4,24950));
    assert(csi_motion_state(&m)==CSI_STATE_STILL && csi_motion_progress(&m)==100);
    for(unsigned i=0;i<200;i++) {
        fixture(p,i%2 ? 2 : 1); csi_motion_feed(&m,p,128,0,25000+i*50);
    }
    assert(m.score<0.00001f && !m.moving); /* common gain rejection */
    uint32_t before=m.accepted;
    assert(!csi_motion_feed(&m,NULL,128,0,35000));
    assert(!csi_motion_feed(&m,p,127,0,35000));
    assert(!csi_motion_feed(&m,p,128,2,35000));
    memset(p,0,sizeof(p)); assert(!csi_motion_feed(&m,p,128,0,35000));
    memset(p,127,sizeof(p)); assert(!csi_motion_feed(&m,p,128,0,35000));
    assert(m.accepted==before && m.rejected==5);
    fixture(p,1); csi_motion_feed(&m,p,128,0,40000);
    assert(csi_motion_state(&m)==CSI_STATE_WARMUP && m.score==0);
    csi_motion_calibrate(&m,20);
    csi_motion_feed(&m,p,128,0,41000);
    csi_motion_feed(&m,p,128,0,44000);
    assert(csi_motion_state(&m)==CSI_STATE_WARMUP); /* interrupted calibration invalidated */
    csi_motion_init(&m);
    for(unsigned i=0;i<100;i++) csi_motion_feed(&m,p,128,0,UINT32_MAX-2500+i*50U);
    assert(csi_motion_state(&m)==CSI_STATE_STILL); /* millisecond wrap */
    puts("motion: alternating skip, exact calibration, gain, invalid input, gaps, wrap PASS");
}
static void protocol_tests(void) {
    assert(csi_crc16("123456789",9)==0x29B1);
    const char* payload="CSI2,4294967295,120,-70,2,4294967295,120,50,20,0,100";
    char frame[CSI_LINE_CAP]; assert(csi_frame(frame,sizeof(frame),payload));
    CsiLine line={0}; bool done=false;
    for(size_t i=0;i<strlen(frame);i++) done=csi_line_feed(&line,(uint8_t)frame[i]);
    assert(done); char* p=csi_unframe(line.data); assert(p && !strcmp(p,payload));
    CsiReport r; assert(csi_report_parse(p,&r));
    assert(r.seq==UINT32_MAX && r.score==120 && r.rssi==-70 && r.pkts==UINT32_MAX);
    const char* bad[]={"CSI2,0,0,-70,9,0,120,0,20,0,0","CSI2,0,0,-70,1,0,0,0,20,0,0",
        "CSI2,0,0,-70,1,0,120,0,20,0,0junk","CSI2,0,0,-70,1,0,120,0,20,0,0,1",
        "CSI2,-1,0,-70,1,0,120,0,20,0,0","CSI2,4294967296,0,-70,1,0,120,0,20,0,0",
        "CSI2,0,0,5,1,0,120,0,20,0,0","CSI2,0,,0,1,0,120,0,20,0,0"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(!csi_report_parse(bad[i],&r));
    assert(csi_frame(frame,sizeof(frame),"CAL 100")); frame[2]^=1; frame[strlen(frame)-1]=0;
    assert(!csi_unframe(frame));
    memset(&line,0,sizeof(line));
    for(int i=0;i<300;i++) assert(!csi_line_feed(&line,'x'));
    assert(csi_frame(frame,sizeof(frame),"CAL 100"));
    for(size_t i=0;i<strlen(frame);i++) assert(!csi_line_feed(&line,(uint8_t)frame[i]));
    for(size_t i=0;i<strlen(frame);i++) done=csi_line_feed(&line,(uint8_t)frame[i]);
    assert(done && csi_unframe(line.data)); /* discard whole oversized line, then recover */
    uint32_t n; assert(!csi_uint("100xyz",0,2000,&n)); assert(!csi_uint("-1",0,2000,&n));
    assert(!csi_uint("99999999999999999999",0,UINT32_MAX,&n));
    char hex[100],plain[50]; assert(csi_hex_encode("a b,#*",hex,sizeof(hex)));
    assert(csi_hex_decode(hex,plain,sizeof(plain)) && !strcmp(plain,"a b,#*"));
    assert(!csi_hex_decode("00",plain,sizeof(plain))); assert(!csi_hex_decode("ZZ",plain,sizeof(plain)));
    assert(!csi_hex_decode("1",plain,sizeof(plain)));
    puts("protocol: CRC, bounds, corruption, overflow recovery, hex credentials PASS");
    /* Deterministic malformed-stream stress. CRC+parser must never read out of bounds. */
    uint32_t seed=42;
    for(unsigned i=0;i<200000;i++) {
        seed=seed*1664525U+1013904223U;
        if(csi_line_feed(&line,(uint8_t)(seed>>24))) {
            char* q=csi_unframe(line.data); if(q) (void)csi_report_parse(q,&r);
        }
    }
    puts("protocol: 200000-byte malformed stream PASS");
}
int main(void) { motion_tests(); protocol_tests(); puts("ALL REGRESSIONS PASSED"); }
