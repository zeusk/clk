/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include <sys/types.h>
#include <compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memchr (void const *, int, size_t) __PURE;
int   memcmp (void const *, const void *, size_t) __PURE;
void *memcpy (void *, void const *, size_t);
void *memmove(void *, void const *, size_t);
void *memset (void *, int, size_t);

//Concatenates two strings.
char       *strcat(char *, char const *);
//Locates the first occurrence of a specified character in a string.
char       *strchr(char const *, int) __PURE;
//Compares the value of two strings.
int         strcmp(char const *, char const *) __PURE;
//Copies one string into another.
char       *strcpy(char *, char const *);
//Calculates the length of a string.
size_t      strlen(char const *) __PURE;
//Adds a specified length of one string to another string.
char       *strncat(char *, char const *, size_t);
//Compares two strings up to a specified length. 
int         strncmp(char const *, char const *, size_t) __PURE;
//Copies a specified length of one string into another.
char       *strncpy(char *, char const *, size_t);
//Locates specified characters in a string.
char       *strpbrk(char const *, char const *) __PURE;
//Locates the last occurrence of a character within a string. 
char       *strrchr(char const *, int) __PURE;
//Locates the first character in a string that is not part of specified set of characters. 
size_t      strspn(char const *, char const *) __PURE;
//Finds the length of the first substring in a string of characters not in a second string.
size_t      strcspn(const char *s, const char *) __PURE;
//Locates the first occurrence of a string in another string.
char       *strstr(char const *, char const *) __PURE;
//Locates a specified token in a string.
char       *strtok(char *, char const *);
//Compares two strings based on the collating elements for the current locale.
int         strcoll(const char *s1, const char *s2) __PURE;
//Transforms strings according to locale.
size_t      strxfrm(char *dest, const char *src, size_t n) __PURE;
//Reserves storage space for the copy of a string.
char       *strdup(const char *str) __MALLOC;
//Reverses the order of characters in a string.
char*       strrev(char* str);
//Search & replace a substring by another one.
char*		replace(const char *str, const char *oldstr, const char *newstr, int *count);
//Replace a character at a specified position
char* 		sreplace(char *buf, int c, int pos);

char        tolower(char c);
char        toupper(char c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* non standard */
void  *bcopy(void const *, void *, size_t);
void   bzero(void *, size_t);
size_t strlcat(char *, char const *, size_t);
size_t strlcpy(char *, char const *, size_t);
int    strncasecmp(char const *, char const *, size_t)  __PURE;
int    strnicmp(char const *, char const *, size_t) __PURE;
size_t strnlen(char const *s, size_t count) __PURE;
unsigned long strtoul(const char *str, char **endptr, int base);
long strtol(const char *str, char **endptr, int base);

#ifdef __cplusplus
}
#endif

#endif
