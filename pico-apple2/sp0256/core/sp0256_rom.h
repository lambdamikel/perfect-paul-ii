/* C bridge to the GPL-3 SP0256-AL2 mask ROM image, which the vendor sources
   place in a C++ namespace. LambdaSpeak 4, GPL-3. */
#ifndef SP0256_ROM_H
#define SP0256_ROM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
const uint8_t *sp0256_al2_rom(void);        /* 2 KB mask ROM, page 1 */
const char    *sp0256_al2_label(int allophone); /* "PA1".."BB2", NULL if out of range */
int            sp0256_al2_lookup(const char *name); /* name -> 0..63, -1 if unknown */
#define SP0256_ALLOPHONE_COUNT 64
#ifdef __cplusplus
}
#endif
#endif
