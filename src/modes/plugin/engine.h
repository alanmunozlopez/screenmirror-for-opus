#ifndef SCREENMIRROR_ENGINES_H
#define SCREENMIRROR_ENGINES_H
/* The controller publishes one atomic (epoch << 2 | mode) token.
 * select runs only in main; capture runs only in the final display hook.
 * Engine selection never resets display-owned publication or mapping state. */
enum sm_mode { SM_FAST=0, SM_BALANCED=1, SM_QUALITY=2, SM_SHARP=3 };
/* Token 3 (epoch 0) is the never-selected value: every real selection uses epoch >= 1. */
/* Only the selected engine may interpret these pools. On every selection main
 * abandons its former job/reference and requires a fresh display token before
 * reading pixels. The sole display producer publishes after completing a copy. */
/* RGB332 (fast/balanced/sharp) uses at most 400x200 bytes per image. Its main-only
 * LZ work buffers live in the bytes the RGB565 snapshots would otherwise use, so
 * sharp needs no extra allocation and no near static memory (see 0.14.0). */
struct sm_rgb_snapshots {
    unsigned char image[3][80000];
    unsigned char lz_buffer[80004],lz_trial[80004];
};
struct sm_rgb_reference {
    unsigned char image[80000];
    unsigned char compressed[80000];
};
union sm_snapshot_storage {
    unsigned short native[3][80000];
    struct sm_rgb_snapshots rgb;
};
union sm_reference_storage {
    unsigned short native[80000];
    struct sm_rgb_reference rgb;
};
/* Large pixels belong in the core's data pool, not the CALL-reachable code
 * image. One allocation in opl_init is shared for this boot by all profiles. */
struct sm_pixel_storage {
    union sm_snapshot_storage snapshots;
    union sm_reference_storage reference;
};
extern struct sm_pixel_storage *sm_pixels;
/* Messages an engine may send per main pass, 1..4 (default 4). 1 keeps the 0.14 cadence exactly. */
extern volatile unsigned sm_burst;
int sm_transport_ready(void);
void sm_send_events(const unsigned int *events,int count);
unsigned sm_new_session(void);
void sm_rgb_select(unsigned token,unsigned session);
void sm_native_select(unsigned token,unsigned session);
void sm_rgb_capture(void *start,void *end,int invalidate,unsigned token);
void sm_native_capture(void *start,void *end,int invalidate,unsigned token);
void sm_rgb_main(void);
void sm_native_main(void);
void sm_rgb_key(int key,int pressed);
void sm_native_key(int key,int pressed);
void sm_rgb_knob(int index,short delta);
void sm_native_knob(int index,short delta);
#endif
