#include <stdio.h>
int stderr_printf( const char * format, ... ) {
    va_list args;
    va_start(args, format);
    return vfprintf(stderr, format, args);
}
