#include "opus_plugin.h"
#include "op1fw.h"
#include "engine.h"
/* 0.14: exact native RGB565 engine with per-snapshot mode/epoch ownership.
 * Display is the sole snapshot producer; main reserves one immutable snapshot.
 * Capture does not send USB, allocate, touch files, or install audio callbacks. */
#define MAX_WIDTH 400u
#define MAX_PIXELS 80000u
#define STRIPE_ROWS 8u
#define MAX_STRIPE_BYTES (MAX_WIDTH*STRIPE_ROWS*2u)
#define PAYLOAD 57u
#define SEND_CYCLES 2000000u
#define CAPTURE_CYCLES 40000000u
#ifndef REFRESH_CYCLES
#define REFRESH_CYCLES 3200000000u
#endif
#include "../../lz-regions-source/lzss.h"
#define snapshots sm_pixels->snapshots.native
#define reference sm_pixels->reference.native
static unsigned char raw_stripe[MAX_STRIPE_BYTES],encoded_stripe[MAX_STRIPE_BYTES];
static unsigned snapshot_seq[3],snapshot_width[3],snapshot_token[3],column_offset[MAX_WIDTH],mapped_rows;
static unsigned expected_token;
static volatile unsigned ready_token=3,send_slot=3;
static unsigned generation,handled_token=3,session_id;
static volatile unsigned frame_callbacks,captures,pending_skips,capture_cycles_latest;
static volatile unsigned frame_gap_max_cycles,key_events,knob_events,capture_status,panel_rows;
static unsigned main_callbacks,stripe_batches_sent,image_messages,usb_unconfigured;
static unsigned main_gap_max_cycles,transmit_cycles_latest,planning_cycles_latest;
static unsigned last_frame_cycle,frame_seen,last_main_cycle,main_seen,transmit_start;
static unsigned last_send,have_send,last_capture,have_capture,last_capture_token,last_refresh,have_reference,refresh_due;
/* pending: 0 idle, 1 scan, 2 prepare one stripe, 3 send one stripe. */
static unsigned pending,active_width,active_height,job_width,job_height,stripe_count;
static unsigned reference_valid,scan_index,dirty_mask,dirty_count,todo_mask,full_snapshot,forced_snapshot;
static unsigned seq,stripe_index,stripe_words,stripe_offset,part,fragment_count,encoding,wire_size,last_status=99;
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
static unsigned valid_width(unsigned rows) {return rows==320u || rows==376u || rows==400u;}
static unsigned all_stripes(void) {return (1u<<stripe_count)-1u;}
void sm_native_capture(void *start,void *end,int invalidate,unsigned token) {
    unsigned now=cycles(),slot,r,x,y,bx,by,bottom,rows;
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
    if(mapped_rows!=rows) {
        for(x=0;x<rows;x++)column_offset[x]=256u*(rows-1u-x);
        mapped_rows=rows;
    }
    barrier();r=ready_token&3u;
    for(slot=0;slot<3;slot++)if(slot!=r && slot!=send_slot)break;
    if(slot==3) {pending_skips++;return;}
#ifdef SCREENMIRROR_TEST_INTERLEAVING
    screenmirror_test_interleave(1);
#endif
    /* A small tiled transpose reuses the eight source columns and destination
     * rows in cache. Capture still preserves every RGB565 word unchanged. */
    for(by=0;by<rows/2u;by+=8u) {
        bottom=by+8u;if(bottom>rows/2u)bottom=rows/2u;
        for(bx=0;bx<rows;bx+=8u)
            for(y=by;y<bottom;y++)
                for(x=bx;x<bx+8u;x++)snapshots[slot][y*rows+x]=buf[column_offset[x]+y];
    }
    generation=(generation+1u)&0x3fffffffu;
    snapshot_seq[slot]=generation&65535u;snapshot_width[slot]=rows;snapshot_token[slot]=token;
    capture_cycles_latest=cycles()-now;captures++;capture_status=5;
    barrier();ready_token=(generation<<2)|slot;
}
static unsigned crc(const unsigned char *p,unsigned n) {
    unsigned c=65535u,i;
    while(n--) {c^=(unsigned)*p++<<8;for(i=0;i<8;i++)c=((c<<1)^((c&32768u)?0x1021u:0u))&65535u;}
    return c;
}
static void send_packet(unsigned char *raw,unsigned length,unsigned version) {
    unsigned char wire[100];unsigned int events[34];
    unsigned i,j=5,k,c,total=0,acc=0,bits=0;
    c=crc(raw,length);raw[length]=(unsigned char)c;raw[length+1]=(unsigned char)(c>>8);
    wire[0]=0xf0;wire[1]=0x7d;wire[2]='S';wire[3]='M';wire[4]=(unsigned char)version;
    for(i=0;i<length+2u;i++) {
        acc|=(unsigned)raw[i]<<bits;bits+=8;
        while(bits>=7u) {wire[j++]=(unsigned char)(acc&127u);acc>>=7;bits-=7;}
    }
    if(bits)wire[j++]=(unsigned char)acc;
    wire[j++]=0xf7;
    for(i=0;i<j;i+=3) {
        k=j-i;if(k>3)k=3;c=(i+k==j)?4u+k:4u;c|=(unsigned)wire[i]<<8;
        if(k>1)c|=(unsigned)wire[i+1]<<16;
        if(k>2)c|=(unsigned)wire[i+2]<<24;
        events[total++]=c;
    }
    sm_send_events(events,(int)total);
    last_send=cycles();have_send=1;
}
static void word(unsigned char *p,unsigned v) {
    p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
static void short_word(unsigned char *p,unsigned v) {p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void diagnostic(unsigned char *raw,unsigned t) {
    unsigned width=active_width?active_width:320u;
    short_word(raw,seq);raw[2]=255;raw[3]=255;raw[4]=(unsigned char)capture_status;
    raw[5]=(unsigned char)(pending!=0);raw[6]=254;
    raw[7]=(unsigned char)(width/8u);raw[8]=(unsigned char)(width/8u);
    raw[9]=4;raw[10]=0;raw[11]=14;raw[12]=1;
    word(raw+13,t);word(raw+17,frame_callbacks);word(raw+21,main_callbacks);word(raw+25,captures);
    word(raw+29,stripe_batches_sent);word(raw+33,image_messages);word(raw+37,key_events);word(raw+41,knob_events);
    word(raw+45,capture_cycles_latest);word(raw+49,frame_gap_max_cycles);word(raw+53,main_gap_max_cycles);
    word(raw+57,wire_size);word(raw+61,transmit_cycles_latest);word(raw+65,planning_cycles_latest);word(raw+69,usb_unconfigured);
    send_packet(raw,73,3);
}
static void release_snapshot(void) {barrier();send_slot=3;pending=0;}
static void stripe_geometry(unsigned stripe) {
    unsigned rows=job_height-stripe*STRIPE_ROWS;
    if(rows>STRIPE_ROWS)rows=STRIPE_ROWS;
    stripe_offset=stripe*STRIPE_ROWS*job_width;stripe_words=rows*job_width;
}
static void reserve_snapshot(void) {
    unsigned token=ready_token,slot,width;
    if((token&3u)==3u || token==handled_token)return;
    slot=token&3u;send_slot=slot;barrier();
#ifdef SCREENMIRROR_TEST_INTERLEAVING
    screenmirror_test_interleave(2);
#endif
    if(ready_token!=token || snapshot_token[slot]!=expected_token) {send_slot=3;return;}
    width=snapshot_width[slot];
    if(!valid_width(width)) {send_slot=3;return;}
    handled_token=token;seq=snapshot_seq[slot];job_width=width;job_height=width/2u;
    stripe_count=(job_height+STRIPE_ROWS-1u)/STRIPE_ROWS;
    if(active_width!=width) {
        if(active_width)session_id=sm_new_session();
        active_width=width;active_height=job_height;
        reference_valid=0;have_reference=0;refresh_due=0;
    }
    scan_index=0;dirty_mask=0;dirty_count=0;
    forced_snapshot=!have_reference || refresh_due;
    pending=1;
}
/* One bounded stripe comparison per main callback. */
static void scan_one(void) {
    unsigned i,changed=0,started=cycles(),bit=1u<<scan_index;
    const unsigned short *src=snapshots[send_slot];
    stripe_geometry(scan_index);
    if(forced_snapshot || !(reference_valid&bit))changed=1;
    else for(i=0;i<stripe_words;i++)if(src[stripe_offset+i]!=reference[stripe_offset+i]) {changed=1;break;}
    if(changed) {dirty_mask|=bit;dirty_count++;}
    planning_cycles_latest=cycles()-started;
    if(++scan_index==stripe_count) {
        full_snapshot=forced_snapshot || refresh_due || dirty_count>stripe_count/2u;
        todo_mask=full_snapshot?all_stripes():dirty_mask;
        if(!todo_mask) {wire_size=0;release_snapshot();return;}
        pending=2;
    }
}
/* Serialize and compress at most 6400 bytes, never a whole 160 KB image. */
static void prepare_one(void) {
    unsigned i,v,n,raw_bytes,raw_count,capacity,started=cycles();
    const unsigned short *src=snapshots[send_slot];
    for(stripe_index=0;stripe_index<stripe_count;stripe_index++)if(todo_mask&(1u<<stripe_index))break;
    stripe_geometry(stripe_index);raw_bytes=stripe_words*2u;
    for(i=0;i<stripe_words;i++) {
        v=src[stripe_offset+i];raw_stripe[i*2u]=(unsigned char)v;raw_stripe[i*2u+1u]=(unsigned char)(v>>8);
    }
    raw_count=(raw_bytes+PAYLOAD-1u)/PAYLOAD;encoding=0;wire_size=raw_bytes;
    capacity=(raw_count-1u)*PAYLOAD;
    if(capacity>=raw_bytes)capacity=raw_bytes-1u;
#ifdef SCREENMIRROR_TEST_PREPARE_COST
    screenmirror_test_prepare_cost();
#endif
    n=bounded_lzss_encode(raw_stripe,raw_bytes,encoded_stripe,capacity,4,0);
    if(n!=0xffffffffu) {encoding=1;wire_size=n;}
    fragment_count=(wire_size+PAYLOAD-1u)/PAYLOAD;part=0;pending=3;
    planning_cycles_latest=cycles()-started;
}
static void main_pass(void) {
    struct opus_clock now;unsigned char raw[80];unsigned n,i,t=cycles();
    const unsigned char *bytes;
    main_callbacks++;
    if(main_seen && t-last_main_cycle>main_gap_max_cycles)main_gap_max_cycles=t-last_main_cycle;
    last_main_cycle=t;main_seen=1;
    if(have_reference && (unsigned)(t-last_refresh)>=REFRESH_CYCLES)refresh_due=1;
    if(have_send && t-last_send<SEND_CYCLES)return;
    if(!sm_transport_ready())return;
    if(!fw_usb_configured()) {usb_unconfigured++;have_reference=0;reference_valid=0;release_snapshot();return;}
    opus_clock(&now);
    if(now.second!=last_status) {last_status=now.second;diagnostic(raw,t);return;}
    if(!pending)reserve_snapshot();
    if(!pending)return;
    if(pending==1) {scan_one();return;}
    if(pending==2)prepare_one();
    if(pending!=3)return;
    word(raw,session_id);short_word(raw+4,seq);raw[6]=(unsigned char)stripe_index;
    raw[7]=(unsigned char)part;raw[8]=(unsigned char)fragment_count;
    raw[9]=(unsigned char)(encoding|(full_snapshot?128u:0u));
    short_word(raw+10,job_width);short_word(raw+12,job_height);short_word(raw+14,wire_size);
    bytes=encoding?encoded_stripe:raw_stripe;
    n=wire_size-part*PAYLOAD;if(n>PAYLOAD)n=PAYLOAD;
    for(i=0;i<n;i++)raw[16+i]=bytes[part*PAYLOAD+i];
    if(!part)transmit_start=cycles();
    send_packet(raw,n+16u,5);image_messages++;
    if(++part==fragment_count) {
        const unsigned short *src=snapshots[send_slot];
        transmit_cycles_latest=cycles()-transmit_start;stripe_batches_sent++;
        for(i=0;i<stripe_words;i++)reference[stripe_offset+i]=src[stripe_offset+i];
        reference_valid|=1u<<stripe_index;todo_mask&=~(1u<<stripe_index);
        if(todo_mask)pending=2;
        else {
            have_reference=reference_valid==all_stripes();
            if(full_snapshot) {last_refresh=cycles();refresh_due=0;}
            release_snapshot();
        }
    }
}
/* Main-only reset. Capture publication, timing and mapping remain display-owned. */
void sm_native_select(unsigned token,unsigned session) {
    release_snapshot();expected_token=token;session_id=session;
    active_width=0;active_height=0;have_reference=0;reference_valid=0;refresh_due=0;
    wire_size=0;fragment_count=0;part=0;last_status=99;
}
void sm_native_main(void) {main_pass();}
void sm_native_key(int key,int pressed) {(void)key;(void)pressed;key_events++;}
void sm_native_knob(int index,short delta) {(void)index;(void)delta;knob_events++;}
