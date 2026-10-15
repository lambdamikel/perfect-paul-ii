/* LambdaSpeak 4, GPL-3. Bridges the vendor's C++ namespace to plain C. */
#include "sp0256_rom.h"
#include "sp0256_al2.h"
#include <string.h>
#include <ctype.h>

extern "C" const uint8_t *sp0256_al2_rom(void)
{
    return sp0256_al2::mask;
}

extern "C" const char *sp0256_al2_label(int allophone)
{
    if (allophone < 0 || allophone >= SP0256_ALLOPHONE_COUNT) return 0;
    return sp0256_al2::labels[allophone];
}

extern "C" int sp0256_al2_lookup(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < SP0256_ALLOPHONE_COUNT; i++) {
        const char *l = sp0256_al2::labels[i];
        int j = 0;
        while (l[j] && name[j] && toupper((unsigned char)name[j]) == l[j]) j++;
        if (!l[j] && !name[j]) return i;
    }
    return -1;
}
