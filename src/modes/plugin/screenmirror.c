#include "opus_plugin.h"
#include "op1fw.h"
#include "engine.h"

/* Shared pixel pool, allocated once in opl_init and used only by the sharp engine. */
struct sm_pixel_storage *sm_pixels;
volatile unsigned sm_burst=4u,sm_screen_on=1u;

/* 0.17: sharp is the only mode. opl_init publishes one token after the engine
 * is ready; display captures only once that token is valid. The settings
 * callback only changes the send rate and the screen on/off switch. */
#define SEND_CYCLES 2000000u
static volatile unsigned active_token=3u;
static unsigned session_counter,last_usb_send,have_usb_send;

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
unsigned sm_new_session(void) {
    /* Called on main only, including physical geometry changes in the engine. */
    if(!++session_counter)session_counter=1u;
    return session_counter;
}
int sm_transport_ready(void) {
    return !have_usb_send || (unsigned)(controller_cycles()-last_usb_send)>=SEND_CYCLES;
}
void sm_send_events(const unsigned int *events,int count) {
    /* The engine is the sole caller. Spacing is measured after the actual send returns. */
    fw_midi_send(0,events,count);
    last_usb_send=controller_cycles();have_usb_send=1u;
}
static void select_sharp(void) {
    unsigned token=(1u<<2)|SM_SHARP; /* Epoch 1: never the uninitialized token 3. */
    sm_rgb_select(token,sm_new_session());
    controller_barrier();active_token=token;
}
static unsigned valid_burst(int value) {
    return value>=0 && value<=3 ? (unsigned)value+1u:4u;
}
static unsigned valid_screen(int value) {
    return value==1 ? 0u:1u; /* Choice 0 "on" (and anything unknown) keeps the mirror on. */
}
static void setting_changed(const char *name,int value) {
    if(name && name[0]=='s' && name[1]=='e' && name[2]=='n' &&
       name[3]=='d' && name[4]==0)sm_burst=valid_burst(value);
    if(name && name[0]=='s' && name[1]=='c' && name[2]=='r' && name[3]=='e' &&
       name[4]=='e' && name[5]=='n' && name[6]==0)sm_screen_on=valid_screen(value);
}
void screenmirror_capture_flush(void *start,void *end,int invalidate) {
    unsigned token=active_token;
    if(!(token>>2))return; /* Not initialized, or opl_init failed. */
    sm_rgb_capture(start,end,invalidate,token);
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
    sm_burst=valid_burst(opus_setting(self,"send"));
    sm_screen_on=valid_screen(opus_setting(self,"screen"));
    select_sharp();
    opus_on_setting(self,setting_changed);
    opus_on_main_pass(self,sm_rgb_main);
    opus_on_key_after(self,sm_rgb_key);
    opus_on_knob_after(self,sm_rgb_knob);
    return 0;
}
