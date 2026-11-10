#ifndef SCREENMIRROR_ENGINES_H
#define SCREENMIRROR_ENGINES_H
/* The controller publishes one atomic (epoch << 2 | mode) token.
 * select runs only in main; capture runs only in the final display hook.
 * Engine selection never resets display-owned publication or mapping state. */
enum sm_mode { SM_FAST=0, SM_BALANCED=1, SM_QUALITY=2 };
/* Only the selected engine may interpret these pools. On every selection main
 * abandons its former job/reference and requires a fresh display token before
 * reading pixels. The sole display producer publishes after completing a copy. */
union sm_snapshot_storage {
    unsigned short native[3][80000];
    unsigned char rgb[3][51200];
};
union sm_reference_storage {
    unsigned short native[80000];
    unsigned char rgb[51200];
};
/* Large pixels belong in the core's data pool, not the CALL-reachable code
 * image. One allocation in opl_init is shared for this boot by all profiles. */
struct sm_pixel_storage {
    union sm_snapshot_storage snapshots;
    union sm_reference_storage reference;
};
extern struct sm_pixel_storage *sm_pixels;
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
