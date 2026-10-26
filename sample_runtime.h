#ifndef SAMPLE_RUNTIME_H
#define SAMPLE_RUNTIME_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { SAMPLE_H2D=1, SAMPLE_D2H=2 };
int sample_device(int ordinal);
int sample_alloc(void **p, size_t n);
int sample_copy(void *d, const void *s, size_t n, int direction);
int sample_sync(void);
int sample_free(void *p);
const char *sample_error(void);
#ifdef __cplusplus
}
#endif
#endif
