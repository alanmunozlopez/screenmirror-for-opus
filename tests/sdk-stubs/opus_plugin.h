#ifndef SCREENMIRROR_HOST_OPUS_PLUGIN_H
#define SCREENMIRROR_HOST_OPUS_PLUGIN_H
/* Test-only declarations of the pinned public OPUS API. Production builds use
 * the real SDK. These types/prototypes provide no firmware or core code. */
struct opl_self { unsigned size; const char *id; const char *folder; int slot; };
struct opus_clock { short year; unsigned char month,day,hour,minute,second,weekday; };
int opl_init(struct opl_self *self);
void *opus_alloc(struct opl_self *self,unsigned bytes);
void opus_clock(struct opus_clock *clock);
int opus_setting(struct opl_self *self,const char *name);
void opus_on_setting(struct opl_self *self,void (*fn)(const char *,int));
void opus_on_main_pass(struct opl_self *self,void (*fn)(void));
void opus_on_key_after(struct opl_self *self,void (*fn)(int,int));
void opus_on_knob_after(struct opl_self *self,void (*fn)(int,short));
#endif
