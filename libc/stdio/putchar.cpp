#include <stdio.h>
#include <unistd.h>

#ifndef __is_libk
#include <unistd.h>

int putchar(int ic) {
    char c = (char)ic;
    write(1, &c, 1);
    return ic;
}
#endif
