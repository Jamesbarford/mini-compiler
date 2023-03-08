#ifndef __CSTR_H
#define __CSTR_H

typedef unsigned char cstr;

#define CSTR_ERR -1
#define CSTR_OK 1

void cstrRelease(void *str);
unsigned int cstrlen(cstr *str);
cstr *cstrnew(void);
cstr *cstrCatLen(cstr *str, const void *buf, unsigned int len);
cstr *cstrCat(cstr *str, char *s);
cstr *cstrCatPrintf(cstr *s, const char *fmt, ...);

#endif
