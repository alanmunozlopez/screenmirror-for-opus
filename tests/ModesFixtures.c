/* Host-only corpus from the real controller and both engines, linked as separate
 * translation units. No device, live MIDI or plugin-private symbols are used. */
#include "opus_plugin.h"
#include "../src/modes/plugin/engine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int opl_init(struct opl_self *self);
void screenmirror_capture_flush(void *start,void *end,int invalidate);
static void (*main_callback)(void);
static void (*setting_callback)(const char *,int);
static uint64_t elapsed_cycles;
static unsigned panel,scene,patch,target_mode,target_session,complete,stripes_received,inside_main;
static unsigned last_send,have_send,total_messages,emitted_messages;
static unsigned char wire[100];
static unsigned wire_length;
static unsigned short framebuffer[256*400];
static struct sm_pixel_storage allocated_pixels __attribute__((aligned(32)));
static unsigned allocations;

void *opus_alloc(struct opl_self *self,unsigned bytes) {
    (void)self;
    assert(++allocations==1u && !inside_main && !main_callback && !setting_callback);
    assert(bytes==640000u && bytes==sizeof(allocated_pixels));
    assert(((uintptr_t)&allocated_pixels&31u)==0u);
    memset(&allocated_pixels,0,sizeof(allocated_pixels));
    return &allocated_pixels;
}
static void allocation_intact(void) {
    assert(allocations==1u && sm_pixels==&allocated_pixels);
}

unsigned screenmirror_test_cycles(void) {return (unsigned)elapsed_cycles;}
void opus_on_main_pass(struct opl_self *self,void (*fn)(void)) {(void)self;allocation_intact();main_callback=fn;}
void opus_on_setting(struct opl_self *self,void (*fn)(const char *,int)) {(void)self;allocation_intact();setting_callback=fn;}
void opus_on_key_after(struct opl_self *self,void (*fn)(int,int)) {(void)self;allocation_intact();(void)fn;}
void opus_on_knob_after(struct opl_self *self,void (*fn)(int,short)) {(void)self;allocation_intact();(void)fn;}
int opus_setting(struct opl_self *self,const char *name) {(void)self;allocation_intact();assert(!strcmp(name,"mode"));return 0;}
void opus_clock(struct opus_clock *clock) {
    unsigned seconds=(unsigned)(elapsed_cycles/400000000u);
    allocation_intact();
    memset(clock,0,sizeof(*clock));clock->second=(unsigned char)(seconds%60u);
    clock->minute=(unsigned char)((seconds/60u)%60u);clock->hour=(unsigned char)((seconds/3600u)%24u);
}
int fw_usb_configured(void) {return 1;}
static unsigned u16(const unsigned char *raw,unsigned at) {return raw[at]|(unsigned)raw[at+1u]<<8;}
static unsigned u32(const unsigned char *raw,unsigned at) {return u16(raw,at)|(u16(raw,at+2u)<<16);}
static unsigned checksum(const unsigned char *bytes,unsigned count) {
    unsigned c=65535u,i;
    while(count--) {
        c^=(unsigned)*bytes++<<8;
        for(i=0;i<8;i++)c=((c<<1)^((c&32768u)?0x1021u:0u))&65535u;
    }
    return c;
}
static void packet_finished(void) {
    unsigned char raw[80];unsigned i,n=0,acc=0,bits=0,width;
    assert(wire_length>=7u && wire_length<=92u);
    assert(wire[0]==240u && wire[1]==125u && wire[2]=='S' && wire[3]=='M');
    for(i=5;i<wire_length-1u;i++) {
        assert(wire[i]<128u);acc|=(unsigned)wire[i]<<bits;bits+=7u;
        while(bits>=8u) {raw[n++]=(unsigned char)acc;acc>>=8;bits-=8u;}
    }
    assert(acc==0u && n>=6u && n<=75u);
    assert(checksum(raw,n-2u)==u16(raw,n-2u));
    if(wire[4]==3u) {
        assert(raw[9]==(target_mode==SM_QUALITY?4u:3u) && raw[10]==0u && raw[11]==14u && raw[12]==1u);
        return;
    }
    emitted_messages++;
    if(!target_session) {assert(!patch);target_session=u32(raw,0);}
    assert(target_session!=0u && u32(raw,0)==target_session);
    if(target_mode==SM_QUALITY) {
        unsigned stripe_count=(panel/2u+7u)/8u;
        assert(wire[4]==5u && ((raw[9]&128u)!=0u)==!patch);
        assert(u16(raw,10)==panel && u16(raw,12)==panel/2u);
        if(patch)assert(raw[6]==0u);
        if(raw[7]+1u==raw[8])stripes_received|=1u<<raw[6];
        if(stripes_received==(patch?1u:((1u<<stripe_count)-1u)))complete=1;
    } else {
        width=target_mode==SM_FAST?240u:320u;
        assert(wire[4]==4u && u16(raw,11)==width && u16(raw,13)==width/2u);
        if(patch)assert(raw[10]==5u || raw[10]==7u);
        else assert(raw[10]==0u || raw[10]==1u || raw[10]==6u);
        if(u16(raw,6)+1u==u16(raw,8))complete=1;
    }
}
void fw_midi_send(int unused,const unsigned int *events,int count) {
    int i,j,n;(void)unused;
    allocation_intact();assert(inside_main && count>0 && count<=31);
    if(have_send)assert((unsigned)(screenmirror_test_cycles()-last_send)>=2000000u);
    for(i=0;i<count;i++) {
        unsigned cin=events[i]&15u;
        n=cin==4u?3:(int)cin-4;
        assert(n>=1 && n<=3);
        for(j=0;j<n;j++) {
            unsigned byte=(events[i]>>(8u+8u*(unsigned)j))&255u;
            assert(wire_length<sizeof(wire));wire[wire_length++]=(unsigned char)byte;
            assert(putchar((int)byte)!=EOF);
            if(byte==247u) {packet_finished();wire_length=0;}
        }
    }
    elapsed_cycles+=80000u;last_send=screenmirror_test_cycles();have_send=1;total_messages++;
}
static unsigned short pixel(unsigned x,unsigned y) {
    static const unsigned short palette[]={0x0000u,0x0821u,0x18a3u,0x0672u,0x5b7fu,0xf81fu,0xffffu};
    unsigned short value=palette[(x/24u+y/12u+scene)%7u];
    /* Fine dim strokes and dark text on a blue selection, plus isolated edges. */
    if(y>=16u && y<32u)value=(x%5u==0u || y%7u==0u)?0x0821u:0x5b7fu;
    if(x==1u || y==1u)value=0xffffu;
    if(patch && x<8u && y<4u)value=0xf800u;
    return value;
}
static void upload(void) {
    unsigned x,y;
    for(y=0;y<panel/2u;y++)for(x=0;x<panel;x++)
        framebuffer[256u*(panel-1u-x)+y]=pixel(x,y);
}
static void write_expected(const char *directory,unsigned index,FILE *manifest) {
    unsigned width=target_mode==SM_FAST?240u:target_mode==SM_BALANCED?320u:panel;
    unsigned x,y,v;char path[1024],name[40];FILE *file;
    assert(snprintf(name,sizeof(name),"frame-%u.%s",index,target_mode==SM_QUALITY?"rgb565":"rgb332")>0);
    assert(snprintf(path,sizeof(path),"%s/%s",directory,name)>0);
    file=fopen(path,"wb");assert(file);
    for(y=0;y<width/2u;y++)for(x=0;x<width;x++) {
        v=pixel(x*panel/width,y*panel/width);
        if(target_mode==SM_QUALITY) {
            assert(fputc(v&255u,file)!=EOF);assert(fputc(v>>8,file)!=EOF);
        } else assert(fputc(((v>>8)&0xe0u)|((v>>6)&0x1cu)|((v>>3)&3u),file)!=EOF);
    }
    assert(!fclose(file));
    assert(fprintf(manifest,"%s{\"width\":%u,\"height\":%u,\"format\":%u,\"file\":\"%s\"}",
        index?",\n":"",width,width/2u,target_mode==SM_QUALITY?5u:4u,name)>0);
}
static void receive_update(void) {
    unsigned step;
    allocation_intact();
    complete=0;stripes_received=0;
    for(step=0;step<10000u && !complete;step++) {
        inside_main=1;main_callback();inside_main=0;
        if(!complete)screenmirror_capture_flush(framebuffer,framebuffer+256u*panel,0);
        elapsed_cycles+=3200000u;
    }
    assert(complete && step<10000u);
}
int main(int argc,char **argv) {
    static const unsigned modes[]={SM_FAST,SM_BALANCED,SM_QUALITY,SM_FAST,SM_QUALITY,SM_BALANCED,SM_FAST};
    unsigned index,previous_session;char path[1024];FILE *manifest;
    assert(argc==3);panel=(unsigned)strtoul(argv[1],0,10);
    assert(panel==320u || panel==376u || panel==400u);
    assert(snprintf(path,sizeof(path),"%s/frames.json",argv[2])>0);
    manifest=fopen(path,"w");assert(manifest && fputs("[\n",manifest)!=EOF);
    assert(opl_init(0)==0 && main_callback && setting_callback);
    for(index=0;index<sizeof(modes)/sizeof(modes[0]);index++) {
        target_mode=modes[index];scene=index;patch=0;
        previous_session=target_session;target_session=0;upload();
        if(index)setting_callback("mode",(int)target_mode);
        receive_update();
        assert(target_session!=previous_session);
        write_expected(argv[2],index*2u,manifest);
        /* Each selected mode must retain its fresh reference for the next small
         * update. The physical 8x4 patch touches one RGB tile or native stripe. */
        patch=1;upload();receive_update();
        write_expected(argv[2],index*2u+1u,manifest);
    }
    allocation_intact();assert(!wire_length && fputs("\n]\n",manifest)!=EOF && !fclose(manifest));
    fprintf(stderr,"PASS modes actual-C corpus: panel%u,14images(7full+7regions),6directionaltransitions,%uimagepackets,%umessages\n",
        panel,emitted_messages,total_messages);
    return 0;
}
