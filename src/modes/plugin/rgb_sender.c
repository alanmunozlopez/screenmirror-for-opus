#include "opus_plugin.h"
#include "op1fw.h"
#include "engine.h"
/* 0.14: original v4 RGB332 planner, with output geometry selected in main.
 * Capture derives its local geometry from the immutable epoch/mode token.
 * Allocation stays bounded by the 320x160 balanced profile. */
#define MAX_WIDTH 320u
#define MAX_HEIGHT 160u
#define MAX_IMAGE_BYTES (MAX_WIDTH*MAX_HEIGHT)
#define MAX_TILES ((MAX_WIDTH/8u)*(MAX_HEIGHT/4u))
#define MAX_REGION_PACKETS (MAX_TILES/2u+1u)
#define MAX_REGION_STREAM_BYTES ((MAX_TILES/2u)*35u)
static unsigned output_width=240u,expected_token;
#define WIDTH output_width
#define HEIGHT (WIDTH/2u)
#define ROW_BYTES WIDTH
#define IMAGE_BYTES (ROW_BYTES*HEIGHT)
#define TILE_W 8
#define TILE_H 4
#define COLS (WIDTH/TILE_W)
#define TILES (COLS*(HEIGHT/TILE_H))
#define TILE_BYTES (TILE_W*TILE_H)
/* A denser update uses a keyframe, so a regional batch needs at most half the tiles. */
#define REGION_PACKETS (TILES/2+1)
#define PAYLOAD 58
#define SEND_CYCLES 2000000u
#define CAPTURE_CYCLES 40000000u
#ifndef REFRESH_CYCLES
#define REFRESH_CYCLES 3200000000u /* Eight seconds since the last completed keyframe. */
#endif
#include "../../lz-regions-source/lzss.h"
#define REGION_STREAM_BYTES ((TILES/2)*35)
#define snapshots sm_pixels->snapshots.rgb
#define reference sm_pixels->reference.rgb
static unsigned char compressed[MAX_IMAGE_BYTES];
static unsigned char region_stream[MAX_REGION_STREAM_BYTES],lz_buffer[MAX_IMAGE_BYTES+3],lz_trial[MAX_IMAGE_BYTES+3];
static unsigned char packets[MAX_REGION_PACKETS][PAYLOAD],packet_lengths[MAX_REGION_PACKETS];
static unsigned snapshot_seq[3],snapshot_token[3],column_offset[MAX_WIDTH],row_offset[MAX_HEIGHT];
static unsigned mapped_rows,mapped_width;
static volatile unsigned ready_token=3,send_slot=3;
static unsigned generation,handled_token=3,session_id;
static volatile unsigned pending,frame_callbacks,captures,pending_skips,capture_cycles_latest;
static volatile unsigned frame_gap_max_cycles,key_events,knob_events,capture_status,panel_rows;
static unsigned main_callbacks,frames_attempted,image_messages,usb_unconfigured;
static unsigned main_gap_max_cycles,transmit_cycles_latest,planning_cycles_latest;
static unsigned last_frame_cycle,frame_seen,last_main_cycle,main_seen,transmit_start;
static unsigned last_send,have_send,last_capture,have_capture,last_capture_token,last_refresh,have_reference,refresh_due;
static unsigned seq,part,fragment_count,kind,wire_size,last_status=99;
static void barrier(void) {
#ifdef __bfin__
    __asm__ __volatile__("ssync;" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}
static unsigned cycles(void) {
#ifdef __bfin__
    unsigned c;__asm__ __volatile__("%0 = CYCLES;" : "=d"(c));return c;
#else
    extern unsigned screenmirror_test_cycles(void);return screenmirror_test_cycles();
#endif
}
static unsigned crc(const unsigned char *p,unsigned n) {
    unsigned c=65535,i;
    while(n--) {c^=(unsigned)*p++<<8;for(i=0;i<8;i++)c=((c<<1)^((c&32768)?0x1021:0))&65535;}
    return c;
}
/* Called only by flush_entry.S at the display thread's single final-flush site.
 * The original call computes end = start + (physical_rows << 9). Validate that
 * span before every read. Never retain start/end, access the opaque raster, or
 * call opus_page_pixels outside a draw context. Assembly always forwards flush,
 * including when this observer refuses the supplied range. */
void sm_rgb_capture(void *start,void *end,int invalidate,unsigned token) {
    unsigned now=cycles(),slot,r,x,y,i=0,v,rows;
    const unsigned width=(token&3u)==SM_BALANCED?320u:240u,height=width/2u;
    __UINTPTR_TYPE__ first=(__UINTPTR_TYPE__)start,last=(__UINTPTR_TYPE__)end,span;
    const unsigned short *buf=(const unsigned short *)start;
    frame_callbacks++;
    if(frame_seen && now-last_frame_cycle>frame_gap_max_cycles)frame_gap_max_cycles=now-last_frame_cycle;
    last_frame_cycle=now;frame_seen=1;
    if(!start) {capture_status=1;return;}
    if(invalidate || (first&1u) || last<=first) {capture_status=3;return;}
    span=last-first;
    if(span!=512u*320u && span!=512u*376u && span!=512u*400u) {capture_status=4;return;}
    rows=(unsigned)(span/512u);panel_rows=rows;
    if(have_capture && last_capture_token==token && now-last_capture<CAPTURE_CYCLES)return;
    last_capture=now;have_capture=1;last_capture_token=token;
    if(mapped_rows!=rows || mapped_width!=width) {
        /* Keep the two divisors constant on Blackfin, as in each old profile. */
        if(width==240u) {
            for(x=0;x<240u;x++)column_offset[x]=256u*(rows-1u-x*rows/240u);
            for(y=0;y<120u;y++)row_offset[y]=y*rows/240u;
        } else {
            for(x=0;x<320u;x++)column_offset[x]=256u*(rows-1u-x*rows/320u);
            for(y=0;y<160u;y++)row_offset[y]=y*rows/320u;
        }
        mapped_rows=rows;mapped_width=width;
    }
    /* Never write the published slot or the slot main has reserved. */
    barrier();r=ready_token&3;
    for(slot=0;slot<3;slot++)if(slot!=r && slot!=send_slot)break;
    if(slot==3) {pending_skips++;return;}
#ifdef SCREENMIRROR_TEST_INTERLEAVING
    screenmirror_test_interleave(1);
#endif
    for(y=0;y<height;y++)for(x=0;x<width;x++) {
        v=buf[column_offset[x]+row_offset[y]];
        snapshots[slot][i++]=(unsigned char)(((v>>8)&0xe0)|((v>>6)&0x1c)|((v>>3)&3));
    }
    generation=(generation+1)&0x3fffffffu;snapshot_seq[slot]=generation&65535;
    snapshot_token[slot]=token;
    capture_cycles_latest=cycles()-now;captures++;capture_status=5;
    barrier();ready_token=(generation<<2)|slot;
}
static void send_packet(unsigned char *raw,unsigned length,unsigned version) {
    unsigned char wire[100];unsigned int events[34];
    unsigned i,j=5,k,c,total=0,acc=0,bits=0;
    c=crc(raw,length);raw[length]=(unsigned char)c;raw[length+1]=(unsigned char)(c>>8);
    wire[0]=0xf0;wire[1]=0x7d;wire[2]='S';wire[3]='M';wire[4]=(unsigned char)version;
    for(i=0;i<length+2;i++) {
        acc|=(unsigned)raw[i]<<bits;bits+=8;
        while(bits>=7) {wire[j++]=(unsigned char)(acc&127);acc>>=7;bits-=7;}
    }
    if(bits)wire[j++]=(unsigned char)acc;
    wire[j++]=0xf7;
    for(i=0;i<j;i+=3) {
        k=j-i;if(k>3)k=3;c=(i+k==j)?4+k:4;c|=(unsigned)wire[i]<<8;
        if(k>1)c|=(unsigned)wire[i+1]<<16;
        if(k>2)c|=(unsigned)wire[i+2]<<24;
        events[total++]=c;
    }
    sm_send_events(events,(int)total);
    /* Bound the gap between actual calls, including time spent preparing a large image. */
    last_send=cycles();have_send=1;
}
static void word(unsigned char *p,unsigned v) {
    p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
static void short_word(unsigned char *p,unsigned v) {p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void diagnostic(unsigned char *raw,unsigned t) {
    short_word(raw,seq);raw[2]=255;raw[3]=255;raw[4]=(unsigned char)capture_status;
    raw[5]=(unsigned char)pending;raw[6]=254;
    /* Schema 3 retains tile-count dimensions, replaces pending_skips by planning cycles. */
    raw[7]=WIDTH/TILE_W;raw[8]=HEIGHT/TILE_H;
    raw[9]=3;raw[10]=0;raw[11]=14;raw[12]=1;
    word(raw+13,t);word(raw+17,frame_callbacks);word(raw+21,main_callbacks);word(raw+25,captures);
    word(raw+29,frames_attempted);word(raw+33,image_messages);word(raw+37,key_events);word(raw+41,knob_events);
    word(raw+45,capture_cycles_latest);word(raw+49,frame_gap_max_cycles);word(raw+53,main_gap_max_cycles);
    word(raw+57,wire_size);word(raw+61,transmit_cycles_latest);word(raw+65,planning_cycles_latest);word(raw+69,usb_unconfigured);
    send_packet(raw,73,3);
}
static unsigned tile_offset(unsigned tile,unsigned j) {
    /* A runtime tile/COLS division in this hot path would cost much more than
     * the original constant240/320 builds. Both branches retain those constants. */
    if(output_width==240u)
        return (tile/30u)*4u*240u+(j/8u)*240u+(tile%30u)*8u+(j%8u);
    return (tile/40u)*4u*320u+(j/8u)*320u+(tile%40u)*8u+(j%8u);
}
static unsigned tile_record(const unsigned char *src,unsigned tile,unsigned char *record) {
    unsigned char raw[TILE_BYTES],runs[TILE_BYTES*2];unsigned i,n=0,run=0,value=0;
    for(i=0;i<TILE_BYTES;i++)raw[i]=src[tile_offset(tile,i)];
    for(i=0;i<TILE_BYTES;i++) {
        if(run && raw[i]!=value) {runs[n++]=(unsigned char)run;runs[n++]=(unsigned char)value;run=0;}
        value=raw[i];run++;
    }
    runs[n++]=(unsigned char)run;runs[n++]=(unsigned char)value;
    short_word(record,tile);
    if(n<TILE_BYTES) {record[2]=(unsigned char)(128|n);for(i=0;i<n;i++)record[3+i]=runs[i];return n+3;}
    record[2]=TILE_BYTES;for(i=0;i<TILE_BYTES;i++)record[3+i]=raw[i];return TILE_BYTES+3;
}
static unsigned compress_full(const unsigned char *src) {
    unsigned i,run=0,value=0,n=0;
    for(i=0;i<IMAGE_BYTES;i++) {
        if(run && (src[i]!=value || run==255)) {
            if(n+2>IMAGE_BYTES)return IMAGE_BYTES;
            compressed[n++]=(unsigned char)run;compressed[n++]=(unsigned char)value;run=0;
        }
        value=src[i];run++;
    }
    if(n+2>IMAGE_BYTES)return IMAGE_BYTES;
    compressed[n++]=(unsigned char)run;compressed[n++]=(unsigned char)value;return n;
}
static void release_snapshot(void) {barrier();send_slot=3;pending=0;}
/* A compressed candidate must save at least one complete message against the
 * best legacy update. Prefix and padding are included. Equal packet counts keep
 * legacy decoding; two LZ candidates compare packets then payload bytes. */
static void try_lz(const unsigned char *src,unsigned decoded,unsigned transform,unsigned target_kind,
                   unsigned legacy_count,unsigned legacy_size) {
    unsigned prefix=target_kind==6?3:2,capacity,n,count,i;
    if(legacy_count<2 || legacy_size<=prefix)return;
    capacity=(legacy_count-1)*PAYLOAD;
    if(capacity<=prefix)return;
    capacity-=prefix;
    if(capacity>legacy_size-prefix)capacity=legacy_size-prefix;
    if(capacity>IMAGE_BYTES)capacity=IMAGE_BYTES;
    n=bounded_lzss_encode(src,decoded,lz_trial+prefix,capacity,4,0);
    if(n==0xffffffffu)return;
    n+=prefix;count=(n+PAYLOAD-1)/PAYLOAD;
    if(count>=legacy_count)return;
    if(count>fragment_count || (count==fragment_count &&
       (kind<6 || n>=wire_size)))return;
    short_word(lz_trial,decoded);
    if(prefix==3)lz_trial[2]=(unsigned char)transform;
    for(i=0;i<n;i++)lz_buffer[i]=lz_trial[i];
    kind=target_kind;wire_size=n;fragment_count=count;
}
static void prepare(unsigned now) {
    unsigned token,slot,i,tile,dirty=0,used=0,count=0,n,changed,full_size=IMAGE_BYTES,full_count,force;
    unsigned stream_size=0,legacy_count,legacy_size,started;
    unsigned char record[TILE_BYTES*2+3];const unsigned char *src;
    token=ready_token;if((token&3)==3 || token==handled_token)return;
    slot=token&3;send_slot=slot;barrier();
#ifdef SCREENMIRROR_TEST_INTERLEAVING
    screenmirror_test_interleave(2);
#endif
    if(ready_token!=token || snapshot_token[slot]!=expected_token) {send_slot=3;return;}
    /* No pixels read until publication and mode/epoch both validate. */
    started=cycles();
#ifdef SCREENMIRROR_TEST_PREPARE_COST
    screenmirror_test_prepare_cost();
#endif
    src=snapshots[slot];handled_token=token;seq=snapshot_seq[slot];
    force=!have_reference || refresh_due || (unsigned)(now-last_refresh)>=REFRESH_CYCLES;
    if(!force) {
        for(tile=0;tile<TILES;tile++) {
            changed=0;
            for(i=0;i<TILE_BYTES;i++)if(src[tile_offset(tile,i)]!=reference[tile_offset(tile,i)]) {changed=1;break;}
            if(!changed)continue;
            if(++dirty>TILES/2) {force=1;break;} /* Atomic large/page change; bound packet storage. */
            n=tile_record(src,tile,record);
            if(used+n>PAYLOAD) {packet_lengths[count++]=(unsigned char)used;used=0;}
            for(i=0;i<n;i++) {
                packets[count][used++]=record[i];region_stream[stream_size++]=record[i];
            }
        }
        if(used)packet_lengths[count++]=(unsigned char)used;
    }
    if(!force && !dirty) {
        wire_size=0;planning_cycles_latest=cycles()-started;release_snapshot();return;
    }
    /* Preserve the 0.9 scan/compression decision. Full-frame work is omitted for
     * unchanged and small regional updates. New LZ searches are always bounded. */
    if(force || dirty>TILES/16)full_size=compress_full(src);
    full_count=(full_size+PAYLOAD-1)/PAYLOAD;
    if(force || count>=full_count) {
        kind=full_size<IMAGE_BYTES?1:0;wire_size=full_size;fragment_count=full_count;
    } else {
        kind=5;wire_size=stream_size;fragment_count=count;
    }
    legacy_count=fragment_count;legacy_size=wire_size;
    if(legacy_count>1) {
        if(force || dirty>TILES/16) {
            try_lz(src,IMAGE_BYTES,0,6,legacy_count,legacy_size);
            if(full_size<IMAGE_BYTES)try_lz(compressed,full_size,1,6,legacy_count,legacy_size);
        }
        if(!force)try_lz(region_stream,stream_size,0,7,legacy_count,legacy_size);
    }
    part=0;pending=1;
    /* Includes slot validation-to-ready planning, comparisons, compression and
     * selected LZ copy. Excludes the reference copy after the batch is sent. */
    planning_cycles_latest=cycles()-started;
}
static void main_pass(void) {
    struct opus_clock now;unsigned char raw[80];unsigned n,i,t=cycles();
    const unsigned char *src;
    main_callbacks++;
    if(main_seen && t-last_main_cycle>main_gap_max_cycles)main_gap_max_cycles=t-last_main_cycle;
    last_main_cycle=t;main_seen=1;
    /* Latch expiry while a long regional batch is still pending. CYCLES wraps in ~10.7 s. */
    if(have_reference && (unsigned)(t-last_refresh)>=REFRESH_CYCLES)refresh_due=1;
    if(have_send && t-last_send<SEND_CYCLES)return;
    if(!sm_transport_ready())return;
    if(!fw_usb_configured()) {usb_unconfigured++;have_reference=0;release_snapshot();return;}
    opus_clock(&now);
    if(now.second!=last_status) {last_status=now.second;diagnostic(raw,t);return;}
    if(!pending)prepare(t);
    if(!pending)return;
    barrier();src=snapshots[send_slot];
    word(raw,session_id);short_word(raw+4,seq);short_word(raw+6,part);short_word(raw+8,fragment_count);
    raw[10]=(unsigned char)kind;short_word(raw+11,WIDTH);short_word(raw+13,HEIGHT);
    if(kind==5) {n=packet_lengths[part];for(i=0;i<n;i++)raw[15+i]=packets[part][i];}
    else {
        n=wire_size-part*PAYLOAD;if(n>PAYLOAD)n=PAYLOAD;
        for(i=0;i<n;i++) {
            if(kind==6 || kind==7)raw[15+i]=lz_buffer[part*PAYLOAD+i];
            else raw[15+i]=(kind==1)?compressed[part*PAYLOAD+i]:src[part*PAYLOAD+i];
        }
    }
    if(part==0)transmit_start=cycles();
    send_packet(raw,n+15,4);image_messages++;
    if(++part==fragment_count) {
        transmit_cycles_latest=cycles()-transmit_start;frames_attempted++;
        for(i=0;i<IMAGE_BYTES;i++)reference[i]=src[i];
        have_reference=1;
        if(kind!=5 && kind!=7) {last_refresh=cycles();refresh_due=0;}
        release_snapshot();
    }
}
/* Only main calls select. Do not reset ready_token, generation, capture
 * timing, mapping state or snapshot storage: display can be preempting main. */
void sm_rgb_select(unsigned token,unsigned session) {
    release_snapshot();expected_token=token;
    output_width=(token&3u)==SM_BALANCED?320u:240u;
    session_id=session;have_reference=0;refresh_due=0;
    wire_size=0;fragment_count=0;part=0;last_status=99;
}
void sm_rgb_main(void) {main_pass();}
void sm_rgb_key(int key,int pressed) {(void)key;(void)pressed;key_events++;}
void sm_rgb_knob(int index,short delta) {(void)index;(void)delta;knob_events++;}
