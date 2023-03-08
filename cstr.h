#ifndef __CSTR_H
#define __CSTR_H

typedef unsigned char cstr;

void cstrRelease(void *str);
unsigned int cstrlen(cstr *str);
cstr *cstrnew(void);
cstr *cstrCatLen(cstr *str, const void *buf, unsigned int len);
cstr *cstrCat(cstr *str, char *s);
cstr *cstrCatPrintf(cstr *s, const char *fmt, ...);

#endif
