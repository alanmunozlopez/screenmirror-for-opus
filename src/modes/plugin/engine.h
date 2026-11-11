#ifndef SCREENMIRROR_ENGINES_H
#define SCREENMIRROR_ENGINES_H
/* 0.17: a single engine, sharp (RGB332 at the physical panel resolution).
 * The controller publishes one atomic (epoch << 2 | SM_SHARP) token once in
 * opl_init. Token 3 (epoch 0) is the never-selected value: capture stays off
 * until initialization succeeds. select runs only in main; capture runs only
 * in the final display hook. */
#define SM_SHARP 3u
/* RGB332 uses at most 400x200 bytes per image. Image-sized main-only LZ/RLE
 * work buffers live here, never in near static memory (see 0.14.0). */
struct sm_pixel_storage {
    unsigned char snapshots[3][80000];
    unsigned char lz_buffer[80004],lz_trial[80004];
    unsigned char reference[80000];
    unsigned char compressed[80000];
};
/* Large pixels belong in the core's data pool, not the CALL-reachable code
 * image. One allocation in opl_init for this boot. */
extern struct sm_pixel_storage *sm_pixels;
/* Messages the engine may send per main pass, 1..4 (default 4). 1 keeps the 0.14 cadence exactly. */
extern volatile unsigned sm_burst;
/* "screen mirror" setting: 1 = on (default), 0 = off. Off: no capture, no MIDI. */
extern volatile unsigned sm_screen_on;
int sm_transport_ready(void);
void sm_send_events(const unsigned int *events,int count);
unsigned sm_new_session(void);
void sm_rgb_select(unsigned token,unsigned session);
void sm_rgb_capture(void *start,void *end,int invalidate,unsigned token);
void sm_rgb_main(void);
void sm_rgb_key(int key,int pressed);
void sm_rgb_knob(int index,short delta);
#endif
