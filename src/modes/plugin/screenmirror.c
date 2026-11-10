#include "opus_plugin.h"
#include "op1fw.h"
#include "engine.h"

/* Only the selected engine accesses these pools. A fresh epoch invalidates all
 * old snapshot metadata before either engine may consume the reused bytes. */
struct sm_pixel_storage *sm_pixels;
volatile unsigned sm_burst=4u;

/* Settings callbacks only publish a request. Main owns cancellation, transport
 * and selecting an engine. Display reads one atomic mode/epoch token and the
 * selected engine publishes a snapshot tagged with that exact token. */
#define SEND_CYCLES 2000000u
static volatile unsigned requested_mode;
static volatile unsigned active_token=3u;
static unsigned epoch,session_counter,last_usb_send,have_usb_send;

static unsigned controller_cycles(void) {
#ifdef __bfin__
    unsigned value;
    __asm__ __volatile__("%0 = CYCLES;" : "=d"(value));
    return value;
#else
    extern unsigned screenmirror_test_cycles(void);
    return screenmirror_test_cycles();
#endif
}
static void controller_barrier(void) {
#ifdef __bfin__
    __asm__ __volatile__("ssync;" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}
static unsigned valid_mode(int value) {
    return value>=0 && value<=3 ? (unsigned)value:SM_SHARP;
}
unsigned sm_new_session(void) {
    /* Called on main only, including physical geometry changes in an engine. */
    if(!++session_counter)session_counter=1u;
    return session_counter;
}
int sm_transport_ready(void) {
    return !have_usb_send || (unsigned)(controller_cycles()-last_usb_send)>=SEND_CYCLES;
}
void sm_send_events(const unsigned int *events,int count) {
    /* The selected engine is the sole caller, once per ready main pass. Keep
     * spacing across engine changes, measured after the actual send returns. */
    fw_midi_send(0,events,count);
    last_usb_send=controller_cycles();have_usb_send=1u;
}
static void select_mode(unsigned mode) {
    unsigned token,session=sm_new_session();
    epoch=(epoch+1u)&0x3fffffffu;
    if(!epoch)epoch=1u; /* Epoch 0 is reserved for the uninitialized token. */
    token=(epoch<<2)|mode;
    if(mode==SM_QUALITY)sm_native_select(token,session);
    else sm_rgb_select(token,session);
    controller_barrier();active_token=token;
}
static unsigned valid_burst(int value) {
    return value>=0 && value<=3 ? (unsigned)value+1u:4u;
}
static void setting_changed(const char *name,int value) {
    if(name && name[0]=='m' && name[1]=='o' && name[2]=='d' &&
       name[3]=='e' && name[4]==0)requested_mode=valid_mode(value);
    if(name && name[0]=='s' && name[1]=='e' && name[2]=='n' &&
       name[3]=='d' && name[4]==0)sm_burst=valid_burst(value);
}
void screenmirror_capture_flush(void *start,void *end,int invalidate) {
    unsigned token=active_token;
    if(!(token>>2))return; /* Not initialized, or opl_init failed. */
    if((token&3u)==SM_QUALITY)sm_native_capture(start,end,invalidate,token);
    else sm_rgb_capture(start,end,invalidate,token);
}
static void main_pass(void) {
    unsigned mode=requested_mode;
    if((active_token&3u)!=mode)select_mode(mode);
    /* Engines count/latch every main pass, then check shared transport spacing
     * immediately before preparing/sending; no extra dispatch throttling. */
    if(mode==SM_QUALITY)sm_native_main();else sm_rgb_main();
}
static void key_after(int key,int pressed) {
    sm_rgb_key(key,pressed);sm_native_key(key,pressed);
}
static void knob_after(int index,short delta) {
    sm_rgb_knob(index,delta);sm_native_knob(index,delta);
}
int opl_init(struct opl_self *self) {
    struct opus_clock clock;
    /* Keep the firmware CALL bridge and code image in the reachable near pool.
     * opus_alloc supplies zeroed, 32-byte aligned data memory once per boot.
     * No callbacks or hooks may run if that allocation is unavailable. */
    sm_pixels=(struct sm_pixel_storage *)opus_alloc(self,sizeof(*sm_pixels));
    if(!sm_pixels)return -1;
    opus_clock(&clock);
    session_counter=controller_cycles()^((unsigned)clock.second<<24)^
        ((unsigned)clock.minute<<16)^((unsigned)clock.hour<<8)^0x534d000eu;
    requested_mode=valid_mode(opus_setting(self,"mode"));
    sm_burst=valid_burst(opus_setting(self,"send"));
    select_mode(requested_mode);
    opus_on_setting(self,setting_changed);
    opus_on_main_pass(self,main_pass);
    opus_on_key_after(self,key_after);
    opus_on_knob_after(self,knob_after);
    return 0;
}
