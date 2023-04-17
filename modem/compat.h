/*
  compat.h - defines for compatability with the arduino code being used
*/
#ifndef compat_h
#define compat_h

#define byte                uint8_t             
#define memmove_P           memmove
#define memcpy_P            memcpy 
#define strlen_P(a)         strlen((const char *)a)
#define strcpy_P(a,b)       strcpy(a,b)
#define pgm_read_byte(x)    (*x) 
#define min(a,b)            ((a)<(b)?(a):(b)) 
#define PGM_P               const char *
#define delay(x)            sleep_ms(x)

#endif // compat_h