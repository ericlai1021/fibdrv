#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "bn.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* count leading zeros of src*/
static int bn_clz(const bn *src)
{
    int cnt = 0;
    for (int i = src->size - 1; i >= 0; i--) {
        if (src->number[i]) {
            // prevent undefined behavior when src = 0
            cnt += __builtin_clz(src->number[i]);
            return cnt;
        } else {
            cnt += 32;
        }
    }
    return cnt;
}

/* count the digits of bn*/
static int bn_digit(const bn *src)
{
    return src->size * 32 - bn_clz(src);
}

static int bn_is_zero(const bn *src)
{
    for (int i = 0; i < src->size; i++) {
        if (src->number[i] != 0U) {
            return 0;
        }
    }

    return 1;
}

char *bn_to_string(bn *src)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = ((sizeof(unsigned int) * src->size) << 3) / 3 + 2;

    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    int cur = len - 2;
    while (!bn_is_zero(src)) {
        unsigned long long remainder = 0U;
        // src->number[size - 1] is msb
        for (int i = src->size - 1; i >= 0; i--) {
            unsigned int quotient = 0U;
            // walk through every bit of bn
            for (unsigned int d = 1U << 31; d > 0; d >>= 1) {
                if (src->number[i] & d) {
                    remainder = (remainder << 1) | 1U;
                } else {
                    remainder = (remainder << 1) | 0U;
                }
                quotient = (quotient << 1) | (remainder / 10U);
                remainder %= 10;
            }
            src->number[i] = quotient;
        }
        s[cur] += remainder;
        cur--;
    }
    // skip leading zero
    while (*p == '0' && *(p + 1) != '\0') {
        p++;
    }

    memmove(s, p, strlen(p) + 1);
    return s;
}

bn *bn_alloc(size_t size)
{
    bn *new = (bn *) kmalloc(sizeof(bn), GFP_KERNEL);
    new->number =
        (unsigned int *) kmalloc(sizeof(unsigned int) * size, GFP_KERNEL);
    // QUESTION? physical or virtual contiguous memory allocation
    memset(new->number, 0, sizeof(unsigned int) * size);
    new->size = size;

    return new;
}

/*
 * free entire bn data structure
 * return 0 on success, -1 on error
 */
int bn_free(bn *src)
{
    if (src == NULL)
        return -1;
    kfree(src->number);
    kfree(src);
    return 0;
}

/*
 * resize bn
 * return 0 on success, -1 on error
 * data lose IS neglected when shinking the size
 */
static int bn_resize(bn *src, size_t size)
{
    if (!src)
        return -1;
    if (size == src->size)
        return 0;
    if (size == 0)  // prevent krealloc(0) = kfree, which will cause problem
        return bn_free(src);

    src->number =
        krealloc(src->number, sizeof(unsigned int) * size, GFP_KERNEL);

    if (!src->number) {  // realloc fails
        return -1;
    }
    if (size > src->size)
        memset(src->number + src->size, 0,
               sizeof(unsigned int) * (size - src->size));
    src->size = size;
    return 0;
}

/*
 * copy the value from src to dest
 * return 0 on success, -1 on error
 */
int bn_cpy(bn *dest, const bn *src)
{
    if (bn_resize(dest, src->size) < 0)
        return -1;
    memcpy(dest->number, src->number, src->size * sizeof(unsigned int));
    return 0;
}

/* c = a + b */
void bn_add(const bn *a, const bn *b, bn *c)
{
    bn_resize(c, MAX(a->size, b->size) + 1);

    unsigned long long carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;
        carry += (unsigned long long) tmp1 + tmp2;

        c->number[i] = carry & 0x00000000FFFFFFFF;
        carry >>= 32;
    }

    if (!c->number[c->size - 1] && c->size > 1)
        bn_resize(c, c->size - 1);
}

/* c = a - b */
void bn_sub(const bn *a, const bn *b, bn *c)
{
    bn_resize(c, MAX(a->size, b->size));

    long long borrow = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;

        borrow = (long long) tmp1 - tmp2 - borrow;
        if (borrow < 0) {
            c->number[i] = borrow + (1LL << 32);
            borrow = 1;
        } else {
            c->number[i] = borrow;
            borrow = 0;
        }
    }

    int clz_ints = bn_clz(c) / 32;
    if (clz_ints == c->size)
        --clz_ints;

    bn_resize(c, c->size - clz_ints);
}

/* dest = src << shift */
void bn_lshift(bn *dest, bn *src, size_t shift)
{
    size_t z = bn_clz(src);
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    if (shift > z)
        bn_resize(src, src->size + 1);

    bn_cpy(dest, src);
    /* bit shift */
    for (int i = src->size - 1; i > 0; i--)
        dest->number[i] =
            src->number[i] << shift | src->number[i - 1] >> (32 - shift);
    dest->number[0] <<= shift;
}

void bn_mult(const bn *a, const bn *b, bn *c)
{
    bn *tmp;

    // make it work properly when c == a or c == b
    if ((c == a) | (c == b)) {
        tmp = c;
        c = bn_alloc(a->size + b->size);
    } else {
        tmp = NULL;
        bn_resize(c, 1);
        c->number[0] = 0U;
        bn_resize(c, a->size + b->size);
    }

    bn *tmp1 = bn_alloc(1);
    bn_cpy(tmp1, a);
    if (b->number[0] & 1U)
        bn_add(c, tmp1, c);

    for (int i = 1; i < bn_digit(b); i++) {
        bn_lshift(tmp1, tmp1, 1);
        if (b->number[i / 32] & (1U << (i % 32)))
            bn_add(c, tmp1, c);
    }
    bn_free(tmp1);

    int clz_ints = bn_clz(c) / 32;
    if (clz_ints == c->size)
        --clz_ints;

    bn_resize(c, c->size - clz_ints);

    if (tmp) {
        bn_cpy(tmp, c);
        bn_free(c);
    }
}

void bn_fib(bn *dest, unsigned int n)
{
    bn_resize(dest, 1);
    if (n < 2) {  // Fib(0) = 0, Fib(1) = 1
        dest->number[0] = n;
        return;
    }

    bn *a = bn_alloc(1);
    bn *b = bn_alloc(1);
    dest->number[0] = 1;

    for (unsigned int i = 1; i < n; i++) {
        bn_cpy(b, dest);
        bn_add(dest, a, dest);
        bn_cpy(a, b);
    }
    bn_free(a);
    bn_free(b);
}
