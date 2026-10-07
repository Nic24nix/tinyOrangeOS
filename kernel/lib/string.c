#include <stdint.h>
#include "../kernel.h"

/* =========================================================
   STRING FUNCTIONS
   ========================================================= */

int kstrlen(const char *str)
{
    int len = 0;

    if (!str)
        return 0;

    while (str[len] != '\0')
        len++;

    return len;
}

int kstrcmp(const char *a, const char *b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}

int kstrncmp(const char *a, const char *b, int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

        if (a[i] == '\0')
            return 0;
    }

    return 0;
}

void kstrcpy(char *dst, const char *src)
{
    int i = 0;

    while (src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

void kstrcat(char *dst, const char *src)
{
    int pos = kstrlen(dst);
    int i = 0;

    while (src[i] != '\0')
    {
        dst[pos++] = src[i++];
    }

    dst[pos] = '\0';
}

/* =========================================================
   BOUNDED STRINGS
   ========================================================= */

int kcopy_bounded(char *dst, const char *src, int max)
{
    int len;

    if (!dst || !src || max <= 0)
        return 0;

    len = kstrlen(src);

    if (len >= max)
    {
        dst[0] = '\0';
        return 0;
    }

    kstrcpy(dst, src);
    return 1;
}

