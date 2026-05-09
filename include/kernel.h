#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>

extern int booting;
extern char* boot_cmdline;
extern char* boot_video_request;
extern char* boot_video_mode;
extern char* boot_input_mode;
extern int boot_safe_mode;
extern int boot_sound_enabled;
extern int boot_usb_debug;
extern uint32_t boot_multiboot_info;
extern uint32_t boot_multiboot_total_size;

#endif
