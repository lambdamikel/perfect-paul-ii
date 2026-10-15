/* Render what the Apple II card will say, on a PC.
   SPDX-License-Identifier: GPL-3.0-or-later

   This links the firmware's own pp2_sp0256.c and feeds it one byte at a time
   exactly as the slot does, at the card's real 11025 Hz output rate and
   through the same resampler. So a protocol or allophone mistake shows up in a
   WAV here rather than in a slot, and a demo's duration can be measured before
   its pacing loops are written.

     ./pp2_render -o hi.wav "HELLO THERE"           # English, the default mode
     ./pp2_render -a -o hi.wav "HH1 EH LL OW"      # allophone mode
     ./pp2_render -p 219,199,237,245 -o x.wav       # raw POKEs, as POKEd
     ./pp2_render -q "27 7 45 53"                  # duration only, no file  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "pp2_sp0256.h"
#include "sp0256_speak.h"
#include "sp0256_resample.h"

#define OUT_RATE 11025u          /* the card's audio sample rate */
#define TAIL_SAMPLES (OUT_RATE / 20u)   /* 50 ms, so a final stop is audible */

static void put32(FILE *f, uint32_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f);
                                         fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f); }
static void put16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }

static void wav_header(FILE *f, uint32_t rate, uint32_t samples)
{
    const uint32_t data = samples * 2u;
    fwrite("RIFF", 1, 4, f); put32(f, 36u + data); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put32(f, 16u); put16(f, 1); put16(f, 1);
    put32(f, rate); put32(f, rate * 2u); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, data);
}

static void usage(void)
{
    fputs("usage: pp2_render [-o out.wav] [-q] [-p bytes] [text ...]\n"
          "  text        one line of allophone names or numbers, as the card\n"
          "              receives it before a CR\n"
          "  -p bytes    comma-separated decimal byte values to feed instead,\n"
          "              i.e. exactly what BASIC would POKE (192+code, 144 ...)\n"
          "  -o file     WAV output, 16-bit mono at 11025 Hz\n"
          "  -a / -t     allophone / English text mode (default: text)\n"
          "  -q          report only, write nothing\n", stderr);
}

int main(int argc, char **argv)
{
    const char *out = NULL;
    const char *pokes = NULL;
    int quiet_file = 0;
    int i = 1;

    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)      out = argv[++i];
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) pokes = argv[++i];
        else if (!strcmp(argv[i], "-q"))                 quiet_file = 1;
        else if (!strcmp(argv[i], "-a"))                 pp2_sp_set_mode(PP2_SP_ALLOPHONE);
        else if (!strcmp(argv[i], "-t"))                 pp2_sp_set_mode(PP2_SP_TEXT);
        else if (!strcmp(argv[i], "-h"))                 { usage(); return 0; }
        else break;
    }

    const pp2_sp_mode_t mode = pp2_sp_get_mode();
    pp2_sp_init();
    pp2_sp_set_mode(mode);      /* init resets the chip, not the chosen mode */
    pp2_sp_input_t in;
    memset(&in, 0, sizeof in);

    if (pokes) {
        /* Feed raw byte values, so the $C0 raw channel and the stop byte are
           exercised through the same path the slot uses. */
        const char *p = pokes;
        while (*p) {
            char *end = NULL;
            long v = strtol(p, &end, 10);
            if (end == p) break;
            if (v < 0 || v > 255) {
                fprintf(stderr, "pp2_render: %ld is not a byte\n", v);
                return 1;
            }
            pp2_sp_feed(&in, (uint8_t)v);
            pp2_sp_service();   /* no second core here: stand in for it */
            p = (*end == ',') ? end + 1 : end;
        }
    } else {
        if (i >= argc) { usage(); return 1; }
        for (int a = i; a < argc; a++) {
            for (const char *c = argv[a]; *c; c++) {
                pp2_sp_feed(&in, (uint8_t)*c);
                pp2_sp_service();
            }
            if (a + 1 < argc) { pp2_sp_feed(&in, (uint8_t)' '); pp2_sp_service(); }
        }
        pp2_sp_feed(&in, (uint8_t)'\r');   /* the POKE DR,13 every demo ends on */
        pp2_sp_service();
    }

    FILE *f = NULL;
    if (!quiet_file) {
        if (!out) out = "pp2.wav";
        f = fopen(out, "wb");
        if (!f) { perror(out); return 1; }
        wav_header(f, OUT_RATE, 0u);
    }

    sp0256_rs_t rs = {0};
    const uint32_t inc = (uint32_t)(((uint64_t)SP0256_SAMPLE_RATE << 16) / OUT_RATE);

    uint32_t written = 0u, tail = 0u;
    int32_t peak = 0;
    for (;;) {
        pp2_sp_service();
        const int done = pp2_sp_staged() == 0u && sp0256_speak_idle();
        if (done && ++tail > TAIL_SAMPLES) break;
        if (!done) tail = 0u;

        const int16_t s = sp0256_rs_next(&rs, inc, pp2_sp_sample);
        if (s > peak) peak = s;
        if (-s > peak) peak = -s;
        if (f) { fputc((uint8_t)(s & 0xff), f); fputc((uint8_t)((s >> 8) & 0xff), f); }
        if (++written > OUT_RATE * 600u) {    /* ten minutes: something is wrong */
            fprintf(stderr, "pp2_render: runaway, stopping\n");
            break;
        }
    }

    if (f) {
        fseek(f, 0, SEEK_SET);
        wav_header(f, OUT_RATE, written);
        fclose(f);
    }

    printf("allophones=%u rejected=%u dropped=%u  samples=%u  %.3f s  peak=%d\n",
           (unsigned)pp2_sp_spoken(), (unsigned)pp2_sp_rejected(),
           (unsigned)pp2_sp_dropped(), (unsigned)written,
           (double)written / (double)OUT_RATE, (int)peak);
    if (!quiet_file) printf("wrote %s\n", out);
    return pp2_sp_rejected() ? 2 : 0;
}
