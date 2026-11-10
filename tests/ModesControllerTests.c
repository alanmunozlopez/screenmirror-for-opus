/* Independent actual-controller + actual-engines tests. No live MIDI or disk. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/modes/plugin/screenmirror.c"

static uint64_t test_time;
static unsigned short physical[256*400];
static unsigned panel=400,expected_mode,usb_up=1;
static unsigned in_display,in_main,sends,image_packets,complete_images;
static unsigned last_send_done,have_sent,prepares;
static unsigned seen_session,last_image_session,last_native_seq,native_seen;
static unsigned last_image_fragment,last_image_count;
static int persisted_mode;
static void (*registered_main)(void);
static void (*registered_setting)(const char *,int);
static unsigned inject_stage,inject_mode;
static struct sm_pixel_storage allocated_pixels __attribute__((aligned(32)));
static unsigned allocations,allocation_fail,sdk_calls_after_allocation;

void *opus_alloc(struct opl_self *self,unsigned bytes) {
    (void)self;
    assert(++allocations==1u && !in_display && !in_main);
    assert(bytes==640000u && bytes==sizeof(allocated_pixels));
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
    (void)self;sdk_after_allocation();if(!strcmp(name,"send"))return 0;assert(!strcmp(name,"mode"));return persisted_mode;
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
    if(have_sent)assert((unsigned)(screenmirror_test_cycles()-last_send_done)>=2000000u);
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
    if(wire[4]==4u || wire[4]==5u) {
        image_packets++;session=read32(raw);assert(session);
        width=read16(raw+(wire[4]==4u?11u:10u));height=read16(raw+(wire[4]==4u?13u:12u));
        assert(width==(expected_mode==0u?240u:expected_mode==1u?320u:panel) && height==width/2u); /* quality and sharp: physical panel */
        assert(wire[4]==(expected_mode==2u?5u:4u));
        last_image_session=session;
        if(wire[4]==4u) {
            last_image_fragment=read16(raw+6u);last_image_count=read16(raw+8u);
            assert(raw[10]!=5u && raw[10]!=7u); /* Each fresh mode starts with a complete base. */
            if(read16(raw+6u)+1u==read16(raw+8u))complete_images++;
        } else {
            unsigned seq=read16(raw+4u),stripe=raw[6];
            last_image_fragment=raw[7];last_image_count=raw[8];
            assert(raw[9]&128u); /* Each tested mode begins without a usable base. */
            if(seen_session!=session || last_native_seq!=seq) {
                seen_session=session;last_native_seq=seq;native_seen=0u;
            }
            if(raw[7]+1u==raw[8])native_seen|=1u<<stripe;
            if(native_seen==((1u<<((height+7u)/8u))-1u)) {complete_images++;native_seen=0u;}
        }
    } else {
        assert(wire[4]==3u && r==75u && raw[6]==254u && raw[10]==0u && raw[11]==16u && raw[12]==0u);
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
static void choose(int mode) {
    unsigned before=sends,old=active_token;
    registered_setting("mode",mode);assert(sends==before && active_token==old);
    expected_mode=mode>=0 && mode<=3?(unsigned)mode:3u;
    pass();assert((active_token&3u)==expected_mode);
}
static void await_complete(void) {
    unsigned before=complete_images,i=0;
    while(complete_images==before) {pass();assert(++i<10000u);}
}
static void await_packet(void) {
    unsigned before=image_packets,i=0;
    while(image_packets==before) {pass();assert(++i<10000u);}
}
void screenmirror_test_interleave(unsigned stage) {
    if(stage!=inject_stage)return;
    inject_stage=0u;
    if(stage==1u) {
        /* Simulate main selecting a new epoch while the old capture is copied.
         * The resumed old capture may publish, but its tag must be rejected. */
        unsigned display=in_display;in_display=0u;choose((int)inject_mode);in_display=display;
    } else {
        /* Publish a newer snapshot between reserve announcement and recheck. */
        capture(0x0821u);
    }
}
int main(int argc,char **argv) {
    unsigned test,i,before,old_session,old_token;
    assert(argc==3);test=(unsigned)strtoul(argv[1],0,10);persisted_mode=(int)strtol(argv[2],0,10);
    allocation_fail=test==8u;
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
    expected_mode=persisted_mode>=0 && persisted_mode<=3?(unsigned)persisted_mode:3u;
    assert(opl_init(0)==0);allocation_intact();
    for(i=0;i<sizeof(allocated_pixels);i++)assert(((unsigned char *)&allocated_pixels)[i]==0u);
    assert(registered_main && registered_setting && !sends && (active_token&3u)==expected_mode);
    if(test==1u) {
        capture(0x0821u);await_complete();old_session=last_image_session;
        /* Same mode and unrelated settings cannot restart the session. */
        old_token=active_token;registered_setting("other",1);registered_setting("mode_extra",2);
        registered_setting(0,2);registered_setting("",2);registered_setting("m",2);registered_setting("mod",2);
        registered_setting("mode",(int)expected_mode);pass();assert(active_token==old_token);
        assert(last_image_session==old_session);
        puts("PASS persisted startup/default/range and ignored setting names");
    } else if(test==2u) {
        static const unsigned choices[]={0,2,3,1,0,3,1,2,3,0};
        old_session=0u;
        for(i=0;i<sizeof(choices)/sizeof(choices[0]);i++) {
            unsigned mode=choices[i];
            if(mode==expected_mode)continue;
            capture(0xffffu);await_packet();
            assert(last_image_fragment+1u<last_image_count); /* A real unfinished old job. */
            old_session=last_image_session;
            choose((int)mode);before=image_packets;
            /* The old published image is never reused after a switch. */
            for(unsigned wait=0;wait<8u;wait++)pass();assert(image_packets==before);
            capture(0x0821u+i);await_complete();assert(last_image_session!=old_session);
            old_session=last_image_session;
        }
        puts("PASS all-mode switches cancel old work, need fresh capture and change session");
    } else if(test==3u) {
        capture(0xffffu);await_packet();before=image_packets;
        assert(last_image_fragment+1u<last_image_count);
        inject_stage=1u;inject_mode=(expected_mode+1u)%4u;capture(0x1111u);
        for(i=0;i<10u;i++)pass();assert(image_packets==before);
        capture(0x0821u);await_complete();
        puts("PASS selection during capture discards old-epoch publication");
    } else if(test==4u) {
        capture(0xffffu);inject_stage=2u;
        for(i=0;i<10000u && !complete_images;i++)pass();assert(complete_images && !inject_stage);
        puts("PASS reserve race retries using newest tagged capture");
    } else if(test==5u) {
        capture(0xffffu);await_packet();before=sends;usb_up=0u;
        for(i=0;i<8u;i++)pass();assert(sends==before);
        choose((int)((expected_mode+1u)%4u));capture(0x0821u);
        for(i=0;i<8u;i++)pass();assert(sends==before);
        usb_up=1u;capture(0x0821u);await_complete();
        puts("PASS mode change while USB disconnected waits and fully repairs");
    } else if(test==6u) {
        unsigned initial=expected_mode;
        capture(0xffffu);await_packet();
        choose((int)((expected_mode+1u)%4u));capture_frame(0x1111u,0u);
        choose((int)initial);before=image_packets;
        /* Same physical buffer and less than100ms since this engine's last
         * capture: a new epoch must still publish immediately. */
        capture_frame(0x0821u,0u);await_complete();assert(image_packets>before);
        puts("PASS first capture of a new epoch bypasses previous-mode capture throttle");
    } else if(test==7u) {
        capture(0xffffu);await_packet();
        test_time=(UINT64_C(1)<<32)-2000000u;pass();
        choose((int)((expected_mode+1u)%4u));capture_frame(0x0821u,0u);await_complete();
        puts("PASS shared USB pacing survives CYCLES wrap across engine changes");
    } else assert(0 && "unknown test");
    allocation_intact();assert(sends && image_packets && !in_display && !in_main);
    return 0;
}
