#include <stdint.h>

/* hash: compute hash value of string */
unsigned int hash_string(const char *str) {
    unsigned int h;
    unsigned char *p;

    h = 0;
    for (p = (unsigned char *) str; *p != '\0'; p++) {
        h = 37 * h + *p;
    }
    return h; // or, h % ARRAY_SIZE;
}
