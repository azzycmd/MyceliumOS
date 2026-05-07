#include "vga.h"
#include "io.h"
#include "lib.h"
#include <stdint.h>

#define CELL_W 8
#define CELL_H 16
#define GLYPH_W 8
#define GLYPH_H 8
#define GLYPH_OFFSET_X 0
#define GLYPH_OFFSET_Y ((CELL_H - GLYPH_H) / 2)
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define FB_VIRT_BASE 0xE0000000
#define FB_MAX_MAP_SIZE (64 * 1024 * 1024)
#define PAGE_PRESENT 0x001
#define PAGE_WRITE 0x002
#define MAX_TEXT_COLS 160
#define MAX_TEXT_ROWS 80
#define FB_TERM_MAX_COLS 140
#define FB_TERM_MAX_ROWS 70
#define PGLIM (MAX_TEXT_COLS * MAX_TEXT_ROWS)

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    uint8_t framebuffer_red_field_position;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_green_field_position;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_blue_field_position;
    uint8_t framebuffer_blue_mask_size;
} __attribute__((packed)) multiboot_tag_framebuffer_t;

char setcolor = BRANCO;
char coragr = BRANCO;
int cursorpos;
int cursor_x;
int cursor_y;
char cursor_x_str[10];
char cursor_y_str[10];

int max_rows = 40;
int max_cols = 140;

int usar_framebuffer = 0;
static uint8_t *fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t fb_bpp = 0;
static uint8_t fb_red_pos = 16;
static uint8_t fb_green_pos = 8;
static uint8_t fb_blue_pos = 0;
static uint8_t fb_red_size = 8;
static uint8_t fb_green_size = 8;
static uint8_t fb_blue_size = 8;
static uint32_t fb_size = 0;
static uint32_t fb_color_cache[16];
static char tela_chars[PGLIM];
static char tela_cores[PGLIM];
static int cursor_desenhado = 0;
static int cursor_desenhado_x = 0;
static int cursor_desenhado_y = 0;
static int text_fallback_geometry = 0;
static int fb_batch_redraw = 0;

static uint64_t pae_pdpt[4] __attribute__((aligned(4096)));
static uint64_t pae_low_pd[512] __attribute__((aligned(4096)));
static uint64_t pae_fb_pd[512] __attribute__((aligned(4096)));
static uint64_t pae_low_pts[32][512] __attribute__((aligned(4096)));
static uint64_t pae_fb_pts[32][512] __attribute__((aligned(4096)));
static uint64_t pae_mmio_pts[8][512] __attribute__((aligned(4096)));
static int pae_paging_enabled = 0;

static const uint8_t font8x8[96][8] = {
    ['!' - 32] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    ['"' - 32] = {0x6c,0x6c,0x48,0x00,0x00,0x00,0x00,0x00},
    ['#' - 32] = {0x24,0x7e,0x24,0x24,0x7e,0x24,0x00,0x00},
    ['$' - 32] = {0x18,0x3e,0x58,0x3c,0x1a,0x7c,0x18,0x00},
    ['%' - 32] = {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00},
    ['&' - 32] = {0x30,0x48,0x50,0x20,0x54,0x48,0x34,0x00},
    ['\'' - 32] = {0x18,0x18,0x10,0x00,0x00,0x00,0x00,0x00},
    ['(' - 32] = {0x0c,0x10,0x20,0x20,0x20,0x10,0x0c,0x00},
    [')' - 32] = {0x30,0x08,0x04,0x04,0x04,0x08,0x30,0x00},
    ['*' - 32] = {0x00,0x24,0x18,0x7e,0x18,0x24,0x00,0x00},
    ['+' - 32] = {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
    [',' - 32] = {0x00,0x00,0x00,0x00,0x18,0x18,0x10,0x20},
    ['-' - 32] = {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00},
    ['.' - 32] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    ['/' - 32] = {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00},
    ['0' - 32] = {0x3c,0x42,0x46,0x4a,0x52,0x62,0x3c,0x00},
    ['1' - 32] = {0x18,0x28,0x08,0x08,0x08,0x08,0x3e,0x00},
    ['2' - 32] = {0x3c,0x42,0x02,0x0c,0x30,0x40,0x7e,0x00},
    ['3' - 32] = {0x3c,0x42,0x02,0x1c,0x02,0x42,0x3c,0x00},
    ['4' - 32] = {0x0c,0x14,0x24,0x44,0x7e,0x04,0x04,0x00},
    ['5' - 32] = {0x7e,0x40,0x7c,0x02,0x02,0x42,0x3c,0x00},
    ['6' - 32] = {0x1c,0x20,0x40,0x7c,0x42,0x42,0x3c,0x00},
    ['7' - 32] = {0x7e,0x02,0x04,0x08,0x10,0x10,0x10,0x00},
    ['8' - 32] = {0x3c,0x42,0x42,0x3c,0x42,0x42,0x3c,0x00},
    ['9' - 32] = {0x3c,0x42,0x42,0x3e,0x02,0x04,0x38,0x00},
    [':' - 32] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    [';' - 32] = {0x00,0x18,0x18,0x00,0x18,0x18,0x10,0x20},
    ['<' - 32] = {0x0c,0x10,0x20,0x40,0x20,0x10,0x0c,0x00},
    ['=' - 32] = {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00},
    ['>' - 32] = {0x30,0x08,0x04,0x02,0x04,0x08,0x30,0x00},
    ['?' - 32] = {0x3c,0x42,0x02,0x0c,0x10,0x00,0x10,0x00},
    ['@' - 32] = {0x3c,0x42,0x5a,0x56,0x5c,0x40,0x3c,0x00},
    ['A' - 32] = {0x18,0x24,0x42,0x42,0x7e,0x42,0x42,0x00},
    ['B' - 32] = {0x7c,0x42,0x42,0x7c,0x42,0x42,0x7c,0x00},
    ['C' - 32] = {0x3c,0x42,0x40,0x40,0x40,0x42,0x3c,0x00},
    ['D' - 32] = {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00},
    ['E' - 32] = {0x7e,0x40,0x40,0x7c,0x40,0x40,0x7e,0x00},
    ['F' - 32] = {0x7e,0x40,0x40,0x7c,0x40,0x40,0x40,0x00},
    ['G' - 32] = {0x3c,0x42,0x40,0x4e,0x42,0x42,0x3c,0x00},
    ['H' - 32] = {0x42,0x42,0x42,0x7e,0x42,0x42,0x42,0x00},
    ['I' - 32] = {0x3e,0x08,0x08,0x08,0x08,0x08,0x3e,0x00},
    ['J' - 32] = {0x1e,0x04,0x04,0x04,0x44,0x44,0x38,0x00},
    ['K' - 32] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},
    ['L' - 32] = {0x40,0x40,0x40,0x40,0x40,0x40,0x7e,0x00},
    ['M' - 32] = {0x42,0x66,0x5a,0x5a,0x42,0x42,0x42,0x00},
    ['N' - 32] = {0x42,0x62,0x52,0x4a,0x46,0x42,0x42,0x00},
    ['O' - 32] = {0x3c,0x42,0x42,0x42,0x42,0x42,0x3c,0x00},
    ['P' - 32] = {0x7c,0x42,0x42,0x7c,0x40,0x40,0x40,0x00},
    ['Q' - 32] = {0x3c,0x42,0x42,0x42,0x4a,0x44,0x3a,0x00},
    ['R' - 32] = {0x7c,0x42,0x42,0x7c,0x48,0x44,0x42,0x00},
    ['S' - 32] = {0x3c,0x42,0x40,0x3c,0x02,0x42,0x3c,0x00},
    ['T' - 32] = {0x7f,0x08,0x08,0x08,0x08,0x08,0x08,0x00},
    ['U' - 32] = {0x42,0x42,0x42,0x42,0x42,0x42,0x3c,0x00},
    ['V' - 32] = {0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x00},
    ['W' - 32] = {0x42,0x42,0x42,0x5a,0x5a,0x66,0x42,0x00},
    ['X' - 32] = {0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x00},
    ['Y' - 32] = {0x41,0x22,0x14,0x08,0x08,0x08,0x08,0x00},
    ['Z' - 32] = {0x7e,0x02,0x04,0x18,0x20,0x40,0x7e,0x00},
    ['[' - 32] = {0x3c,0x20,0x20,0x20,0x20,0x20,0x3c,0x00},
    ['\\' - 32] = {0x40,0x20,0x10,0x08,0x04,0x02,0x00,0x00},
    [']' - 32] = {0x3c,0x04,0x04,0x04,0x04,0x04,0x3c,0x00},
    ['^' - 32] = {0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00},
    ['_' - 32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x7e,0x00},
    ['`' - 32] = {0x18,0x08,0x04,0x00,0x00,0x00,0x00,0x00},
    ['a' - 32] = {0x00,0x00,0x3c,0x02,0x3e,0x42,0x3e,0x00},
    ['b' - 32] = {0x40,0x40,0x5c,0x62,0x42,0x62,0x5c,0x00},
    ['c' - 32] = {0x00,0x00,0x3c,0x42,0x40,0x42,0x3c,0x00},
    ['d' - 32] = {0x02,0x02,0x3a,0x46,0x42,0x46,0x3a,0x00},
    ['e' - 32] = {0x00,0x00,0x3c,0x42,0x7e,0x40,0x3c,0x00},
    ['f' - 32] = {0x0e,0x10,0x10,0x7c,0x10,0x10,0x10,0x00},
    ['g' - 32] = {0x00,0x00,0x3a,0x46,0x46,0x3a,0x02,0x3c},
    ['h' - 32] = {0x40,0x40,0x5c,0x62,0x42,0x42,0x42,0x00},
    ['i' - 32] = {0x08,0x00,0x18,0x08,0x08,0x08,0x1c,0x00},
    ['j' - 32] = {0x04,0x00,0x0c,0x04,0x04,0x44,0x44,0x38},
    ['k' - 32] = {0x40,0x40,0x44,0x48,0x70,0x48,0x44,0x00},
    ['l' - 32] = {0x18,0x08,0x08,0x08,0x08,0x08,0x1c,0x00},
    ['m' - 32] = {0x00,0x00,0x76,0x49,0x49,0x49,0x49,0x00},
    ['n' - 32] = {0x00,0x00,0x5c,0x62,0x42,0x42,0x42,0x00},
    ['o' - 32] = {0x00,0x00,0x3c,0x42,0x42,0x42,0x3c,0x00},
    ['p' - 32] = {0x00,0x00,0x5c,0x62,0x62,0x5c,0x40,0x40},
    ['q' - 32] = {0x00,0x00,0x3a,0x46,0x46,0x3a,0x02,0x02},
    ['r' - 32] = {0x00,0x00,0x5c,0x62,0x40,0x40,0x40,0x00},
    ['s' - 32] = {0x00,0x00,0x3e,0x40,0x3c,0x02,0x7c,0x00},
    ['t' - 32] = {0x10,0x10,0x7c,0x10,0x10,0x12,0x0c,0x00},
    ['u' - 32] = {0x00,0x00,0x42,0x42,0x42,0x46,0x3a,0x00},
    ['v' - 32] = {0x00,0x00,0x42,0x42,0x24,0x24,0x18,0x00},
    ['w' - 32] = {0x00,0x00,0x41,0x49,0x49,0x49,0x36,0x00},
    ['x' - 32] = {0x00,0x00,0x42,0x24,0x18,0x24,0x42,0x00},
    ['y' - 32] = {0x00,0x00,0x42,0x42,0x46,0x3a,0x02,0x3c},
    ['z' - 32] = {0x00,0x00,0x7e,0x04,0x18,0x20,0x7e,0x00},
    ['{' - 32] = {0x0e,0x10,0x10,0x60,0x10,0x10,0x0e,0x00},
    ['|' - 32] = {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    ['}' - 32] = {0x70,0x08,0x08,0x06,0x08,0x08,0x70,0x00},
    ['~' - 32] = {0x00,0x00,0x32,0x4c,0x00,0x00,0x00,0x00},
};

static uint32_t rgb(enum vga_color cor) {
    static const uint32_t cores[16] = {
        0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
        0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
        0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
        0xff5555, 0xff55ff, 0xffff55, 0xffffff
    };
    return cores[cor & 0x0F];
}

static uint32_t fb_color(enum vga_color cor) {
    return fb_color_cache[cor & 0x0F];
}

static void fb_update_color_cache(void) {
    for (int i = 0; i < 16; i++) {
        uint32_t c = rgb((enum vga_color)i);
        uint8_t r = (uint8_t)((c >> 16) & 0xFF);
        uint8_t g = (uint8_t)((c >> 8) & 0xFF);
        uint8_t b = (uint8_t)(c & 0xFF);
        uint32_t rv = fb_red_size >= 8 ? r : (r >> (8 - fb_red_size));
        uint32_t gv = fb_green_size >= 8 ? g : (g >> (8 - fb_green_size));
        uint32_t bv = fb_blue_size >= 8 ? b : (b >> (8 - fb_blue_size));

        fb_color_cache[i] = (rv << fb_red_pos) |
                            (gv << fb_green_pos) |
                            (bv << fb_blue_pos);
    }
}

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!usar_framebuffer || x >= fb_width || y >= fb_height) return;

    uint8_t *pixel = fb_addr + (y * fb_pitch) + ((x * fb_bpp) / 8);
    if (fb_bpp == 32) {
        *((uint32_t*)pixel) = color;
    } else if (fb_bpp == 24) {
        pixel[0] = (uint8_t)(color & 0xFF);
        pixel[1] = (uint8_t)((color >> 8) & 0xFF);
        pixel[2] = (uint8_t)((color >> 16) & 0xFF);
    } else if (fb_bpp == 16) {
        *((uint16_t*)pixel) = (uint16_t)color;
    }
}

static void fb_clear_all(void) {
    if (!usar_framebuffer) return;

    uint32_t bytes = fb_pitch * fb_height;
    uint32_t dwords = bytes / 4;
    uint32_t tail = bytes & 3;
    uint8_t *dst = fb_addr;

    __asm__ volatile(
        "cld\n"
        "rep stosl"
        : "+D"(dst), "+c"(dwords)
        : "a"(0)
        : "memory"
    );

    for (uint32_t i = 0; i < tail; i++) {
        dst[i] = 0;
    }
}

static void enable_pae_paging() {
    uint32_t cr4;
    uint32_t cr0;
    uint32_t cr3 = (uint32_t)pae_pdpt;

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x20;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
    pae_paging_enabled = 1;
}

static void map_framebuffer(uint64_t phys_addr, uint32_t size) {
    uint64_t phys_base = phys_addr & ~0xFFFULL;
    uint32_t phys_offset = (uint32_t)(phys_addr & 0xFFF);
    uint32_t pages = (size + phys_offset + 0xFFF) / 0x1000;
    if (pages > (32 * 512)) pages = 32 * 512;

    for (int i = 0; i < 4; i++) pae_pdpt[i] = 0;
    for (int i = 0; i < 512; i++) {
        pae_low_pd[i] = 0;
        pae_fb_pd[i] = 0;
    }
    for (int pt = 0; pt < 32; pt++) {
        for (int i = 0; i < 512; i++) {
            pae_low_pts[pt][i] = 0;
            pae_fb_pts[pt][i] = 0;
        }
    }
    for (int pt = 0; pt < 8; pt++) {
        for (int i = 0; i < 512; i++) {
            pae_mmio_pts[pt][i] = 0;
        }
    }

    pae_pdpt[0] = ((uint64_t)(uint32_t)pae_low_pd) | PAGE_PRESENT | PAGE_WRITE;
    for (int pt = 0; pt < 32; pt++) {
        pae_low_pd[pt] = ((uint64_t)(uint32_t)pae_low_pts[pt]) | PAGE_PRESENT | PAGE_WRITE;
        for (int i = 0; i < 512; i++) {
            uint64_t phys = ((uint64_t)pt * 0x200000) + ((uint64_t)i * 0x1000);
            pae_low_pts[pt][i] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
    }

    pae_pdpt[3] = ((uint64_t)(uint32_t)pae_fb_pd) | PAGE_PRESENT | PAGE_WRITE;
    for (int pt = 0; pt < 32; pt++) {
        int pd_index = 256 + pt;
        pae_fb_pd[pd_index] = ((uint64_t)(uint32_t)pae_fb_pts[pt]) | PAGE_PRESENT | PAGE_WRITE;
    }

    for (uint32_t page = 0; page < pages; page++) {
        uint32_t pt = page / 512;
        uint32_t index = page % 512;
        pae_fb_pts[pt][index] = (phys_base + ((uint64_t)page * 0x1000)) | PAGE_PRESENT | PAGE_WRITE;
    }

    enable_pae_paging();
    fb_addr = (uint8_t*)(FB_VIRT_BASE + phys_offset);
}

void* vga_map_mmio_region(uint64_t phys_addr, uint32_t size, uint32_t virt_base) {
    uint64_t phys_base = phys_addr & ~0xFFFULL;
    uint32_t virt_aligned = virt_base & ~0xFFFU;
    uint32_t virt_offset = virt_base & 0xFFFU;
    uint32_t pages = (size + virt_offset + 0xFFF) / 0x1000;
    uint32_t pdpt_index = virt_aligned >> 30;
    uint32_t pd_index = (virt_aligned >> 21) & 0x1FF;

    if (!pae_paging_enabled) {
        return (void*)virt_base;
    }

    if (pdpt_index != 3 || pages > (8 * 512) || pd_index + ((pages + 511) / 512) > 512) {
        return 0;
    }

    pae_pdpt[3] = ((uint64_t)(uint32_t)pae_fb_pd) | PAGE_PRESENT | PAGE_WRITE;

    for (uint32_t pt = 0; pt < (pages + 511) / 512; pt++) {
        pae_fb_pd[pd_index + pt] = ((uint64_t)(uint32_t)pae_mmio_pts[pt]) |
                                   PAGE_PRESENT | PAGE_WRITE;
    }

    for (uint32_t page = 0; page < pages; page++) {
        uint32_t pt = page / 512;
        uint32_t index = page % 512;
        pae_mmio_pts[pt][index] = (phys_base + ((uint64_t)page * 0x1000)) |
                                  PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t cr3 = (uint32_t)pae_pdpt;
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    return (void*)virt_base;
}

static void setup_framebuffer_mapping(uint64_t phys_addr, uint32_t size) {
    uint64_t end = phys_addr + size;

    if (end <= 0x100000000ULL) {
        fb_addr = (uint8_t*)(uint32_t)phys_addr;
        return;
    }

    map_framebuffer(phys_addr, size);
}

static void desenha_celula(int x, int y, char ch, enum vga_color cor_texto, int cursor_ativo) {
    if (!usar_framebuffer) return;

    uint32_t px = (uint32_t)x * CELL_W;
    uint32_t py = (uint32_t)y * CELL_H;
    uint32_t fg = fb_color(cor_texto);
    uint32_t bg = fb_color(PRETO);

    unsigned char glyph = (unsigned char)ch;
    if (glyph < 32 || glyph > 127) glyph = ' ';

    for (int row = 0; row < CELL_H; row++) {
        int glyph_row = row - GLYPH_OFFSET_Y;
        uint8_t bits = 0;
        uint8_t *line = fb_addr + ((py + (uint32_t)row) * fb_pitch);
        if (glyph_row >= 0 && glyph_row < GLYPH_H) {
            bits = font8x8[glyph - 32][glyph_row];
        }

        if (fb_bpp == 32) {
            uint32_t *dst = (uint32_t*)(line + (px * 4));
            for (int col = 0; col < CELL_W; col++) {
                int glyph_col = col - GLYPH_OFFSET_X;
                int aceso = 0;
                if (glyph_col >= 0 && glyph_col < GLYPH_W) {
                    aceso = bits & (0x80 >> glyph_col);
                }
                if (cursor_ativo && row >= CELL_H - 2) aceso = 1;
                dst[col] = aceso ? fg : bg;
            }
        } else if (fb_bpp == 24) {
            uint8_t *dst = line + (px * 3);
            for (int col = 0; col < CELL_W; col++) {
                int glyph_col = col - GLYPH_OFFSET_X;
                int aceso = 0;
                uint32_t color;
                if (glyph_col >= 0 && glyph_col < GLYPH_W) {
                    aceso = bits & (0x80 >> glyph_col);
                }
                if (cursor_ativo && row >= CELL_H - 2) aceso = 1;
                color = aceso ? fg : bg;
                dst[(col * 3) + 0] = (uint8_t)(color & 0xFF);
                dst[(col * 3) + 1] = (uint8_t)((color >> 8) & 0xFF);
                dst[(col * 3) + 2] = (uint8_t)((color >> 16) & 0xFF);
            }
        } else if (fb_bpp == 16) {
            uint16_t *dst = (uint16_t*)(line + (px * 2));
            for (int col = 0; col < CELL_W; col++) {
                int glyph_col = col - GLYPH_OFFSET_X;
                int aceso = 0;
                if (glyph_col >= 0 && glyph_col < GLYPH_W) {
                    aceso = bits & (0x80 >> glyph_col);
                }
                if (cursor_ativo && row >= CELL_H - 2) aceso = 1;
                dst[col] = (uint16_t)(aceso ? fg : bg);
            }
        } else {
            for (int col = 0; col < CELL_W; col++) {
                int glyph_col = col - GLYPH_OFFSET_X;
                int aceso = 0;
                if (glyph_col >= 0 && glyph_col < GLYPH_W) {
                    aceso = bits & (0x80 >> glyph_col);
                }
                if (cursor_ativo && row >= CELL_H - 2) aceso = 1;
                fb_putpixel(px + col, py + row, aceso ? fg : bg);
            }
        }
    }
}

static void fb_scroll_terminal(void) {
    if (!usar_framebuffer) return;

    uint32_t bytes_per_pixel = fb_bpp / 8;
    if (bytes_per_pixel == 0) return;

    uint32_t terminal_height = (uint32_t)max_rows * CELL_H;
    uint32_t scroll_bytes = (uint32_t)CELL_H * fb_pitch;

    if (terminal_height > fb_height) terminal_height = fb_height;
    if (terminal_height <= CELL_H) return;

    uint32_t terminal_width = (uint32_t)max_cols * CELL_W * bytes_per_pixel;
    if (terminal_width > fb_pitch) terminal_width = fb_pitch;

    for (uint32_t y = 0; y < terminal_height - CELL_H; y++) {
        uint8_t *dst = fb_addr + (y * fb_pitch);
        uint8_t *src = dst + scroll_bytes;
        uint32_t dwords = terminal_width / 4;
        uint32_t tail = terminal_width & 3;

        __asm__ volatile(
            "cld\n"
            "rep movsl"
            : "+D"(dst), "+S"(src), "+c"(dwords)
            :
            : "memory"
        );
        for (uint32_t i = 0; i < tail; i++) {
            dst[i] = src[i];
        }
    }

    for (uint32_t y = terminal_height - CELL_H; y < terminal_height; y++) {
        uint8_t *dst = fb_addr + (y * fb_pitch);
        uint32_t dwords = terminal_width / 4;
        uint32_t tail = terminal_width & 3;

        __asm__ volatile(
            "cld\n"
            "rep stosl"
            : "+D"(dst), "+c"(dwords)
            : "a"(0)
            : "memory"
        );
        for (uint32_t i = 0; i < tail; i++) {
            dst[i] = 0;
        }
    }
}

static void grava_celula(int x, int y, char ch, enum vga_color cor_texto) {
    int index = y * max_cols + x;
    tela_chars[index] = ch;
    tela_cores[index] = cor_texto;

    if (usar_framebuffer) {
        if (!fb_batch_redraw) {
            desenha_celula(x, y, ch, cor_texto, 0);
        }
    } else {
        int offset = index * 2;
        video_memory[offset] = ch;
        video_memory[offset + 1] = cor_texto;
    }
}

static void redesenha_tela() {
    if (!usar_framebuffer) return;
    for (int y = 0; y < max_rows; y++) {
        for (int x = 0; x < max_cols; x++) {
            int index = y * max_cols + x;
            desenha_celula(x, y, tela_chars[index], tela_cores[index], 0);
        }
    }
}

static void limpa_cursor_desenhado(void) {
    if (!usar_framebuffer || !cursor_desenhado) return;
    if (cursor_desenhado_x < 0 || cursor_desenhado_x >= max_cols ||
        cursor_desenhado_y < 0 || cursor_desenhado_y >= max_rows) {
        cursor_desenhado = 0;
        return;
    }

    int index = cursor_desenhado_y * max_cols + cursor_desenhado_x;
    desenha_celula(cursor_desenhado_x, cursor_desenhado_y,
                   tela_chars[index], tela_cores[index], 0);
    cursor_desenhado = 0;
}

static void scroll_linhas(int linhas);

static void configurar_geometria_texto(void) {
    if (!usar_framebuffer) {
        max_cols = 80;
        max_rows = 25;
        return;
    }

    max_cols = (int)(fb_width / CELL_W);
    max_rows = (int)(fb_height / CELL_H);

    if (max_cols > FB_TERM_MAX_COLS) max_cols = FB_TERM_MAX_COLS;
    if (max_rows > FB_TERM_MAX_ROWS) max_rows = FB_TERM_MAX_ROWS;
    if (max_cols < 80) max_cols = 80;
    if (max_rows < 25) max_rows = 25;

    while (max_cols * max_rows > PGLIM && max_rows > 25) {
        max_rows--;
    }
    while (max_cols * max_rows > PGLIM && max_cols > 80) {
        max_cols--;
    }
}

void vga_init_from_multiboot(uint32_t mb_info) {
    usar_framebuffer = 0;
    text_fallback_geometry = 0;
    max_cols = 80;
    max_rows = 25;

    if (mb_info != 0) {
        multiboot_info_t *info = (multiboot_info_t*)mb_info;
        uint32_t offset = 8;

        while (offset + sizeof(multiboot_tag_t) <= info->total_size) {
            multiboot_tag_t *tag = (multiboot_tag_t*)(mb_info + offset);
            if (tag->type == 0 || tag->size == 0) break;

            if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
                multiboot_tag_framebuffer_t *fb = (multiboot_tag_framebuffer_t*)tag;
                if (fb->framebuffer_type == 1 &&
                    (fb->framebuffer_bpp == 32 || fb->framebuffer_bpp == 24 || fb->framebuffer_bpp == 16) &&
                    fb->framebuffer_width >= (uint32_t)(max_cols * CELL_W) &&
                    fb->framebuffer_height >= (uint32_t)(max_rows * CELL_H)) {
                    fb_pitch = fb->framebuffer_pitch;
                    fb_width = fb->framebuffer_width;
                    fb_height = fb->framebuffer_height;
                    fb_bpp = fb->framebuffer_bpp;
                    fb_red_pos = fb->framebuffer_red_field_position;
                    fb_green_pos = fb->framebuffer_green_field_position;
                    fb_blue_pos = fb->framebuffer_blue_field_position;
                    fb_red_size = fb->framebuffer_red_mask_size;
                    fb_green_size = fb->framebuffer_green_mask_size;
                    fb_blue_size = fb->framebuffer_blue_mask_size;
                    fb_update_color_cache();
                    fb_size = fb_pitch * fb_height;
                    if (fb_size > FB_MAX_MAP_SIZE) fb_size = FB_MAX_MAP_SIZE;
                    setup_framebuffer_mapping(fb->framebuffer_addr, fb_size);
                    usar_framebuffer = 1;
                    configurar_geometria_texto();
                }
                break;
            }

            offset += (tag->size + 7) & ~7;
        }
    }

    configurar_geometria_texto();

    for (int i = 0; i < max_rows * max_cols; i++) {
        tela_chars[i] = ' ';
        tela_cores[i] = setcolor;
    }
    redesenha_tela();
}

void vga_init_text(void) {
    usar_framebuffer = 0;
    text_fallback_geometry = 0;
    max_cols = 80;
    max_rows = 25;
    cursor_x = 0;
    cursor_y = 0;
    cursor_desenhado = 0;

    for (int i = 0; i < max_rows * max_cols; i++) {
        tela_chars[i] = ' ';
        tela_cores[i] = setcolor;
    }
}

void vga_init_text_fallback(uint32_t mb_info) {
    vga_init_from_multiboot(mb_info);

    if (!usar_framebuffer) {
        vga_init_text();
        return;
    }

    text_fallback_geometry = 1;
    max_cols = 80;
    max_rows = 25;
    cursor_x = 0;
    cursor_y = 0;
    cursor_desenhado = 0;

    for (int i = 0; i < max_rows * max_cols; i++) {
        tela_chars[i] = ' ';
        tela_cores[i] = setcolor;
    }
    redesenha_tela();
}

char* vga_mode_name(void) {
    if (usar_framebuffer && text_fallback_geometry) return "framebuffer-text";
    return usar_framebuffer ? "framebuffer" : "text";
}

uint32_t vga_framebuffer_width(void) {
    return fb_width;
}

uint32_t vga_framebuffer_height(void) {
    return fb_height;
}

uint8_t vga_framebuffer_bpp(void) {
    return fb_bpp;
}

static void fb_draw_scaled_char_fixed(uint32_t x, uint32_t y, char ch,
                                      enum vga_color fg_color,
                                      enum vga_color bg_color,
                                      uint32_t scale_fixed) {
    unsigned char glyph = (unsigned char)ch;
    if (glyph < 32 || glyph > 127) glyph = ' ';

    uint32_t fg = fb_color(fg_color);
    uint32_t bg = fb_color(bg_color);

    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = font8x8[glyph - 32][row];
        for (uint32_t col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;

            uint32_t x0 = x + ((col * scale_fixed) >> 8);
            uint32_t y0 = y + ((row * scale_fixed) >> 8);
            uint32_t x1 = x + ((((col + 1) * scale_fixed) + 255) >> 8);
            uint32_t y1 = y + ((((row + 1) * scale_fixed) + 255) >> 8);

            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;

            for (uint32_t py = y0; py < y1; py++) {
                for (uint32_t px = x0; px < x1; px++) {
                    fb_putpixel(px, py, color);
                }
            }
        }
    }
}

static void fb_draw_scaled_text_fixed(uint32_t x, uint32_t y, char *text,
                                      enum vga_color fg_color,
                                      enum vga_color bg_color,
                                      uint32_t scale_fixed) {
    uint32_t pen_x = x;
    uint32_t pen_y = y;
    uint32_t start_x = x;
    uint32_t advance = ((8 * scale_fixed) + 255) >> 8;
    uint32_t line_advance = advance + ((scale_fixed + 255) >> 8);

    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            pen_x = start_x;
            pen_y += line_advance;
            continue;
        }

        fb_draw_scaled_char_fixed(pen_x, pen_y, text[i], fg_color, bg_color,
                                  scale_fixed);
        pen_x += advance;
    }
}

static void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                         enum vga_color color) {
    uint32_t c = fb_color(color);
    for (uint32_t py = y; py < y + h && py < fb_height; py++) {
        for (uint32_t px = x; px < x + w && px < fb_width; px++) {
            fb_putpixel(px, py, c);
        }
    }
}

static void hex32_to_str(uint32_t value, char out[11]) {
    char *digits = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 8; i++) {
        out[2 + i] = digits[(value >> (28 - (i * 4))) & 0x0F];
    }
    out[10] = '\0';
}

static uint32_t panic_line(uint32_t x, uint32_t y, char *label, char *value,
                           enum vga_color color) {
    fb_draw_scaled_text_fixed(x, y, label, color, VERMELHO_ESCRO, 256);
    if (value) {
        fb_draw_scaled_text_fixed(x + 112, y, value, BRANCO, VERMELHO_ESCRO, 256);
    }
    return y + 12;
}

static uint32_t panic_hex_line(uint32_t x, uint32_t y, char *label,
                               uint32_t value, enum vga_color color) {
    char hex[11];
    hex32_to_str(value, hex);
    return panic_line(x, y, label, hex, color);
}

void vga_panic_screen(char *motivo) {
    if (!usar_framebuffer) {
        limpatela();
        cor(BRANCO);
        print("A fatal problem has been detected and MyceliumOS has been halted.\n");
        print("KERNEL_PANIC\n\n");
        print("        _.-^^---....,,--\n");
        print("    _--                  --_\n");
        print("   <                        >)\n");
        print("   |                         |\n");
        print("    \\._                   _./\n");
        print("       ```--. . , ; .--'''\n");
        print("             | |   |\n");
        print("          .-=||  | |=-.\n");
        print("          `-=#$%&%$#=-'\n");
        print("             | ;  :|\n\n");
        print("the mycelium is burning\n\n");
        print("If this is the first time you've seen this panic screen,\n");
        print("restart your computer. If this screen appears again, check\n");
        print("recent kernel changes, drivers, or hardware assumptions.\n\n");
        print("Motivo: ");
        print(motivo ? motivo : "falha fatal desconhecida");
        print("\n");
        print("EIP="); print_hex(panic_eip);
        print(" CS="); print_hex(panic_cs);
        print(" EFLAGS="); print_hex(panic_eflags);
        print("\nERR="); print_hex(panic_error_code);
        print(" VECTOR="); print_hex(panic_vector);
        print(" CR2="); print_hex(panic_cr2);
        print("\nCR0="); print_hex(panic_cr0);
        print(" CR3="); print_hex(panic_cr3);
        print(" CR4="); print_hex(panic_cr4);
        print("\nOrigem: ");
        print(panic_origin_file ? panic_origin_file : "?");
        print(":");
        char linha[16];
        itoa(panic_origin_line, linha);
        print(linha);
        print("\n");
        return;
    }

    fb_fill_rect(0, 0, fb_width, fb_height, VERMELHO_ESCRO);
    cursor_x = 0;
    cursor_y = 0;

    uint32_t x = 10;
    uint32_t y = 8;
    char line[16];
    uint32_t art_x = fb_width > 360 ? fb_width - 470 : 10;
    uint32_t art_y = 542;
    
    fb_draw_scaled_text_fixed(x, y, "A fatal problem has been detected and MyceliumOS has been halted to prevent damage to your computer.", BRANCO, VERMELHO_ESCRO, 256);
    y += 22;

    fb_draw_scaled_text_fixed(x, y, "KERNEL_PANIC", BRANCO, VERMELHO_ESCRO, 512);
    fb_draw_scaled_text_fixed(x + 224, y + 10, "MyceliumOS halted", CIANO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y,      "                             ____", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 12, "                     __,-~~/~    `---.", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 24, "                   _/_,---(      ,    )", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 36, "               __ /        <    /   )  \\___", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 48, "- ------===;;;'====------------------===;;;===----- -  -", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 60, "                  \\/  ~'~'~'~'~'~\\~'~)~'/", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 72, "                  (_ (   \\  (     >    \\)", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 84, "                   \\_( _ <         >_>'", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 96, "                      ~ `-i' ::>|--'", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 108, "                          I;|.|.|", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 120, "                         <|i::|i|`.", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 132, "                        (` ^''`-' ')", BRANCO, VERMELHO_ESCRO, 256);
    fb_draw_scaled_text_fixed(art_x, art_y + 144, "-//-/-//--/-//--/  the mycelium is burning  --/-//-/-/--", CIANO, VERMELHO_ESCRO, 256);
    y += 38;

    fb_draw_scaled_text_fixed(x, y, "If this is the first time you've seen this panic screen,", BRANCO, VERMELHO_ESCRO, 256);
    y += 14;
    fb_draw_scaled_text_fixed(x, y, "restart your computer. If this screen appears again, follow these steps: ", BRANCO, VERMELHO_ESCRO, 256);
    y += 22;
    fb_draw_scaled_text_fixed(x, y, "Check to make sure any new kernel code or driver is properly installed.", BRANCO, VERMELHO_ESCRO, 256);
    y += 14;
    fb_draw_scaled_text_fixed(x, y, "If this is real hardware, turn off your PC.", BRANCO, VERMELHO_ESCRO, 256);
    y += 22;
    fb_draw_scaled_text_fixed(x, y, "If problems continue, disable newly added devices, boot with minimal input, or inspect memory and interrupt state.", BRANCO, VERMELHO_ESCRO, 256);
    y += 22;

    y = panic_line(x, y, "Reason", motivo ? motivo : "unknown fatal failure", BRANCO);
    y += 14;
    y = panic_line(x, y, "Version", versao, CIANO);
    y += 14;
    y = panic_line(x, y, "Codename", codename, CIANO);
    y += 14;
    itoa((int)timer_ticks, line);
    y = panic_line(x, y, "Ticks", line, CIANO);
    y += 14;

    if (panic_origin_file) {
        y = panic_line(x, y, "Origin", panic_origin_file, AMARELO);
        itoa(panic_origin_line, line);
        y = panic_line(x, y, "Line", line, AMARELO);
        y += 14;
    }

    y += 8;
    fb_draw_scaled_text_fixed(x, y, "CPU context", BRANCO, VERMELHO_ESCRO, 256);
    y += 14;
    y = panic_hex_line(x, y, "Vector", panic_vector, AMARELO);
    y = panic_hex_line(x, y, "ErrCode", panic_error_code, AMARELO);
    y = panic_hex_line(x, y, "EIP", panic_eip, BRANCO);
    y = panic_hex_line(x, y, "CS", panic_cs, BRANCO);
    y = panic_hex_line(x, y, "EFLAGS", panic_eflags, BRANCO);

    y += 8;
    fb_draw_scaled_text_fixed(x, y, "Control registers", BRANCO, VERMELHO_ESCRO, 256);
    y += 14;
    y = panic_hex_line(x, y, "CR0", panic_cr0, CIANO);
    y = panic_hex_line(x, y, "CR2", panic_cr2, CIANO);
    y = panic_hex_line(x, y, "CR3", panic_cr3, CIANO);
    y = panic_hex_line(x, y, "CR4", panic_cr4, CIANO);

    y += 8;
    fb_draw_scaled_text_fixed(x, y, "Hint: EIP is the faulting instruction. CR2 is the fault address for #PF.", BRANCO, VERMELHO_ESCRO, 256);
}

void print(char* msg) {
    if (usar_framebuffer) {
        limpa_cursor_desenhado();
    }
    fb_batch_redraw = 0;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        }
        else if (msg[i] == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = max_cols - 1;
            }

            grava_celula(cursor_x, cursor_y, ' ', setcolor);
        }
        else if (msg[i] == '\t') {
            for (int j = 0; j < 4; j++) {
                grava_celula(cursor_x, cursor_y, ' ', setcolor);
                cursor_x++;
                if (cursor_x >= max_cols) {
                    cursor_x = 0;
                    cursor_y++;
                    break;
                }
            }
        }
        else {
            grava_celula(cursor_x, cursor_y, msg[i], setcolor);
            cursor_x++;
        }

        if (cursor_x >= max_cols) {
            cursor_x = 0;
            cursor_y++;
        }

        if (cursor_y >= max_rows) {
            int linhas = cursor_y - max_rows + 1;
            if (usar_framebuffer) {
                fb_batch_redraw = 1;
            }
            scroll_linhas(linhas);
            cursor_y -= linhas;
        }
    }
    if (usar_framebuffer && fb_batch_redraw) {
        redesenha_tela();
        fb_batch_redraw = 0;
    }
    cursor();
}

static void scroll_linhas(int linhas) {
    if (linhas <= 0) return;
    if (linhas > max_rows) linhas = max_rows;

    for (int y = 0; y < max_rows - linhas; y++) {
        for (int x = 0; x < max_cols; x++) {
            int dst = y * max_cols + x;
            int src = (y + linhas) * max_cols + x;
            tela_chars[dst] = tela_chars[src];
            tela_cores[dst] = tela_cores[src];

            if (!usar_framebuffer) {
                video_memory[dst * 2] = tela_chars[dst];
                video_memory[dst * 2 + 1] = tela_cores[dst];
            }
        }
    }

    for (int y = max_rows - linhas; y < max_rows; y++) {
        for (int x = 0; x < max_cols; x++) {
            int index = y * max_cols + x;
            tela_chars[index] = ' ';
            tela_cores[index] = setcolor;

            if (!usar_framebuffer) {
                video_memory[index * 2] = ' ';
                video_memory[index * 2 + 1] = setcolor;
            }
        }
    }

    if (usar_framebuffer) {
        if (!fb_batch_redraw) {
            if (linhas == 1) {
                fb_scroll_terminal();
            } else {
                redesenha_tela();
            }
        }
    }
}

void scroll() {
    scroll_linhas(1);
}

void set_cursor_pos(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= max_cols) x = max_cols - 1;
    if (y >= max_rows) y = max_rows - 1;
    cursor_x = x;
    cursor_y = y;
    cursor();
}

int get_cursor_x() {
    return cursor_x;
}

int get_cursor_y() {
    return cursor_y;
}

void cursor() {
    if (usar_framebuffer) {
        limpa_cursor_desenhado();
        int index = cursor_y * max_cols + cursor_x;
        desenha_celula(cursor_x, cursor_y, tela_chars[index], tela_cores[index], 1);
        cursor_desenhado = 1;
        cursor_desenhado_x = cursor_x;
        cursor_desenhado_y = cursor_y;
    } else {
        uint16_t pos = (cursor_y * max_cols) + cursor_x;

        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }

    itoa(cursor_x, cursor_x_str);
    itoa(cursor_y, cursor_y_str);
}

void limpatela() {
    if (usar_framebuffer) {
        fb_clear_all();
        for (int i = 0; i < max_rows * max_cols; i++) {
            tela_chars[i] = ' ';
            tela_cores[i] = setcolor;
        }
        cursor_x = 0;
        cursor_y = 0;
        cursor_desenhado = 0;
        cursor();
        return;
    }

    for (int y = 0; y < max_rows; y++) {
        for (int x = 0; x < max_cols; x++) {
            grava_celula(x, y, ' ', setcolor);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    cursor();
}

void cursor_on() {
    if (usar_framebuffer) {
        cursor();
        return;
    }

    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void cor(enum vga_color cor) {
    setcolor = cor;
}
