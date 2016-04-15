/*
 * sio_win32.h
 *
 */
#include <process.h>
#include <time.h>
#include <windows.h>
#include <io.h>                 
#include <pdh.h>
#include <pdhmsg.h>

#define RAW_PREFIX      "\\\\.\\"

/*
 * Win32 typedefs
 */
typedef unsigned long pthread_t;
typedef unsigned long pthread_attr_t;

typedef HANDLE pthread_mutex_t;

//typedef unsigned long off_t;
typedef unsigned long ssize_t;

typedef struct {
#define SIGNAL          0
#define BROADCAST       1
#define MAX_EVENTS      2
        HANDLE events_[MAX_EVENTS];  /* Signal and broadcast event HANDLEs */
} pthread_cond_t;

typedef __int64 Offset_t;

void pthread_attr_init(pthread_attr_t   *attr);
void pthread_exit( void * exit_code );
void pthread_attr_setscope(pthread_attr_t *attr, unsigned type);
int pthread_create(pthread_t    *tid,
               pthread_attr_t   *attr,
               void             *(*func)(void *),
               void             *arg);
void    pthread_kill(pthread_t tid,int sig);
unsigned long pthread_self();
int     gettimeofday( struct timeval *tv , struct timezone *not_used );
int     getopt(int nargc, char **nargv, char *ostr);
int     win32_read(HANDLE han, char *buf, ULONG size);
int     win32_write(HANDLE han, char *buf, ULONG size);
Offset_t     win32_lseek(HANDLE han, Offset_t, ULONG size);
int     win32_open(char *name, int priv, int perm);
int    win32_pread(HANDLE han, char *buf, ULONG size, Offset_t offset);
int    win32_pwrite(HANDLE han, char *buf, ULONG size, Offset_t offset);
void   win32_set_file_option(int option, int value);

#define set_file_option(option, value) \
		win32_set_file_option(option, value)


#define PTHREAD_SCOPE_SYSTEM    1
#define PTHREAD_NORMAL_EXIT 0

static ULONG   highoff = 0;                                            \

#define open(file, mode, perm)	win32_open(file, mode, perm)

#define lseek(han, off, whence) \
        win32_lseek((HANDLE)han, off, whence)

#define read(han, buf, size)                            \
        win32_read((HANDLE)han, buf, size)

#define write(han, buf, size)                             \
        win32_write((HANDLE)han, buf, size)

#define close(han)         CloseHandle((HANDLE)han)
               
#define pread(han, buf, nbyte, offset) \
		win32_pread((HANDLE)han, (buf), (nbyte), (offset))

#define pwrite(han, buf, nbyte, offset) \
		win32_pwrite((HANDLE)han, (buf), (nbyte), (offset))

int usleep(UINT us);
UINT sleep(UINT secs);

typedef void *  caddr_t;
#define strtoll  strtol
#define SIGKILL SIGINT

#define pthread_attr_setstacksize(a,b) win32_noop();
void win32_noop();

