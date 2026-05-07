#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>

extern char* boot_cmdline;
extern char* boot_video_request;
extern char* boot_video_mode;
extern char* boot_input_mode;
extern int boot_safe_mode;
extern int boot_sound_enabled;
extern int boot_usb_debug;

#endif
