/*
 * bignum data structure
 * number[size - 1] = msb, number[0] = lsb
 */
typedef struct _bn {
    unsigned int *number;
    unsigned int size;
} bn;

char *bn_to_string(bn *src);

/*
 * allocate a bn structure with the given size
 * the value is initialized to 0
 */
bn *bn_alloc(size_t size);

/*
 * free entire bn data structure
 * return 0 on success, -1 on error
 */
int bn_free(bn *src);

/*
 * copy the value from src to dest
 * return 0 on success, -1 on error
 */
int bn_cpy(bn *dest, const bn *src);

/* c = a + b */
void bn_add(const bn *a, const bn *b, bn *c);

/* c = a - b */
void bn_sub(const bn *a, const bn *b, bn *c);

/* dest = src << shift */
void bn_lshift(bn *dest, bn *src, size_t shift);

/* c = a * b */
void bn_mult(const bn *a, const bn *b, bn *c);

/* calc n-th Fibonacci number and save into dest */
void bn_fib_fast(bn *dest, unsigned int n);
void bn_fib(bn *dest, unsigned int n);
