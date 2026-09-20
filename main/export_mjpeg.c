// =====================================================================
//  Showreel  --  MJPEG video export (see export_mjpeg.h)
// =====================================================================
//
//  The file is a plain AVI 1.0 (RIFF, well under 1 GB): one video
//  stream, codec MJPG, one JPEG per '00dc' chunk, and an idx1 index. The
//  header's frame totals and the chunk sizes are patched in at the end,
//  when they are known; the index entries are kept in memory until then.

#include "export_mjpeg.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "bsp/device.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "reel.h"
#include "synthengine3d.h"
#include "testkit/showtime.h"

// stb_image_write (public domain, main/third_party/): only its JPEG
// writer is used, through a callback, so no stdio variants; its few
// allocations go to PSRAM. Not STB_IMAGE_WRITE_STATIC: as static
// functions its unused writers (PNG, BMP, TGA, HDR) would each warn.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_MALLOC(sz)        heap_caps_malloc((sz), MALLOC_CAP_SPIRAM)
#define STBIW_REALLOC(p, newsz) heap_caps_realloc((p), (newsz), MALLOC_CAP_SPIRAM)
#define STBIW_FREE(p)           heap_caps_free(p)
#define STBIW_ASSERT(x)         ((void)0)
#include "third_party/stb_image_write.h"

static char const TAG[] = "export";

// --- Growable PSRAM byte buffer (one JPEG) ------------------------------

typedef struct {
    uint8_t* p;
    size_t   n, cap;
    bool     failed;
} bytes_t;

static void bytes_put(void* ctx, void* data, int size) {
    bytes_t* b = ctx;
    if (b->failed || size <= 0) return;
    if (b->n + (size_t)size > b->cap) {
        size_t cap = b->cap ? b->cap : 65536;
        while (cap < b->n + (size_t)size) cap *= 2;
        uint8_t* np = heap_caps_realloc(b->p, cap, MALLOC_CAP_SPIRAM);
        if (np == NULL) {
            b->failed = true;
            return;
        }
        b->p   = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, data, (size_t)size);
    b->n += (size_t)size;
}

// --- AVI writing ----------------------------------------------------------

typedef struct {
    uint32_t offset;  // from the 'movi' fourcc
    uint32_t size;
} idx_t;

static FILE*    s_f;
static uint8_t* s_iobuf;
static uint8_t* s_rgb;   // one frame, RGB888, logical orientation
static bytes_t  s_jpeg;  // one frame, encoded
static idx_t*   s_idx;   // one entry per frame
static int      s_frames, s_idx_cap;
static uint32_t s_max_chunk;
static long     s_movi_pos;  // file offset of the 'movi' fourcc
static bool     s_started, s_failed;
static int      s_w, s_h;
static int64_t  s_t0_us, s_enc_us, s_io_us;

// Header field offsets, patched at the end.
static long s_riff_size_pos, s_avih_frames_pos, s_avih_bufsize_pos, s_strh_length_pos, s_strh_bufsize_pos,
    s_movi_size_pos;

static void put_u32(uint32_t v) {
    uint8_t const b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    fwrite(b, 1, 4, s_f);
}
static void put_u16(uint16_t v) {
    uint8_t const b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    fwrite(b, 1, 2, s_f);
}
static void put_cc(char const cc[4]) {
    fwrite(cc, 1, 4, s_f);
}
static void patch_u32(long pos, uint32_t v) {
    fseek(s_f, pos, SEEK_SET);
    put_u32(v);
}

static void write_headers(void) {
    uint32_t const us_per_frame = 1000000u / EXPORT_FPS;
    put_cc("RIFF");
    s_riff_size_pos = ftell(s_f);
    put_u32(0);  // patched
    put_cc("AVI ");

    put_cc("LIST");
    put_u32(4 + (8 + 56) + (8 + 4 + (8 + 56) + (8 + 40)));
    put_cc("hdrl");

    put_cc("avih");  // MainAVIHeader
    put_u32(56);
    put_u32(us_per_frame);
    put_u32(0);     // max bytes/s: unknown
    put_u32(0);     // padding granularity
    put_u32(0x10);  // AVIF_HASINDEX
    s_avih_frames_pos = ftell(s_f);
    put_u32(0);  // total frames, patched
    put_u32(0);  // initial frames
    put_u32(1);  // streams
    s_avih_bufsize_pos = ftell(s_f);
    put_u32(0);  // suggested buffer size, patched
    put_u32((uint32_t)s_w);
    put_u32((uint32_t)s_h);
    for (int i = 0; i < 4; i++) put_u32(0);

    put_cc("LIST");
    put_u32(4 + (8 + 56) + (8 + 40));
    put_cc("strl");

    put_cc("strh");  // AVIStreamHeader
    put_u32(56);
    put_cc("vids");
    put_cc("MJPG");
    put_u32(0);           // flags
    put_u16(0);           // priority
    put_u16(0);           // language
    put_u32(0);           // initial frames
    put_u32(1);           // scale
    put_u32(EXPORT_FPS);  // rate: rate / scale = frames per second
    put_u32(0);           // start
    s_strh_length_pos = ftell(s_f);
    put_u32(0);  // length in frames, patched
    s_strh_bufsize_pos = ftell(s_f);
    put_u32(0);            // suggested buffer size, patched
    put_u32(0xFFFFFFFFu);  // quality: default
    put_u32(0);            // sample size: varies
    put_u16(0);
    put_u16(0);
    put_u16((uint16_t)s_w);
    put_u16((uint16_t)s_h);

    put_cc("strf");  // BITMAPINFOHEADER
    put_u32(40);
    put_u32(40);
    put_u32((uint32_t)s_w);
    put_u32((uint32_t)s_h);
    put_u16(1);   // planes
    put_u16(24);  // bits per pixel
    put_cc("MJPG");
    put_u32((uint32_t)(s_w * s_h * 3));
    for (int i = 0; i < 4; i++) put_u32(0);

    put_cc("LIST");
    s_movi_size_pos = ftell(s_f);
    put_u32(0);  // patched
    s_movi_pos = ftell(s_f);
    put_cc("movi");
}

static void finish_file(void) {
    // Index: offsets relative to the 'movi' fourcc, as players expect.
    put_cc("idx1");
    put_u32((uint32_t)s_frames * 16u);
    for (int i = 0; i < s_frames; i++) {
        put_cc("00dc");
        put_u32(0x10);  // AVIIF_KEYFRAME: every MJPEG frame stands alone
        put_u32(s_idx[i].offset);
        put_u32(s_idx[i].size);
    }
    long const end = ftell(s_f);
    patch_u32(s_riff_size_pos, (uint32_t)(end - 8));
    patch_u32(s_avih_frames_pos, (uint32_t)s_frames);
    patch_u32(s_avih_bufsize_pos, s_max_chunk);
    patch_u32(s_strh_length_pos, (uint32_t)s_frames);
    patch_u32(s_strh_bufsize_pos, s_max_chunk);
    // 'movi' list size: from its fourcc up to the index.
    long const idx_pos = end - 8 - (long)s_frames * 16;
    patch_u32(s_movi_size_pos, (uint32_t)(idx_pos - s_movi_pos));
    fclose(s_f);
    s_f = NULL;
}

// --- Frame capture ----------------------------------------------------------

// The framebuffer (RGB565, raw panel orientation, maybe byte-swapped) ->
// RGB888 in logical orientation, the way the screen looks.
static void grab_rgb(pax_buf_t* fb) {
    uint16_t const* px  = pax_buf_get_pixels(fb);
    bool const      rev = fb->reverse_endianness;
    uint8_t*        o   = s_rgb;
    for (int y = 0; y < s_h; y++) {
        for (int x = 0; x < s_w; x++) {
            uint16_t v = px[direct_565_logical_index(x, y)];
            if (rev) v = (uint16_t)__builtin_bswap16(v);
            uint8_t const r = (uint8_t)((v >> 11) & 0x1F), g = (uint8_t)((v >> 5) & 0x3F), b = (uint8_t)(v & 0x1F);
            *o++ = (uint8_t)((r << 3) | (r >> 2));
            *o++ = (uint8_t)((g << 2) | (g >> 4));
            *o++ = (uint8_t)((b << 3) | (b >> 2));
        }
    }
}

static void fail(char const* what) {
    ESP_LOGE(TAG, "%s -- export aborted", what);
    s_failed = true;
    if (s_f != NULL) {
        fclose(s_f);
        s_f = NULL;
    }
}

void export_begin(void) {
    s_w = DISPLAY_LOG_W;
    s_h = DISPLAY_LOG_H;
    mkdir("/sd/showreel", 0777);
    s_rgb   = heap_caps_malloc((size_t)(s_w * s_h * 3), MALLOC_CAP_SPIRAM);
    s_iobuf = heap_caps_malloc(32768, MALLOC_CAP_SPIRAM);
    s_f     = fopen(EXPORT_PATH, "wb");
    if (s_rgb == NULL || s_iobuf == NULL || s_f == NULL) {
        fail(s_f == NULL ? "cannot open " EXPORT_PATH : "out of PSRAM");
        return;
    }
    setvbuf(s_f, (char*)s_iobuf, _IOFBF, 32768);
    write_headers();
    ESP_LOGI(TAG, "exporting the reel to %s: %dx%d, %d fps, JPEG quality %d", EXPORT_PATH, s_w, s_h, EXPORT_FPS,
             EXPORT_QUALITY);
}

void export_update(void) {
    if (s_started || s_failed) return;
    // First frame: show time 0, fixed steps of 1/EXPORT_FPS from here on,
    // the playlist from its first scene.
    s_started = true;
    showtime_set_fixed_step((float)EXPORT_FPS);
    showtime_set(0.0);
    reel_restart();
    s_t0_us = esp_timer_get_time();
}

void export_frame(pax_buf_t* fb) {
    if (s_failed || !s_started) return;

    // Done when the playlist has wrapped round to its first scene again.
    if (reel_cycles() > 0) {
        finish_file();
        int64_t const total = esp_timer_get_time() - s_t0_us;
        ESP_LOGI(TAG, "done: %d frames (%.1f s of video) in %.1f s; encode %.0f ms/frame, write %.0f ms/frame -> %s",
                 s_frames, (double)s_frames / EXPORT_FPS, (double)total / 1e6,
                 (double)s_enc_us / 1000.0 / (double)s_frames, (double)s_io_us / 1000.0 / (double)s_frames,
                 EXPORT_PATH);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(300));
        audio_mixer_shutdown();
        bsp_device_restart_to_launcher();
        return;
    }

    int64_t const t0 = esp_timer_get_time();
    grab_rgb(fb);
    s_jpeg.n = 0;
    if (!stbi_write_jpg_to_func(bytes_put, &s_jpeg, s_w, s_h, 3, s_rgb, EXPORT_QUALITY) || s_jpeg.failed) {
        fail("JPEG encoding failed");
        return;
    }
    int64_t const t1 = esp_timer_get_time();

    if (s_frames == s_idx_cap) {
        int const cap = s_idx_cap ? s_idx_cap * 2 : 1024;
        idx_t*    ni  = heap_caps_realloc(s_idx, (size_t)cap * sizeof(idx_t), MALLOC_CAP_SPIRAM);
        if (ni == NULL) {
            fail("out of PSRAM for the index");
            return;
        }
        s_idx     = ni;
        s_idx_cap = cap;
    }
    long const     pos  = ftell(s_f);
    uint32_t const size = (uint32_t)s_jpeg.n;
    put_cc("00dc");
    put_u32(size);
    if (fwrite(s_jpeg.p, 1, size, s_f) != size) {
        fail("write failed (card full?)");
        return;
    }
    if (size & 1) fputc(0, s_f);  // chunks are word-aligned
    s_idx[s_frames++] = (idx_t){(uint32_t)(pos - s_movi_pos), size};
    if (size > s_max_chunk) s_max_chunk = size;
    int64_t const t2 = esp_timer_get_time();

    s_enc_us += t1 - t0;
    s_io_us  += t2 - t1;
    if (s_frames % EXPORT_FPS == 0) {
        ESP_LOGI(TAG, "frame %d (%s, t=%.2f): %u bytes, encode %lld ms, write %lld ms", s_frames, reel_scene_name(),
                 reel_scene_time(), (unsigned)size, (long long)((t1 - t0) / 1000), (long long)((t2 - t1) / 1000));
    }
}
