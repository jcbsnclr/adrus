#include <assert.h>
#include <jbase.h>
#include <stddef.h>

void *jb_buf_grow(const void *buf, size_t new_len, size_t elem_size) {
    assert(jb_buf_cap(buf) <= (SIZE_MAX - 1) / 2);

    size_t new_cap = JB_MAX(16, JB_MAX(2 * jb_buf_cap(buf), new_len));

    assert(new_len <= new_cap);
    assert(new_cap <= (SIZE_MAX - offsetof(jb_buf_hdr_t, buf)) / elem_size);

    size_t new_size = offsetof(jb_buf_hdr_t, buf) + new_cap * elem_size;

    jb_buf_hdr_t *new_hdr;
    if (buf) {
        new_hdr = realloc(jb_buf_hdr(buf), new_size);
    } else {
        new_hdr = malloc(new_size);
        new_hdr->len = 0;
    }

    new_hdr->cap = new_cap;
    return new_hdr->buf;
}
