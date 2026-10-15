#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int mode_t;

int mkdir(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // SYS_STAT_H
