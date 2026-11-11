/* Independent actual-controller + actual-engine tests (sharp only). No live MIDI or disk. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/modes/plugin/screenmirror.c"

static uint64_t test_time;
static unsigned short physical[256*400];
static unsigned panel=400,usb_up=1;
static unsigned in_display,in_main,sends,image_packets,complete_images;
static unsigned last_send_done,have_sent,prepares;
static unsigned last_image_session;
static unsigned last_image_fragment,last_image_count,telemetry_count,telemetry_captures;
static int persisted_send,persisted_screen;
static void (*registered_main)(void);
static void (*registered_setting)(const char *,int);
static unsigned inject_stage;
static struct sm_pixel_storage allocated_pixels __attribute__((aligned(32)));
static unsigned allocations,allocation_fail,sdk_calls_after_allocation;

void *opus_alloc(struct opl_self *self,unsigned bytes) {
    (void)self;
    assert(++allocations==1u && !in_display && !in_main);
    assert(bytes==560008u && bytes==sizeof(allocated_pixels));
    assert(((uintptr_t)&allocated_pixels&31u)==0u);
    if(allocation_fail)return 0;
    /* Simulate the documented zero-filled, 32-aligned pool allocation, starting
     * from poisoned backing memory rather than relying on static C startup. */
    memset(&allocated_pixels,0,sizeof(allocated_pixels));
    return &allocated_pixels;
}
static void allocation_intact(void) {
    assert(allocations==1u && !allocation_fail && sm_pixels==&allocated_pixels);
}
static void sdk_after_allocation(void) {
    allocation_intact();sdk_calls_after_allocation++;
}

unsigned screenmirror_test_cycles(void) {return (unsigned)test_time;}
int opus_setting(struct opl_self *self,const char *name) {
    /* 0.17 reads only "send"; a "mode" persisted by 0.16 is never consulted. */
    (void)self;sdk_after_allocation();
    if(!strcmp(name,"screen"))return persisted_screen;
    assert(!strcmp(name,"send"));return persisted_send;
}
void opus_on_setting(struct opl_self *self,void (*fn)(const char *,int)) {
    (void)self;sdk_after_allocation();assert(fn);registered_setting=fn;
}
void opus_on_main_pass(struct opl_self *self,void (*fn)(void)) {
    (void)self;sdk_after_allocation();assert(fn);registered_main=fn;
}
void opus_on_key_after(struct opl_self *self,void (*fn)(int,int)) {(void)self;sdk_after_allocation();assert(fn);}
void opus_on_knob_after(struct opl_self *self,void (*fn)(int,short)) {(void)self;sdk_after_allocation();assert(fn);}
void opus_clock(struct opus_clock *clock) {
    sdk_after_allocation();
    memset(clock,0,sizeof(*clock));clock->second=(unsigned char)(test_time/400000000u%60u);
}
int fw_usb_configured(void) {return (int)usb_up;}
static unsigned read16(const unsigned char *p) {return (unsigned)p[0]|(unsigned)p[1]<<8;}
static unsigned read32(const unsigned char *p) {
    return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;
}
static unsigned wire_crc(const unsigned char *p,unsigned n) {
    unsigned c=65535u,i;
    while(n--) {c^=(unsigned)*p++<<8;for(i=0;i<8;i++)c=((c<<1)^((c&32768u)?0x1021u:0u))&65535u;}
    return c;
}
void fw_midi_send(int cable,const unsigned *events,int count) {
    unsigned char wire[100],raw[80];unsigned w=0,r=0,i,j,n,acc=0,bits=0,session,width,height;
    (void)cable;allocation_intact();assert(in_main && !in_display && usb_up && count>0 && count<=31);
    if(have_sent && sm_burst==1u)assert((unsigned)(screenmirror_test_cycles()-last_send_done)>=2000000u);
    for(i=0;i<(unsigned)count;i++) {
        n=(events[i]&15u)==4u?3u:(events[i]&15u)-4u;
        assert(n>=1u && n<=3u);
        for(j=0;j<n;j++)wire[w++]=(unsigned char)(events[i]>>(8u+8u*j));
    }
    assert(w<=92u && w>=7u && wire[0]==240u && wire[1]==125u && wire[2]=='S' && wire[3]=='M' && wire[w-1u]==247u);
    for(i=5;i<w-1u;i++) {
        assert(wire[i]<128u);acc|=(unsigned)wire[i]<<bits;bits+=7u;
        while(bits>=8u) {raw[r++]=(unsigned char)acc;acc>>=8;bits-=8u;}
    }
    assert(r>=6u && r<=75u && !acc && wire_crc(raw,r-2u)==read16(raw+r-2u));
    if(wire[4]==4u) {
        image_packets++;session=read32(raw);assert(session);
        width=read16(raw+11u);height=read16(raw+13u);
        assert(width==panel && height==width/2u); /* Sharp: physical panel, never scaled. */
        last_image_session=session;
        last_image_fragment=read16(raw+6u);last_image_count=read16(raw+8u);
        assert(raw[10]!=5u && raw[10]!=7u); /* Every tested image needs a complete base. */
        if(read16(raw+6u)+1u==read16(raw+8u))complete_images++;
    } else {
        assert(wire[4]==3u && r==75u && raw[6]==254u && raw[9]==3u && raw[10]==0u && raw[11]==17u && raw[12]==0u);
        telemetry_count++;telemetry_captures=read32(raw+25u); /* Captures actually performed. */
    }
    test_time+=80000u;last_send_done=screenmirror_test_cycles();have_sent=1u;sends++;
}
void screenmirror_test_prepare_cost(void) {prepares++;test_time+=3200000u;}
static void pass(void) {
    unsigned before=prepares;
    allocation_intact();
    test_time+=2000000u;in_main++;registered_main();in_main--;
    assert(prepares-before<=1u);
}
static void capture_frame(unsigned value,unsigned advance) {
    unsigned x,y,before=sends,injecting=inject_stage,random=0x517abc31u;
    allocation_intact();
    memset(physical,0,sizeof(physical));
    for(y=0;y<panel/2u;y++)for(x=0;x<panel;x++) {
        unsigned short pixel=(unsigned short)value;
        if(value==0xffffu) {
            random^=random<<13;random^=random>>17;random^=random<<5;pixel=(unsigned short)random;
        }
        physical[256u*(panel-1u-x)+y]=pixel;
    }
    if(advance)test_time+=40000000u;
    in_display++;
    screenmirror_capture_flush(physical,physical+256u*panel,0);in_display--;
    assert(sends==before || injecting); /* Injected main can legally send telemetry. */
}
static void capture(unsigned value) {capture_frame(value,1u);}
static void await_complete(void) {
    unsigned before=complete_images,i=0;
    while(complete_images==before) {pass();assert(++i<10000u);}
}
static void await_telemetry(void) {
    unsigned before=telemetry_count,i=0;
    while(telemetry_count==before) {pass();assert(++i<10000u);}
}
static void await_packet(void) {
    unsigned before=image_packets,i=0;
    while(image_packets==before) {pass();assert(++i<10000u);}
}
void screenmirror_test_interleave(unsigned stage) {
    if(stage!=inject_stage)return;
    inject_stage=0u;
    /* Publish a newer snapshot between reserve announcement and recheck. */
    if(stage==2u)capture(0x0821u);
}
int main(int argc,char **argv) {
    unsigned test,i,before,old_session,old_token;
    assert(argc==3);test=(unsigned)strtoul(argv[1],0,10);persisted_send=(int)strtol(argv[2],0,10);
    allocation_fail=test==8u;persisted_screen=test==9u?1:0; /* Test 9 boots with the mirror off. */
    memset(&allocated_pixels,0xa5,sizeof(allocated_pixels));
    if(allocation_fail) {
        assert(opl_init(0)==-1 && allocations==1u && !sdk_calls_after_allocation);
        assert(!registered_main && !registered_setting && !sends && !sm_pixels && active_token==3u);
        /* A failed initialization must leave capture disabled, even if the
         * entry is invoked independently of the core's uninstalled hook. */
        screenmirror_capture_flush(physical,physical+256u*panel,0);
        assert(!sends && !sm_pixels);
        for(i=0;i<sizeof(allocated_pixels);i++)assert(((unsigned char *)&allocated_pixels)[i]==0xa5u);
        puts("PASS allocation failure refuses init before SDK callbacks and leaves capture disabled");
        return 0;
    }
    assert(opl_init(0)==0);allocation_intact();
    for(i=0;i<sizeof(allocated_pixels);i++)assert(((unsigned char *)&allocated_pixels)[i]==0u);
    assert(registered_main && registered_setting && !sends && active_token==((1u<<2)|SM_SHARP));
    assert(sm_burst==(persisted_send>=0 && persisted_send<=3?(unsigned)persisted_send+1u:4u));
    assert(sm_screen_on==(test==9u?0u:1u));
    /* Display captures only after main has seen a configured USB host. */
    capture(0xffffu);for(i=0;i<20u;i++)pass();assert(!image_packets);
    if(test!=9u)assert(telemetry_count && !telemetry_captures); /* That capture did no pixel work. */
    if(test==9u) {
        /* Off from boot: no capture and no MIDI at all, not even telemetry. */
        assert(!sends);
        for(i=0;i<5u;i++) {capture(0xffffu);pass();}
        assert(!sends);
        registered_setting("screen",0);assert(sm_screen_on==1u);
        for(i=0;i<20u;i++)pass();assert(!image_packets);
        await_telemetry();assert(!telemetry_captures); /* Nothing was captured while off. */
        capture(0x0821u);await_complete();
        puts("PASS mirror off at boot sends nothing; turning it on needs a fresh capture");
        allocation_intact();assert(sends && image_packets);
        return 0;
    }
    if(test==1u) {
        capture(0x0821u);await_complete();old_session=last_image_session;
        /* A leftover 0.16 "mode" setting and unrelated names change nothing. */
        old_token=active_token;before=sm_burst;
        for(i=0;i<5u;i++)registered_setting("mode",(int)i);
        registered_setting("other",1);registered_setting("send_extra",2);
        registered_setting(0,2);registered_setting("",2);registered_setting("s",2);registered_setting("sen",2);
        registered_setting("scree",1);registered_setting("screens",1);registered_setting("screen_extra",1);
        assert(sm_screen_on==1u);
        assert(active_token==old_token && sm_burst==before);
        capture(0x1111u);await_complete();assert(last_image_session==old_session);
        puts("PASS persisted send rate/default/range; mode and other setting names ignored");
    } else if(test==2u) {
        /* Live send-rate changes keep the session and the token. */
        capture(0x0821u);await_complete();old_session=last_image_session;old_token=active_token;
        for(i=0;i<6u;i++) {
            registered_setting("send",(int)i-1);
            assert(sm_burst==(i>=1u && i<=4u?i:4u));
            capture(i%2u?0x0821u:0xffffu);await_complete(); /* Each image differs from the last. */
            assert(last_image_session==old_session && active_token==old_token);
        }
        puts("PASS live send-rate changes keep the session");
    } else if(test==3u) {
        /* A different physical panel geometry starts a new session at the new width. */
        capture(0x0821u);await_complete();old_session=last_image_session;
        panel=320u;capture(0x1111u);await_complete();
        assert(last_image_session!=old_session);old_session=last_image_session;
        panel=376u;capture(0x2222u);await_complete();
        assert(last_image_session!=old_session);
        puts("PASS panel geometry change opens a new session at native width");
    } else if(test==4u) {
        capture(0xffffu);inject_stage=2u;
        for(i=0;i<10000u && !complete_images;i++)pass();assert(complete_images && !inject_stage);
        puts("PASS reserve race retries using newest tagged capture");
    } else if(test==5u) {
        capture(0xffffu);await_packet();before=sends;usb_up=0u;
        for(i=0;i<8u;i++)pass();assert(sends==before);
        capture(0x0821u); /* Skipped: no pixel work without a USB host. */
        for(i=0;i<8u;i++)pass();assert(sends==before);
        usb_up=1u;before=image_packets;
        for(i=0;i<20u;i++)pass();assert(image_packets==before);
        await_telemetry();assert(telemetry_captures==1u); /* Only the capture made before the disconnect. */
        capture(0x0821u);await_complete();
        puts("PASS USB disconnect skips capture, waits and fully repairs");
    } else if(test==6u) {
        /* Capture is throttled to one snapshot per 100 ms. */
        capture(0x0821u);await_complete();before=image_packets;
        capture_frame(0xffffu,0u);for(i=0;i<20u;i++)pass();assert(image_packets==before);
        capture(0xffffu);await_complete();assert(image_packets>before);
        puts("PASS capture throttle holds snapshots inside 100 ms");
    } else if(test==7u) {
        capture(0xffffu);await_packet();
        test_time=(UINT64_C(1)<<32)-2000000u;pass();
        capture_frame(0x0821u,0u);await_complete();
        puts("PASS USB pacing survives CYCLES wrap");
    } else if(test==10u) {
        /* Live off/on: off stops everything; on opens a new session from a fresh capture. */
        capture(0x0821u);await_complete();old_session=last_image_session;
        capture(0xffffu);await_packet();
        registered_setting("screen",1);assert(sm_screen_on==0u);
        pass();before=sends;
        for(i=0;i<5u;i++) {capture(i%2u?0x0821u:0xffffu);pass();}
        for(i=0;i<20u;i++)pass();assert(sends==before);
        registered_setting("screen",0);before=image_packets;
        for(i=0;i<20u;i++)pass();assert(image_packets==before);
        await_telemetry();assert(telemetry_captures==2u); /* Only the two captures made while on. */
        capture(0x1111u);await_complete();assert(last_image_session!=old_session);
        puts("PASS live mirror off sends nothing; on resumes in a new session");
    } else assert(0 && "unknown test");
    allocation_intact();assert(sends && image_packets && !in_display && !in_main);
    return 0;
}
