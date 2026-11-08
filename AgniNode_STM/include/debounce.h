#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

/* Asymmetric counter debouncer: a new level must persist for on_n / off_n
 * consecutive samples before the stable output changes. */
typedef struct {
    bool state;
    uint16_t cnt;
    uint16_t on_n;
    uint16_t off_n;
} debounce_t;

static inline void debounce_init(debounce_t *d, uint16_t on_n, uint16_t off_n) {
    d->state = false;
    d->cnt = 0;
    d->on_n = on_n;
    d->off_n = off_n;
}

static inline bool debounce_update(debounce_t *d, bool raw) {
    if (raw == d->state) {
        d->cnt = 0;
    } else if (++d->cnt >= (raw ? d->on_n : d->off_n)) {
        d->state = raw;
        d->cnt = 0;
    }
    return d->state;
}

#endif
