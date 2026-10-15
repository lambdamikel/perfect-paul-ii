/* What do the rules make of this? -- the Perfect Paul ][ side of run_words.sh.
   SPDX-License-Identifier: GPL-3.0-or-later

     ./nrl_test HEAD LIVE BROUGHT          # with the exception table
     ./nrl_test -x HEAD LIVE BROUGHT       # without it: the rules' own errors
     ./nrl_test -l                         # list the exception table
     ./nrl_test -w out.wav "HELLO THERE"   # and hear it                      */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "nrl.h"
#include "pp2_sp0256.h"
#include "sp0256_rom.h"
#include "sp0256_speak.h"
#include "sp0256_resample.h"

#define OUT_RATE 11025u

static void put32(FILE *f, uint32_t v){for(int i=0;i<4;i++)fputc((v>>(8*i))&0xff,f);}
static void put16(FILE *f, uint16_t v){fputc(v&0xff,f);fputc((v>>8)&0xff,f);}
static void wav_header(FILE *f, uint32_t rate, uint32_t samples)
{
    const uint32_t data = samples*2u;
    fwrite("RIFF",1,4,f); put32(f,36u+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); put32(f,16u); put16(f,1); put16(f,1);
    put32(f,rate); put32(f,rate*2u); put16(f,2); put16(f,16);
    fwrite("data",1,4,f); put32(f,data);
}

int main(int argc, char **argv)
{
    const char *wav = NULL;
    int i = 1;
    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-x"))                         nrl_set_exceptions(0);
        else if (!strcmp(argv[i], "-w") && i + 1 < argc)    wav = argv[++i];
        else if (!strcmp(argv[i], "-l")) {
            printf("%d measured respellings:\n", nrl_exception_count());
            for (int k = 0; k < nrl_exception_count(); k++)
                printf("  %-12s -> %s\n", nrl_exception_word(k), nrl_exception_said(k));
            return 0;
        }
        else break;
    }
    if (i >= argc) { fprintf(stderr, "usage: nrl_test [-x] [-w out.wav] text...\n"); return 1; }

    char joined[512] = {0};
    for (int a = i; a < argc; a++) {
        if (joined[0]) strncat(joined, " ", sizeof joined - strlen(joined) - 1);
        strncat(joined, argv[a], sizeof joined - strlen(joined) - 1);
    }

    char arpa[1024];
    nrl_text_to_arpa(joined, arpa, sizeof arpa);

    uint8_t allo[512];
    const size_t n = nrl_text_to_al2(joined, allo, sizeof allo);

    printf("%s\n", joined);
    printf("  arpa: %s\n", arpa);
    printf("  al2 : ");
    for (size_t k = 0; k < n; k++) printf("%s%s", sp0256_al2_label(allo[k]), k+1<n?" ":"\n");

    if (!wav) return 0;

    pp2_sp_init();
    for (size_t k = 0; k < n; k++) pp2_sp_push(allo[k]);

    FILE *f = fopen(wav, "wb");
    if (!f) { perror(wav); return 1; }
    wav_header(f, OUT_RATE, 0u);

    sp0256_rs_t rs = {0};
    const uint32_t inc = (uint32_t)(((uint64_t)SP0256_SAMPLE_RATE << 16) / OUT_RATE);
    uint32_t written = 0u, tail = 0u;
    for (;;) {
        pp2_sp_service();
        const int done = pp2_sp_staged() == 0u && sp0256_speak_idle();
        if (done && ++tail > OUT_RATE/20u) break;
        if (!done) tail = 0u;
        const int16_t s = sp0256_rs_next(&rs, inc, pp2_sp_sample);
        put16(f, (uint16_t)s);
        if (++written > OUT_RATE*120u) break;
    }
    fseek(f, 0, SEEK_SET); wav_header(f, OUT_RATE, written); fclose(f);
    printf("  wrote %s, %.2f s\n", wav, (double)written/OUT_RATE);
    return 0;
}
