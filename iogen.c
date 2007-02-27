#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "clparse.h"

typedef enum { READ, WRITE, RW } rw_t;

struct thread_info {
	pid_t	pid;
	int	index;
	FILE	*fp;		  /* log output */

	unsigned int	seed;
	unsigned	dry_run;
	unsigned	io_log;
	unsigned long long min_io;
	unsigned long long max_io;
	unsigned long long min_span;
	unsigned long long max_span;
	long long	num_ios;
	rw_t	rw;

	char	*device;

	int	big_buf;
	char	*buf;

	unsigned fixed;
	unsigned seq;

	unsigned long long last_start;

	int fd;
};

/* ---------- Get program arguments ---------- */

#define DEFAULT_PARENT_SEED	0x5A33CF

#define MIN_IO_DEFAULT		512
#define MAX_IO_DEFAULT		(128*1024)
#define SMALL_BUF_LIMIT 	MAX_IO_DEFAULT

static struct prog_opts {
	unsigned int	seed;
	unsigned	dry_run;
	unsigned	io_log;
	unsigned	num_threads;
	unsigned long long min_io;
	unsigned long long max_io;
	unsigned long long min_span;
	unsigned long long max_span;
	long long	num_ios;
	rw_t	rw;
	int	num_devices;
	char	**devices;
	unsigned long long fixed;
	unsigned long long seq;
} prog_opts = {
	.seed = DEFAULT_PARENT_SEED,
	.dry_run = 0,
	.io_log = 0,
	.num_threads = 1,
	.min_io = MIN_IO_DEFAULT,
	.max_io = MAX_IO_DEFAULT,
	.min_span = 0,
	.max_span = 0,
	.num_ios = -1,
	.rw = READ,
	.num_devices = 0,
	.devices = NULL,
	.fixed = 0,
	.seq = 0,
};
	
static int get_ull_value(char *str, unsigned long long *val)
{
	char *end;

	*val = strtoull(str, &end, 0);
	if (end == str)
		return -1;
	else if (*end == ' ' || *end == '\0')
		return 0;
	else {
		switch (tolower(*end)) {
		case 'k':
			*val *= 1024;
			break;
		case 'm':
			*val *= 1024*1024;
			break;
		case 'g':
			*val *= 1024*1024*1024;
			break;
		default:
			return -1;
		}
	}

	return 0;
}

int get_seed(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	char *end;

	opts->seed = strtoul(value, &end, 0);
	if (end == value || (*end != ' ' && *end != '\0')) {
		fprintf(stderr, "Incorrect seed: %s\n", value);
		return -1;
	}

	return 0;
}

int set_dry_run(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;

	opts->dry_run = 1;

	return 0;
}

int get_num_threads(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	char *end;

	opts->num_threads = strtoul(value, &end, 0);
	if (end == value || (*end != ' ' && *end != '\0')) {
		fprintf(stderr, "Incorrect number of threads: %s\n", value);
		return -1;
	}
	return 0;
}


int get_min_io(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->min_io);
	if (res == -1) {
		fprintf(stderr, "Incorrect min_io: %s\n", value);
		return -1;
	}

	return 0;
}

int get_max_io(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->max_io);
	if (res == -1) {
		fprintf(stderr, "Incorrect max_io: %s\n", value);
		return -1;
	}

	return 0;
}

int get_min_span(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->min_span);
	if (res == -1) {
		fprintf(stderr, "Incorrect min_span: %s\n", value);
		return -1;
	}

	return 0;
}

int get_max_span(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->max_span);
	if (res == -1) {
		fprintf(stderr, "Incorrect max_span: %s\n", value);
		return -1;
	}

	return 0;
}

int get_devices(int index_last, int argc, char *argv[])
{
	int num_dev = argc - index_last;
	int i;

	prog_opts.devices = malloc(num_dev * sizeof(char *));
	if (!prog_opts.devices) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	for (i = 0; i < num_dev; i++)
		prog_opts.devices[i] = argv[i+index_last];

	prog_opts.num_devices = num_dev;

	return 0;
}

void free_devices(void)
{
	free(prog_opts.devices);
}

int get_rw_op(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;

	if (strcmp(value, "READ") == 0)
		opts->rw = READ;
	else if (strcmp(value, "WRITE") == 0)
		opts->rw = WRITE;
	else if (strcmp(value, "RW") == 0)
		opts->rw = RW;
	else {
		fprintf(stderr, "Incorrect value for rw: %s\n", value);
		return -1;
	}

	return 0;
}

int get_num_ios(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->num_ios);
	if (res) {
		fprintf(stderr, "Incorrect num_ios: %s\n", value);
		return -1;
	}

	return 0;
}

int set_io_log(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;

	opts->io_log = 1;

	return 0;
}

int get_fixed(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	int res;

	res = get_ull_value(value, &opts->fixed);
	if (res) {
		fprintf(stderr, "Incorrect fixed parameter: %s\n", value);
		return -1;
	}

	return 0;
}

int set_seq(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;

	opts->seq = 1;

	return 0;
}

const struct clparse_opt cmd_opts[] = {
	{ '\0', "seed", 1, get_seed, "Initial random seed" },
	{ '\0', "dry-run", 0, set_dry_run, "Do not actually do IO" },
	{ '\0', "io-log", 0, set_io_log, "Print IO ops log" },
	{ '\0', "num-threads", 1, get_num_threads, "Number of IO threads (default 1)" },
	{ '\0', "min-io", 1, get_min_io, "Minimum IO size (default 512)" },
	{ '\0', "max-io", 1, get_max_io, "Maximum IO size (default 128 KiB)" },
	{ '\0', "min-span", 1, get_min_span, "Minimum span (default 0)" },
	{ '\0', "max-span", 1, get_max_span, "Maximum span (default device size)" },
	{ '\0', "rw", 1, get_rw_op, "One of: READ, WRITE, RW (default: READ)" },
	{ '\0', "num-ios", 1, get_num_ios,
	  "Number of IO ops per thread (default: infinite)" },
	{ '\0', "fixed", 1, get_fixed,
	  "IO size is fixed at that number, i.e. not random\n" },
	{ '\0', "seq", 0, set_seq, "Do sequential IO, i.e. not random" },
};

#define NUM_OPTIONS	(sizeof(cmd_opts)/sizeof(cmd_opts[0]))

/* ---------- Time ---------- */

void print_time(FILE *fp)
{
	time_t t;
	struct tm tm;
	char s[100];
	char *p = s;

	t = time(NULL);
	gmtime_r(&t, &tm);
	asctime_r(&tm, s);
	for ( ; *p && *p != '\n'; p++)
		;
	sprintf(p, " UTC");
	fprintf(fp, "Time: %s\n", s);
}

/* ---------- Thread ---------- */

#define RANDOM(_A, _B)	((_A)+(unsigned long long)(((_B)-(_A)+1)*drand48()))

int do_io_op(struct thread_info *thread)
{
	int res = 0;
	rw_t rw;
	void *buf;
	size_t count;
	off64_t start;

	if (thread->rw == READ)
		rw = READ;
	else if (thread->rw == WRITE)
		rw = WRITE;
	else {
		rw = RANDOM(0, 1);
	}

	if (thread->fixed)
		count = thread->fixed;
	else
		count = RANDOM(thread->min_io, thread->max_io);

	if (thread->seq) {
		start = thread->last_start + count;
		if (start >= thread->max_span)
			start = 0;
		thread->last_start = start;
	} else
		start = RANDOM(thread->min_span, thread->max_span-count-1);

	if (thread->big_buf)
		buf = malloc(count);
	else
		buf = thread->buf;

	if (!buf) {
		fprintf(thread->fp, "Out of memory for buffer for "
			"rw: %s, offs: %lu, count: %lu\n",
			rw == READ ? "READ" : rw == WRITE ? "WRITE" : "RW",
			start, count);
		return -1;
	}

	if (!thread->dry_run) {
		if (lseek64(thread->fd, start, SEEK_SET) == -1) {
			fprintf(thread->fp, "lseek64 error (%s) for "
				"rw: %s, offs: %lu, count: %lu\n",
				strerror(errno),
				rw == READ ? "READ" : rw == WRITE ? "WRITE" : "RW",
				start, count);
			return -1;
		}

		if (rw == READ)
			res = read(thread->fd, buf, count);
		else
			res = write(thread->fd, buf, count);
	}

	if (thread->io_log) {
		fprintf(thread->fp, "Result: %6d, rw: %-5s, offs: %16lu, count: %6lu\n",
			res, rw == READ ? "READ" : rw == WRITE ? "WRITE" : "RW",
			start, count);
	}

	if (thread->big_buf)
		free(buf);

	return res;
}

FILE *thread_fp;

void sighandler_thread(int sig)
{
	fprintf(thread_fp, "Thread %d terminated by signal %d\n",
		getpid(), sig);
	print_time(thread_fp);
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

int do_thread(struct thread_info *thread)
{
	FILE *fp;
	char thread_name[255];

	sprintf(thread_name, "/tmp/iogen_thread.%d", getpid());
	fp = fopen(thread_name, "w+");
	if (fp == NULL) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			thread_name, strerror(errno));
		exit(1);
	}
	setvbuf(fp, NULL, _IONBF, 1);
	thread->fp = fp;
	thread_fp = fp;
	srand48(thread->seed);

	/* Catch and report terminating signals, then
	 * perform their default action (terminate).
	 */
	signal(SIGHUP, sighandler_thread);
	signal(SIGINT, sighandler_thread);
	signal(SIGTERM, sighandler_thread);
	signal(SIGUSR1, sighandler_thread);
	signal(SIGUSR2, sighandler_thread);

	print_time(fp);
	fprintf(fp, "Thread: pid: %d\n", getpid());
	fprintf(fp, "Seed: %u\n", thread->seed);
	fprintf(fp, "Dry run: %d\n", thread->dry_run);
	fprintf(fp, "IO log: %s\n", thread->io_log ? "yes" : "log");
	fprintf(fp, "Min io: %llu\n", thread->min_io);
	fprintf(fp, "Max io: %llu\n", thread->max_io);
	fprintf(fp, "Min span: %llu\n", thread->min_span);
	fprintf(fp, "Fixed: %s\n", thread->fixed ? "yes" : "no");
	fprintf(fp, "Sequential: %s\n", thread->seq ? "yes" : "no");

	if (!thread->dry_run) {
		thread->fd = open(thread->device, thread->rw == READ ? O_RDONLY :
				  thread->rw == WRITE ? O_WRONLY : O_RDWR);
		if (thread->fd == -1) {
			fprintf(fp, "Couldn't open device %s : %s\n", thread->device,
				strerror(errno));
			exit(1);
		}

		if (thread->max_span == 0) {
			off64_t end = lseek64(thread->fd, 0, SEEK_END);

			if (end == -1 || errno) {
				fprintf(thread->fp, "Couldn't determine the size of device %s "
					"since max-span not given\n", thread->device);
				fprintf(thread->fp, "error: %s\n", strerror(errno));
				exit(1);
			}

			thread->max_span = end;
		}
	}
	fprintf(fp, "Max span: %llu\n", thread->max_span);
	fprintf(fp, "rw: %s\n", thread->rw == READ ? "READ" :
		thread->rw == WRITE ? "WRITE" : "RW");
	fprintf(fp, "Num ios: %lld\n", thread->num_ios);
	fprintf(fp, "Device: %s\n", thread->device);

	if (thread->max_io <= SMALL_BUF_LIMIT) {
		thread->big_buf = 0;
		thread->buf = malloc(thread->max_io);

		if (!thread->buf) {
			fprintf(fp, "Couldn't allocate buffer of size %llu\n", thread->max_io);
			exit(1);
		}
	}

	do {
		do_io_op(thread);

		if (thread->num_ios == -1)
			;
		else if (--thread->num_ios <= 0)
			break;
	} while (1);

	if (!thread->big_buf)
		free(thread->buf);

	fprintf(fp, "Thread %d done\n", getpid());

	if (!thread->dry_run)
		close(thread->fd);
	print_time(fp);
	fclose(fp);
	exit(0);
}

/* ---------- Main program ---------- */

int main(int argc, char *argv[])
{
	int res, i;
	int index_last = -1;
	struct thread_info *thread;
	char parent_name[255];
	FILE *fp;

	res = cl_get_prog_opts(argc, argv, cmd_opts, NUM_OPTIONS, &prog_opts,
			       &index_last, 0);
	if (res) {
		fprintf(stderr,
			"Usage: iogen [options] <device> <device> ...\n");
		fprintf(stderr, "Options:\n");
		cl_print_opts_help(cmd_opts, NUM_OPTIONS);
		exit(1);
	}

	if (index_last == -1 || index_last >= argc) {
		fprintf(stderr, "No devices given\n");
		exit(1);
	}

	res = get_devices(index_last, argc, argv);
	if (res)
		exit(1);

	if (prog_opts.num_threads == 0)
		prog_opts.num_threads = 1;

	thread = malloc(prog_opts.num_threads * sizeof(*thread));
	if (!thread) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	memset(thread, 0, prog_opts.num_threads * sizeof(*thread));

	srand48(prog_opts.seed);

	sprintf(parent_name, "/tmp/iogen.%d", getpid());
	fp = fopen(parent_name, "w+");
	if (fp == NULL) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			parent_name, strerror(errno));
		exit(1);
	}
	setvbuf(fp, NULL, _IONBF, 1);

	print_time(fp);
	fprintf(fp, "Parent: pid: %d\n", getpid());
	fprintf(fp, "Seed: %u\n", prog_opts.seed);
	fprintf(fp, "Dry run: %s\n", prog_opts.dry_run ? "yes" : "no");
	fprintf(fp, "IO log: %s\n", prog_opts.io_log ? "yes" : "log");
	fprintf(fp, "Num threads: %d\n", prog_opts.num_threads);
	fprintf(fp, "Min io: %llu\n", prog_opts.min_io);
	fprintf(fp, "Max io: %llu\n", prog_opts.max_io);
	fprintf(fp, "Min span: %llu\n", prog_opts.min_span);
	fprintf(fp, "Max span: %llu\n", prog_opts.max_span);
	fprintf(fp, "rw: %s\n", prog_opts.rw == READ ? "READ" :
		prog_opts.rw == WRITE ? "WRITE" : "RW");
	fprintf(fp, "Num ios: %lld\n", prog_opts.num_ios);
	fprintf(fp, "Fixed: %s\n", prog_opts.fixed ? "yes" : "no");
	fprintf(fp, "Sequential: %s\n", prog_opts.seq ? "yes" : "no");
	fprintf(fp, "Num devices: %d\n", prog_opts.num_devices);
	for (i = 0; i < prog_opts.num_devices; i++)
		fprintf(fp, "    Device%d: %s\n", i, prog_opts.devices[i]);

	for (i = 0; i < prog_opts.num_threads; i++) {
		pid_t pid;

		thread[i].index = i;
		thread[i].seed = lrand48();
		thread[i].dry_run = prog_opts.dry_run;
		thread[i].io_log = prog_opts.io_log;
		thread[i].min_io = prog_opts.min_io;
		thread[i].max_io = prog_opts.max_io;
		thread[i].min_span = prog_opts.min_span;
		thread[i].max_span = prog_opts.max_span;
		thread[i].num_ios = prog_opts.num_ios;
		thread[i].rw = prog_opts.rw;
		thread[i].device = prog_opts.devices[i%prog_opts.num_devices];
		thread[i].big_buf = 0;
		thread[i].buf = NULL;
		thread[i].fixed = prog_opts.fixed;
		thread[i].seq = prog_opts.seq;
		thread[i].last_start = 0;

		if ((pid = fork()) == 0) {
			/* child, never returns */
			fclose(fp);
			do_thread(&thread[i]);
		} else {
			thread[i].pid = pid;
			fprintf(fp, "Thread %d\n", pid);
		}
	}

	/* When the children quit we report their status,
	 * then we quit too. */
	signal(SIGINT, SIG_IGN);

	do {
		int	status;
		pid_t	pid;

		pid = wait(&status);

		if (WIFSIGNALED(status)) {
			fprintf(fp, "Thread %d terminated by signal %d, "
				"status %d\n", pid, WTERMSIG(status),
				WEXITSTATUS(status));
		} else {
			fprintf(fp, "Thread %d exited with status %d\n", pid,
				WEXITSTATUS(status));
		}
	} while (--prog_opts.num_threads > 0);

	print_time(fp);
	fclose(fp);
	free(thread);
	free_devices();

	return 0;
}
