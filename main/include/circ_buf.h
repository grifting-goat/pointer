#ifndef CIRC_BUF_H
#define CIRC_BUF_H

#include <stdint.h>


#define CIRC_BUF_DEF(x,y)                \
    int16_t x##_data_space[y];            \
    Circ_buf x = {                     \
        .buffer = x##_data_space,         \
        .head = 0,                        \
        .maxlen = y                       \
    }


typedef struct {
    int16_t* const buffer;
    int head;
    const int maxlen;
} Circ_buf;


static inline void circ_buf_push(Circ_buf *buf, int16_t data) {
    int next = buf->head + 1;

    if (next >= buf->maxlen) {
        next = 0;
    }

    buf->buffer[buf->head] = data;
    buf->head = next;
}

static inline void circ_buf_copy(const Circ_buf *src, Circ_buf *dest) {
    int len = src->maxlen > dest->maxlen ? dest->maxlen : src->maxlen;
    for (int i = 0; i < len; i++) {
        dest->buffer[i] = src->buffer[i];
    }

}

#endif // CIRC_BUF_H