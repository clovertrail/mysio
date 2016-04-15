/* 
 * sio_ntap:  Network Appliance Threaded IO load generator
 * 
 * This file is provided "as is" and is not warrented or supported
 * by Network Appliance.
 * 
 * Compilation definitions:
 * _SOLARIS  - Build for Solaris
 * WIN32     - Build for Win32 native.
 * __LINUX__ - Build for Linux
 * _HPUX     - Build for HPUX
 * _AIX      - Build for AIX
 */

/*
 * version tracking
 * 45: customer pointed out some errors (fflush stdout and linux/sleep)
 * 44: fixed ifdef bug.  hpux now works.  nopread works correctly.
 * 43: start tracking.  Change hpux to no pread()
 */
#define VERSION "Version: 3.00"

#define _REENTRANT 1		//errno 
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES

#ifdef WIN32
#include "sio_win32.h"
#else
#include "sio_unx.h"
#endif

#include <sys/types.h>		// need for type information
#include <sys/stat.h>
#include <sys/timeb.h>		// need for ftime
#include <fcntl.h>
#include <stdio.h>		// need for printf stuff
#include <stdlib.h>		// need for rand, atoi, etc.
#include <limits.h>		// need for INT_MAX
#include <string.h>
#include <errno.h>
#include <signal.h>


/*Constant declarations*/
#define MAX_INT (1<<30) - 1


#ifdef _SOLARIS
typedef off64_t Offset_t;
#define USE_64BIT_OFFSETS 1
#define USE_RAND48 1
#define USE_FLOCK 1
#define USE_PREAD 1
#endif

#ifdef WIN32
#define USE_64BIT_OFFSETS 1
#define USE_FLOCK 0
#define USE_RAND48 0
#define USE_PREAD 1
#endif

#ifdef __LINUX__
#define USE_64BIT_OFFSETS 1
#define USE_RAND48 1
#define USE_FLOCK 1
typedef off_t Offset_t;
#define USE_PREAD 1
#endif

#ifdef __FreeBSD__
#define USE_64BIT_OFFSETS 0
#define USE_RAND48 1
#define USE_FLOCK 1
typedef off_t Offset_t;
#define USE_PREAD 1
#endif

#ifdef _HPUX
#include <inttypes.h>
#define USE_64BIT_OFFSETS 1
#define USE_RAND48 1
#define USE_FLOCK 1
#define USE_PREAD 0
typedef off_t Offset_t;
#endif

#ifdef _AIX
#include <inttypes.h>
#define USE_64BIT_OFFSETS 1
#define USE_RAND48 1
#define USE_FLOCK 1
typedef off_t Offset_t;
#define USE_PREAD 1
#endif


#if USE_RAND48==1
	#define RAND()		lrand48()
	#define SRAND(x)	srand48(x)
#else
	#define RAND()		rand()
	#define SRAND(x)	srand(x)
#endif

#define PREAD_POS 1
#define PRAND_POS 2
#define BLKSZ_POS 3
#define END_POS 4
#define SECONDS_POS 5
#define NUMTHREAD_POS 6
#define DEVICES_POS 7

#define MAX_DEVICES 600
#define MAX_THREAD 256

#ifdef __LINUX__
	/*
	 * MDW: Add in alignment for raw io.
	 */
	#define LINUX_RAWIO_ALIGNMENT 512
	#define MAX_BLKSIZE 1024*256+LINUX_RAWIO_ALIGNMENT
#else
	#define MAX_BLKSIZE 1024*256
#endif    

#define MAX_STR 2048
#define EVENT_NAME "sio"

#define BlockNum_t   Offset_t

/* Prototypes*/
void *work_thread (void *);	
void print_stats ();
void print_usage();
int random_block ();
int increment_with_wrap ();
void die ();
void open_fds (int[], int);
Offset_t stringBytes (char *s);

/* globals */
pthread_t tid[256];
char devs[MAX_DEVICES][MAX_STR];
int io_completes[MAX_THREAD];
struct timeb begin_time, end_time;
int total_ios, warmup_ios, p_read, prand, num_devs, run_time, num_threads;
int blk_sz;
char **device_name;
BlockNum_t global_curblk[MAX_DEVICES];

Offset_t begin_blk, end_blk;
long seed;
volatile int stop_flag;

/*cmdline configurable stuff*/
int debug = 0;
int flockon = 1;
FILE *out_fp;
int directio_on = 0;

int main (int argc, char **argv)
{
	int i;
	pthread_attr_t attr;

	/* setup the out_fd default */
	out_fp = stdout;
	fprintf(out_fp, "%s\n", VERSION); fflush(stdout);

	/*first, rip off any -flag stuff*/
	while (argv[argc - 1][0] == '-') {
		if (strcmp ("-noflock", argv[argc - 1]) == 0)
		    flockon = 0;
		if (strcmp ("-debug", argv[argc - 1]) == 0)
		    debug = 1;
		if (strcmp ("-output", argv[argc - 1]) == 0) {
			out_fp=fopen(&(argv[argc][1]),"w");
			if (out_fp == NULL) {
				perror ("Failure to open output file");
				fprintf (stderr, "File: %s\n", &(argv[argc][1]));
				die ("File open error");
			} 
		}
		if (strcmp ("-direct", argv[argc - 1]) == 0) {
#if (defined(_SOLARIS) || defined(_AIX) || defined(__LINUX__))
		    directio_on = 1;
#else
		    die ("Option -direct not supported");
#endif
		}
		argc--;
	}
	
	/*parse-inputs*/
	device_name = argv + DEVICES_POS;
	num_devs = argc - DEVICES_POS;
	for (i = 0; i < num_devs; i++) {
		strcpy (devs[i], *(device_name + i));
	}

	if ((num_devs < 1) || (num_devs > MAX_DEVICES)) print_usage();

	p_read = atoi (argv[PREAD_POS]);
	prand = atoi (argv[PRAND_POS]);

	num_threads = atoi (argv[NUMTHREAD_POS]);
	blk_sz = (size_t) stringBytes (argv[BLKSZ_POS]);

	if (blk_sz > MAX_BLKSIZE) die ("Error: Max block size exceeded");

	begin_blk = 0;
	end_blk = stringBytes (argv[END_POS]) / blk_sz;
	run_time = atoi (argv[SECONDS_POS]);

	if (run_time == 0) run_time = MAX_INT;

	if (num_threads > MAX_THREAD) die ("Error:  num_threads > MAX_THREAD");

	/*start_threads*/
	stop_flag = 0;
	for (i = 0; i < num_threads; i++)
	{
		pthread_attr_init (&attr);
		pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize (&attr, (size_t) (1024*1024));

		if (pthread_create (&(tid[i]), &attr, work_thread, (void *) i) != 0) {
		    die("Thread create failed!"); fflush(stderr);
		}

		usleep (2000);		/*sleep 1ms*/
	}

	/*wait until all threads have recorded at least 1 i/o.
	then, count all that have occured, and use that as a base
	line to remove from the total count.
	*/
	for (i = 0; i < num_threads; i++) {
		while (!io_completes[i])
			sleep (1);
	}

	warmup_ios = 0;
	for (i = 0; i < num_threads; i++) warmup_ios += io_completes[i];

	// record begin time, sleep, notify threads, record end time
	ftime (&begin_time);
	sleep (run_time);
	stop_flag = 1;
	ftime (&end_time);
	
	// Count up the I/O's
	total_ios = 0;
	for (i = 0; i < num_threads; i++) total_ios += io_completes[i];
	total_ios -= warmup_ios;

	print_stats ();
	#ifndef __LINUX__
		fflush(stdout);
		sleep(2);
	#endif

	// give threads time to close files
	// note that linux doesn't like having threads exit while
	// it's asleep, so this is not called
	#ifndef __LINUX__
		sleep (5);
	#endif
						  
	fprintf(out_fp, "Terminating threads ..."); fflush(out_fp);
	fclose(out_fp);
	// kill all the threads
	for (i = 0; i < num_threads; i++) pthread_kill (tid[i], SIGKILL);
	sleep(1);

}


void
print_stats ()
{
	double ios_per_sec, tot_time,throughput;
	int secs = end_time.time - begin_time.time;
	int msecs = end_time.millitm - begin_time.millitm;
	int i;

	// correct if msecs happened to be negative
	if (msecs < 0) {
		secs--;
		msecs += 1000;
	}

	tot_time = secs + msecs / 1000.0;	// number of seconds during run
	ios_per_sec = (double) total_ios / tot_time;

	throughput = ios_per_sec * (double) (blk_sz / 1024.0);

	fprintf(out_fp, "\nSIO_NTAP:\n");
	fprintf(out_fp, "Inputs\n");
	fprintf(out_fp, "Read %%:\t\t%d\n",p_read);
	fprintf(out_fp, "Random %%:\t%d\n",prand);
	fprintf(out_fp, "Block Size:\t%d\n",blk_sz);
	// windows big %I64d, unix big is %lld, non-big is %ld
	if (USE_64BIT_OFFSETS) {
		#ifdef WIN32
			fprintf(out_fp, "File Size:\t%I64d\n",end_blk*blk_sz);
		#else
			fprintf(out_fp, "File Size:\t%lld\n",end_blk*blk_sz);
		#endif
	} else {
		fprintf(out_fp, "File Size:\t%ld\n",end_blk*blk_sz);
	}
	fprintf(out_fp, "Secs:\t\t%d\n",run_time);
	fprintf(out_fp, "Threads:\t%d\n",num_threads);
	fprintf(out_fp, "File(s):\t");
	for (i=0;i<num_devs;i++) { fprintf(out_fp, "%s ", devs[i]); }
	fprintf(out_fp, "\n");
	fprintf(out_fp, "Outputs\n");
	fprintf(out_fp, "IOPS:\t\t%.0f\n", ios_per_sec);
	fprintf(out_fp, "KB/s:\t\t%.0f\n", throughput);
	fprintf(out_fp, "IOs:\t\t%d\n", total_ios);

}

void
open_fds (int fds[], int threadnum)
{
	int i, fd, flags;

	#if USE_FLOCK==1
		struct flock flock;
	#endif

	for (i = 0; i < num_devs; i++) {
		flags = 0;
		if(p_read == 100) {
		    flags |= O_RDONLY;
		} else {
		    flags |= O_RDWR;
		}
#if (defined(__LINUX__) || defined(_AIX))
		if (directio_on) {
		    flags |= O_DIRECT;
		}
#endif

		fd = open (devs[i], flags, 0);

		if (fd == -1) {
			perror ("open");
			fprintf (stderr, "File: %s\n", devs[i]);
			die ("File open error");
		} 
		#if USE_FLOCK==1
			if (flockon) {
				int error;
				if (threadnum == 0) {
					flock.l_type = F_RDLCK;
					flock.l_whence = SEEK_SET;
					flock.l_start = 1;
					flock.l_len = 1;
					error = fcntl (fd, F_SETLK, &flock);
					if (error == -1) die ("Error locking file\n");
				}
			}
		#endif
#ifdef _SOLARIS
		if (directio_on) {
		    directio(fd,DIRECTIO_ON);
		} else {
		    directio(fd,DIRECTIO_OFF);
		}
#endif
		fds[i] = fd;
		// setup seq stuff
		global_curblk[i] = begin_blk;
	}
}

/*
 * do_io - main function for doing synch I/O within a thread
 */
void
do_io (int threadnum, int fd[], char *buf)
{
	int probread, probrand;
	int target_device;
	BlockNum_t curblk;
	int byte_count;
	curblk = begin_blk;		// give a place to start out

	while (!stop_flag) {

		probread = (int) RAND () % 100;
		probrand = (int) RAND () % 100;

		target_device = (int) RAND () % num_devs;

		if (probrand < prand)
		    random_block (&curblk);
		else
		    increment_with_wrap (target_device, &curblk);

		if (probread < p_read) {
			#if USE_PREAD==1
				byte_count = pread (fd[target_device], buf, blk_sz,
					curblk * blk_sz);
			#else
				lseek(fd[target_device], curblk*blk_sz, SEEK_SET);
				byte_count = read (fd[target_device], buf, blk_sz);
			#endif
		} else {
			#if USE_PREAD==1
				byte_count = pwrite (fd[target_device], buf, blk_sz,
					curblk * blk_sz);
			#else
				lseek(fd[target_device], curblk * blk_sz, SEEK_SET);
				byte_count = write (fd[target_device], buf, blk_sz);
			#endif
		}
		if (byte_count != blk_sz) {

			perror("byte count mismatch");
			die("Error accessing file");
		}

		io_completes[threadnum]++;
	}
}

// main worker thread code
void *
work_thread (void *arg)
{

	int threadnum = (int) arg;
	int fd[MAX_DEVICES];
	int i;
	caddr_t buf, alloc_memory;

	#ifdef __LINUX__
	// MDW: Add in alignment for raw io.
	alloc_memory = malloc (blk_sz + LINUX_RAWIO_ALIGNMENT);
	buf = (caddr_t) (((unsigned long)alloc_memory + LINUX_RAWIO_ALIGNMENT) & ~(LINUX_RAWIO_ALIGNMENT-1));
	#else
	alloc_memory = malloc (blk_sz);
	buf = alloc_memory;
	#endif    

	if(buf==NULL) die("Unable to get buffer memory");

	SRAND ((threadnum + 1) * (int) getpid ());

	open_fds (fd, threadnum);

	do_io (threadnum, fd, buf);

	for (i = 0; i < num_devs; i++)
	    close (fd[i]);

	free (alloc_memory);

	pthread_exit (NULL);

	return(NULL); // windows wants this
}

// utility functions (that should inline) to make a little cleaner code
int
random_block (BlockNum_t * target)
{
#if USE_64BIT_OFFSETS==1
#ifdef WIN32
	unsigned __int64 l, h;
#else
	unsigned long long l, h;
#endif
	l = RAND ();
	h = RAND ();
	(*target) = (((h << 32) + l) % (end_blk - begin_blk - 1));
#else
#if USE_RAND48==1
	(*target) = RAND () % (end_blk - begin_blk);
#else
	(*target) = (((RAND () << 15) + RAND ()) % (end_blk - begin_blk - 1));
#endif
#endif
	(*target) += begin_blk;	// offset properly
	return 1;			// always need to seek
}

int
increment_with_wrap (int device, Offset_t * target)
{
	int result=0;
	*target = global_curblk[device];
	if(*target >= end_blk) {
	    *target=begin_blk;
	    result=1;
	}
	global_curblk[device]++;
	if ((global_curblk[device]) >= end_blk) {
	    global_curblk[device] = begin_blk;
	    result=1;
	}
	return result;
}

Offset_t 
stringBytes (char *s)
{
	char *c;
	Offset_t size;
	#ifdef _HPUX
	size = __strtoll (s,NULL, 0);
	#else
	size = strtoll (s, NULL, 0);
	#endif
	for (c = s; isdigit (*c); c++)
	    ;
	switch (*c) {
		case '\0': break;
		case 't': case 'T':
		    size *= 1024L;
		case 'g': case 'G':
		    size *= 1024L;
		case 'm': case 'M':
		    size *= 1024L;
		case 'k': case 'K':
		    size *= 1024L;
		    break;
		default:
		    fprintf (stderr, "Unknown size type %c\n", *c);
		    exit (2);
	}

	if (debug) {
		if (USE_64BIT_OFFSETS) {
			#ifdef WIN32
				fprintf(stderr,"return: %I64d\n",size);
			#else
				fprintf(stderr,"return: %lld\n",size);
			#endif
		} else {
			fprintf(stderr,"return: %ld\n",size);
		}
	}

	return size;
}
void
die (char *string)
{
	fprintf (stderr, "\nsio_ntap: %s\n", string);
	exit (1);
}

void print_usage()
{
	printf("Usage: \n");
	printf("  sio <read%> <rand%> <blksz> <file_size> <seconds> <numthreads> <dev> [devs...]\n");
	printf("\n");
	printf("<read>:       percentage of accesses that are reads.  Range [0,100].\n");
	printf("              BEWARE, writing to a file is unchecked and will trash files\n");
	printf("<rand>:       percentage of acceses that are random.  Range [0,100].\n");
	printf("              Sequential accesses = 0% random\n");
	printf("<blksz>:      size of I/O's. Example: 2k, 4k, 1m\n");
	printf("<file_size>:  total bytes accessed in each file (e.g. 100m, 2g, 1000k)\n");
	printf("<seconds>:    Runtime for test.  Counting starts AFTER all threads have started\n");
	printf("<numthreads>: Concurrent I/O generators.  Uses real individual threads.\n");
	printf("<dev>:        Device to access.  May be file (foo.out) or device (/dev/dsk/etc)\n");
	printf("[devs...]:    Multiple devices can be specified.  I/O is distributed evenly and\n");
	printf("              randomly across the devices.\n");
	printf("\n");
	printf("Options:\n");
	printf("  -noflock           :  prevents the files from being locked during access\n");
	printf("  -output -filename  :  send all output from the command to 'filename'\n");
	printf("  -direct            :  disable file system caching - available in aix, solaris and linux\n");
	printf("\n");
	printf("Examples:\n");
	printf("  Random 4K Reads to files a.file, b.file for 10 seconds with 2 threads.\n");
	printf("  Accessing a total of 100 megabytes in each file.  File locks off.\n");
	printf("	sio_ntap 100 100 4096 100m 10 2 a.file b.file -noflock\n");
	printf("  Or by saving the output to file 'foo.out'\n"); 
	printf("	sio_ntap 100 100 4096 100m 10 2 a.file b.file -noflock -output -foo.out\n");
	printf("\n");
	printf("Notes:\n");
	printf("  This program supports these OS's: Windows, Solaris, Linux, HPUX, AIX\n");
	printf("\n");

	exit(0);
}

#ifdef WIN32
#define Thread __declspec(thread)

void win32_noop()
{
}

static void
win32_error (char *format)
{
    char *lpMsgBuf;

    FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,
		   GetLastError (),
		   MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		   (LPTSTR) & lpMsgBuf, 0, NULL);

    fprintf (stderr, format, lpMsgBuf);

    /* Free the buffer. */
    LocalFree (lpMsgBuf);
}

/*
 * Fake pthread implementation using Windows threads. Win threads
 * is generally a superset of pthreads, no lost functionality.
 */

void
pthread_attr_init (pthread_attr_t * attr)
{
    ;				/* does nothing */
}

void
pthread_attr_setscope (pthread_attr_t * attr, unsigned type)
{
    ;				/* does nothing */
}

int
pthread_create (pthread_t * tid,
		pthread_attr_t * attr, void *(*func) (void *), void *arg)
{
    unsigned long th;
    unsigned long thread_id;

    /*
     * Use CreateThread so we have a real thread handle
     * to synchronize on
     */
    th = (unsigned long) CreateThread (NULL,
				       0,
				       (LPTHREAD_START_ROUTINE) func,
				       arg, 0, &thread_id);
    if (th)
    {
	*tid = th;
	return (0);
    }

    return (-1);
}

void
pthread_exit (void *status)
{
    ExitThread (0);
}

void
pthread_kill (pthread_t t, int sig)
{
    if (sig == SIGKILL)
	TerminateThread ((HANDLE) t, sig);
    else
	fprintf (stderr, "Oops, can't kill threads nicely yet\n");
}

void
pthread_mutex_init (pthread_mutex_t * lock, void *name)
{
    *lock = CreateMutex (NULL, FALSE, NULL);
}

void
pthread_mutex_lock (pthread_mutex_t * lock)
{
    WaitForSingleObject (*lock, INFINITE);
}

void
pthread_mutex_unlock (pthread_mutex_t * lock)
{
    ReleaseMutex (*lock);
}

int
pthread_cond_init (pthread_cond_t * cv, const void *dummy)
{
    /* Create an auto-reset event */
    cv->events_[SIGNAL] = CreateEvent (NULL,	/* no security */
				       FALSE,	/* auto-reset event */
				       FALSE,	/* non-signaled initially */
				       NULL);	/* unnamed */

    /* Create a manual-reset event. */
    cv->events_[BROADCAST] = CreateEvent (NULL,	/* no security */
					  TRUE,	/* manual-reset */
					  FALSE,	/* non-signaled initially */
					  NULL);	/* unnamed */
    return (TRUE);
}

unsigned long
pthread_self ()
{
    return (unsigned long) GetCurrentThreadId ();
}


void
pthread_cond_wait (pthread_cond_t * cv, pthread_mutex_t * lock)
{
    /* Release the lock and wait for the other lock
     * in one move.
     *
     * N.B.
     *        This isn't strictly pthread_cond_wait, but it works
     *        for this program without any race conditions.
     *
     */
    SignalObjectAndWait (*lock, cv->events_[BROADCAST], INFINITE, TRUE);
    /* reacquire the lock */
    WaitForSingleObject (*lock, INFINITE);
}


void
pthread_cond_signal (pthread_cond_t * cv)
{
    /* Try to release one waiting thread. */
    PulseEvent (cv->events_[SIGNAL]);
}


void
pthread_cond_broadcast (pthread_cond_t * cv)
{
    /* Try to release all waiting threads. */
    PulseEvent (cv->events_[BROADCAST]);
}


int
pthread_join (pthread_t thread, void **exit_value)
{
    WaitForSingleObject ((HANDLE) thread, INFINITE);
    *exit_value = PTHREAD_NORMAL_EXIT;

    return (0);
}

int
gettimeofday (struct timeval *tv, struct timezone *not_used)
{
    SYSTEMTIME sys_time;
    GetSystemTime (&sys_time);

    tv->tv_sec = (long) time (NULL);
    tv->tv_usec = sys_time.wMilliseconds;

    return 0;
}

int
win32_open (char *name, int priv, int perm)
{
    HANDLE Handle;
    DWORD flags;
    DWORD access_flags = 0;

    /* 
     * SIO-specific, try to avoid Windows buffering
     * so enable write-through w/no buffering
     */
    flags = (FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_WRITE_THROUGH);
    flags |= FILE_FLAG_NO_BUFFERING;

    if (p_read == 100) {
	access_flags |= FILE_READ_DATA;
    } else {
	access_flags |= FILE_WRITE_DATA | FILE_READ_DATA;
    }
    Handle = CreateFile (name, access_flags,
			 FILE_SHARE_READ | FILE_SHARE_WRITE,
			 NULL, OPEN_EXISTING, flags, NULL);
    if (Handle == INVALID_HANDLE_VALUE)
    {

	win32_error ("CreateFile fails with %s\n");
	return (-1);
    }

    return ((int) Handle);
}


int
win32_read (HANDLE han, char *buf, ULONG size)
{
    ULONG bytesRead;
    BOOL op;

    op = ReadFile (han, buf, size, &bytesRead, NULL);
    if (!op)
    {
	win32_error ("ReadFile fails with %s\n");
	exit (1);
    }

    return (bytesRead);
}


int
win32_write (HANDLE han, char *buf, ULONG size)
{
    ULONG bytesWrit;
    BOOL op;

    op = WriteFile (han, buf, size, &bytesWrit, NULL);
    if (!op)
    {
	win32_error ("WriteFile fails with %s\n");
	exit (1);
    }


    return (bytesWrit);
}

int
usleep (UINT usecs)
{
    if (usecs > 1000000) {
	return -1;
    }
    Sleep (usecs / 1000);
    return 0;
}

UINT
sleep (UINT secs)
{
    Sleep (secs * 1000);
    return 0;
}

/* Do not simply redefine SEEK_XXX, they may be macros already */
static DWORD seek_map[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
/*
 * Note that this is a 64-bit seek, Offset_t better be 64 bits
 */
Offset_t win32_lseek (HANDLE han, Offset_t off, ULONG whence)
{
    LARGE_INTEGER li;
    li.QuadPart = off;
    li.LowPart = SetFilePointer (han, li.LowPart, &li.HighPart,
				 seek_map[whence]);
    if (li.LowPart == 0xffffffff && GetLastError () != NO_ERROR)
    {
	li.QuadPart = -1;
    }
    return li.QuadPart;
}

/*
 * Return TRUE if win2k or higher, FALSE otherwise
 */
static int
isWin2K ()
{
    OSVERSIONINFO osinfo;
    osinfo.dwOSVersionInfoSize = sizeof (osinfo);
    if (!GetVersionEx (&osinfo))
    {
	win32_error ("Unable to get Windows version %s\n");
	return FALSE;
    }
    if (debug)
	fprintf (stderr, "Windows version %d\n", osinfo.dwMajorVersion);
    if (osinfo.dwMajorVersion > 4)
    {
	return TRUE;
    }
    else
    {
	return FALSE;
    }
}

int
win32_pread (HANDLE han, char *buf, ULONG size, Offset_t offset)
{
    ULONG bytesRead;
    OVERLAPPED ol;
    BOOL res;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    ol.Offset = li.LowPart;
    ol.OffsetHigh = li.HighPart;
    ol.hEvent = NULL;

    res = ReadFile (han, buf, size, &bytesRead, &ol);
    if (!res)
    {
	if (GetLastError () == ERROR_IO_PENDING)
	{
	    res = GetOverlappedResult (han, &ol, &bytesRead, TRUE);
	}
	if (!res)
	{
	    win32_error ("ReadFile fails with %s\n");
	    bytesRead = -1;
	}
    }
    if(ol.hEvent!=NULL)
    {
	CloseHandle(ol.hEvent);
    }
    return bytesRead;
}

int
win32_pwrite (HANDLE han, char *buf, ULONG size, Offset_t offset)
{
    ULONG bytesWrite;
    OVERLAPPED ol;
    BOOL res;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    ol.Offset = li.LowPart;
    ol.OffsetHigh = li.HighPart;
    ol.hEvent = NULL;

    res = WriteFile (han, buf, size, &bytesWrite, &ol);
    if (!res)
    {
	if (GetLastError () == ERROR_IO_PENDING)
	{
	    res = GetOverlappedResult (han, &ol, &bytesWrite, TRUE);
	}
	if (!res)
	{
	    win32_error ("WriteFile fails with %s\n");
	    bytesWrite = -1;
	}
    }
    if(ol.hEvent!=NULL)
    {
	CloseHandle(ol.hEvent);
    }
    return bytesWrite;
}

#endif
