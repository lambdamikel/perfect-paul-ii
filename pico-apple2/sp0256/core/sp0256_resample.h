/* Rate conversion for the SP0256's 10 kHz stream, shared by the firmware
   mixer and the host test so both run the identical arithmetic.
   LambdaSpeak 4, GPL-3. */
#ifndef SP0256_RESAMPLE_H
#define SP0256_RESAMPLE_H
#include <stdint.h>

typedef struct { uint32_t phase; int16_t cur, nxt; } sp0256_rs_t;

/* `inc` is (source_rate << 16) / output_rate. Linear interpolation: cheap, and
   closer to what the chip's own output filter does than a zero-order hold. */
static inline int16_t sp0256_rs_next(sp0256_rs_t *r, uint32_t inc,
                                     int16_t (*src)(void))
{
    r->phase += inc;
    while (r->phase >= 0x10000u) {
        r->phase -= 0x10000u;
        r->cur = r->nxt;
        r->nxt = src();
    }
    return (int16_t)(r->cur +
        (((int32_t)(r->nxt - r->cur) * (int32_t)r->phase) >> 16));
}
#endif
