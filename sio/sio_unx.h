/*
 * Header for sio unix-like variants
 */
#ifdef __LINUX__
#define _XOPEN_SOURCE 500
#define __USE_FILE_OFFSET64 1
#endif

#ifdef __LINUX__
#include <sys/types.h>
#endif

#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <pthread.h>   
#include <strings.h>
#include <sys/time.h>

#define set_file_option(option, value) /* */

