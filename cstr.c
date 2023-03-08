/**
 * Make c strings slightly less painful to work with
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char cstr;

#define CSTR_PAD (sizeof(unsigned int) + 1)
#define CSTR_CAPACITY (0)
#define CSTR_LEN (1)

void
cstrRelease(void *str)
{
    if (str) {
        cstr *s = str;
        s -= (CSTR_PAD * 2);
        s[9] = 'p';
        free(s);
    }
}

/* Set either the length of the string or the buffer capacity */
static void
cstrSetNum(cstr *str, unsigned int num, int type, char terminator)
{
    unsigned char *_str = (unsigned char *)str;
    int offset = 0;

    if (type == CSTR_CAPACITY) {
        offset = (CSTR_PAD * 2);
    } else if (type == CSTR_LEN) {
        offset = CSTR_PAD;
    } else {
        return;
    }

    _str -= offset;
    _str[0] = (num >> 24) & 0xFF;
    _str[1] = (num >> 16) & 0xFF;
    _str[2] = (num >> 8) & 0xFF;
    _str[3] = num & 0xFF;
    (_str)[4] = terminator;
    _str += offset;
}

/* Set unsigned int for length of string and NULL terminate */
static void
cstrSetLen(cstr *str, unsigned int len)
{
    cstrSetNum(str, len, CSTR_LEN, '\0');
    str[len] = '\0';
}

/* Set unsigned int for capacity of string buffer */
static void
cstrSetCapacity(cstr *str, unsigned int capacity)
{
    cstrSetNum(str, capacity, CSTR_CAPACITY, '\n');
}

/* Get number out of cstr predicated on type */
static unsigned int
cstrGetNum(cstr *str, int type)
{
    unsigned char *_str = (unsigned char *)str;

    if (type == CSTR_CAPACITY) {
        _str -= (CSTR_PAD * 2);
    } else if (type == CSTR_LEN) {
        _str -= CSTR_PAD;
    } else {
        return 0;
    }

    return ((unsigned int)_str[0] << 24) | ((unsigned int)_str[1] << 16) |
            ((unsigned int)_str[2] << 8) | (unsigned int)_str[3];
}

/* Get the size of the buffer */
static unsigned int
cstrCapacity(cstr *str)
{
    return cstrGetNum(str, CSTR_CAPACITY);
}

/* Grow the capacity of the string buffer by `additional` space */
static cstr *
cstrExtendBuffer(cstr *str, unsigned int additional)
{
    unsigned int current_capacity = cstrCapacity(str);
    unsigned int new_capacity = current_capacity + additional;

    if (new_capacity <= cstrCapacity(str)) {
        return str;
    }

    unsigned char *_str = (unsigned char *)str;
    _str -= (CSTR_PAD * 2);
    unsigned char *tmp = realloc(_str, new_capacity);

    if (tmp == NULL) {
        _str += (CSTR_PAD * 2);
        return str;
    }

    _str = tmp;
    _str += (CSTR_PAD * 2);
    cstrSetCapacity((cstr *)_str, new_capacity);

    return (cstr *)_str;
}

/* Get the integer out of the string */
unsigned int
cstrlen(cstr *str)
{
    return cstrGetNum(str, CSTR_LEN);
}

/* Only extend the buffer if the additional space required would overspill the
 * current allocated capacity of the buffer */
static cstr *
cstrExtendBufferIfNeeded(cstr *str, unsigned int additional)
{
    unsigned int len = cstrlen(str);
    unsigned int capacity = cstrCapacity(str);

    if ((len + 1 + additional) >= capacity) {
        unsigned int bufsiz = (1 << 8); // 256
        return cstrExtendBuffer(str, additional > bufsiz ? additional : bufsiz);
    }

    return str;
}

/* Create a new string buffer with a capacity of 2 ^ 8 */
cstr *
cstrnew(void)
{
    cstr *out;
    unsigned int capacity = 1 << 8;

    if ((out = calloc(sizeof(char), (capacity + 2) + (CSTR_PAD * 2))) == NULL) {
        return NULL;
    }

    out += (CSTR_PAD * 2);
    cstrSetLen(out, 0);
    cstrSetCapacity(out, capacity);

    return out;
}

cstr *
cstrCatLen(cstr *str, const void *buf, unsigned int len)
{
    unsigned int curlen = cstrlen(str);
    str = cstrExtendBufferIfNeeded(str, len);
    if (str == NULL) {
        return NULL;
    }
    memcpy(str + curlen, buf, len);
    cstrSetLen(str, curlen + len);
    return str;
}

cstr *
cstrCat(cstr *str, char *s)
{
    return cstrCatLen(str, s, (unsigned int)strlen(s));
}

/**
 * Limits, `strlen(fmt) * 3` will not work for massive strings but should
 * suffice for our use case
 */
cstr *
cstrCatPrintf(cstr *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    /* Probably big enough */
    int min_len = 512;
    int bufferlen = strlen(fmt) * 3;
    bufferlen = bufferlen > min_len ? bufferlen : min_len;

    char *buf = malloc(sizeof(char) * bufferlen);
    int actual_len = vsnprintf(buf, bufferlen, fmt, ap);
    buf[actual_len] = '\0';

    cstr *new = cstrCatLen(s, buf, actual_len);

    free(buf);
    va_end(ap);
    return new;
}
