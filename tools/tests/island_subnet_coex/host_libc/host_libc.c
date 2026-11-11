#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

extern int main(void);

#define HEAP_SIZE (4U * 1024U * 1024U)
#define BLOCK_MAGIC 0x484C424BU

typedef struct block_hdr {
    size_t size;
    uint32_t magic;
    uint8_t used;
    uint8_t pad[3];
    struct block_hdr *next;
} block_hdr_t;

static uint8_t s_heap[HEAP_SIZE] __attribute__((aligned(16)));
static block_hdr_t *s_free_list = NULL;

static char s_log_buf[65536];
static size_t s_log_pos = 0;
static volatile int s_aborted = 0;
static void (*s_py_abort_cb)(const char *msg) = NULL;

static void heap_init_if_needed(void)
{
    if (!s_free_list) {
        s_free_list = (block_hdr_t *)s_heap;
        s_free_list->size = HEAP_SIZE - sizeof(block_hdr_t);
        s_free_list->magic = BLOCK_MAGIC;
        s_free_list->used = 0;
        s_free_list->next = NULL;
    }
}

void *malloc(size_t size)
{
    heap_init_if_needed();
    if (size == 0) size = 1;
    size = (size + 15U) & ~(size_t)15U;

    block_hdr_t *cur = s_free_list;
    while (cur) {
        if (!cur->used && cur->size >= size) {
            if (cur->size >= size + sizeof(block_hdr_t) + 16U) {
                block_hdr_t *split = (block_hdr_t *)((uint8_t *)(cur + 1) + size);
                split->size = cur->size - size - sizeof(block_hdr_t);
                split->magic = BLOCK_MAGIC;
                split->used = 0;
                split->next = cur->next;
                cur->size = size;
                cur->next = split;
            }
            cur->used = 1;
            return (void *)(cur + 1);
        }
        cur = cur->next;
    }
    return NULL;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    block_hdr_t *hdr = ((block_hdr_t *)ptr) - 1;
    if (hdr->size >= size) return ptr;
    void *n = malloc(size);
    if (n) {
        memcpy(n, ptr, hdr->size);
        free(ptr);
    }
    return n;
}

void free(void *ptr)
{
    if (!ptr) return;
    block_hdr_t *hdr = ((block_hdr_t *)ptr) - 1;
    if (hdr->magic != BLOCK_MAGIC) return;
    hdr->used = 0;
    block_hdr_t *cur = s_free_list;
    while (cur && cur->next) {
        if (!cur->used && !cur->next->used) {
            cur->size += sizeof(block_hdr_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

static void put_ch(char *buf, size_t size, size_t *pos, char ch)
{
    if (buf && *pos + 1 < size) buf[*pos] = ch;
    (*pos)++;
}

static void put_str(char *buf, size_t size, size_t *pos, const char *s)
{
    if (!s) s = "(null)";
    while (*s) put_ch(buf, size, pos, *s++);
}

static void put_uint(char *buf, size_t size, size_t *pos, uint64_t val, unsigned base, int upper, int min_w, char pad)
{
    char tmp[32];
    int len = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val > 0) {
            tmp[len++] = digits[val % base];
            val /= base;
        }
    }
    for (int i = len; i < min_w; i++) put_ch(buf, size, pos, pad);
    while (len > 0) put_ch(buf, size, pos, tmp[--len]);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t pos = 0;
    while (fmt && *fmt) {
        if (*fmt != '%') {
            put_ch(buf, size, &pos, *fmt++);
            continue;
        }
        fmt++;
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        int is_long = 0;
        int is_ll = 0;
        if (*fmt == 'z' || *fmt == 'l') {
            is_long = 1;
            if (*fmt == 'l' && fmt[1] == 'l') { is_ll = 1; fmt++; }
            fmt++;
        }
        switch (*fmt) {
        case 's': put_str(buf, size, &pos, va_arg(ap, const char *)); break;
        case 'c': put_ch(buf, size, &pos, (char)va_arg(ap, int)); break;
        case 'd': {
            int64_t v = is_ll ? va_arg(ap, long long) : (is_long ? va_arg(ap, long) : va_arg(ap, int));
            if (v < 0) { put_ch(buf, size, &pos, '-'); v = -v; }
            put_uint(buf, size, &pos, (uint64_t)v, 10, 0, width, pad);
            break;
        }
        case 'u': {
            uint64_t v = is_ll ? va_arg(ap, unsigned long long) : (is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int));
            put_uint(buf, size, &pos, v, 10, 0, width, pad);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = is_ll ? va_arg(ap, unsigned long long) : (is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int));
            put_uint(buf, size, &pos, v, 16, (*fmt == 'X'), width, pad);
            break;
        }
        case 'p':
            put_str(buf, size, &pos, "0x");
            put_uint(buf, size, &pos, (uint64_t)(uintptr_t)va_arg(ap, void *), 16, 0, 0, '0');
            break;
        case '%':
            put_ch(buf, size, &pos, '%');
            break;
        default:
            put_ch(buf, size, &pos, *fmt);
            break;
        }
        if (*fmt) fmt++;
    }
    if (buf && size > 0) {
        size_t end = (pos < size) ? pos : (size - 1);
        buf[end] = '\0';
    }
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    size_t i = 0;
    while (tmp[i] && s_log_pos + 1 < sizeof(s_log_buf)) {
        s_log_buf[s_log_pos++] = tmp[i++];
    }
    s_log_buf[s_log_pos] = '\0';
    return r;
}

void host_assert_fail(const char *expr, const char *file, int line)
{
    s_aborted = 1;
    printf("ASSERTION FAILED: %s at %s:%d\n", expr, file, line);
    if (s_py_abort_cb) s_py_abort_cb(s_log_buf);
}

void abort(void)
{
    s_aborted = 1;
    if (s_py_abort_cb) s_py_abort_cb("abort() called");
}

void __chkstk(void) {}

__declspec(dllexport) int host_dll_run(void (*abort_cb)(const char *msg), const char **out_log)
{
    s_py_abort_cb = abort_cb;
    s_aborted = 0;
    s_log_pos = 0;
    s_log_buf[0] = '\0';
    s_free_list = NULL;
    heap_init_if_needed();
    int rc = main();
    if (out_log) *out_log = s_log_buf;
    if (s_aborted && rc == 0) rc = 1;
    return rc;
}

