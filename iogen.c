#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "clparse.h"

typedef enum { READ, WRITE, RW } rw_t;

struct thread_info {
	pid_t	pid;
	int	index;

	unsigned int seed;
	int	log_only;
	unsigned long long min_io;
	unsigned long long max_io;
	unsigned long long min_span;
	unsigned long long max_span;
	rw_t	rw;

	char *device;
};

/* ---------- Get program arguments ---------- */

#define MAX_IO_DEFAULT		(128*1024*1024)
#define MAX_SPAN_DEFAULT	(1*1024*1024*1024)

static struct prog_opts {
	int	seed_set;
	unsigned int	seed;
	unsigned	log_only;
	unsigned	num_threads;
	unsigned long long min_io;
	unsigned long long max_io;
	unsigned long long min_span;
	unsigned long long max_span;
	rw_t	rw;
	int	num_devices;
	char	**devices;
} prog_opts = {
	.seed_set = 0,
	.seed = 0,
	.log_only = 0,
	.num_threads = 1,
	.min_io = 0,
	.max_io = MAX_IO_DEFAULT,
	.min_span = 0,
	.max_span = MAX_SPAN_DEFAULT,
	.rw = RW,
	.num_devices = 0,
	.devices = NULL,
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

	opts->seed_set = 1;

	return 0;
}

int set_log_only(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;

	opts->log_only = 1;
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

const struct clparse_opt cmd_opts[] = {
	{ '\0', "seed", 1, get_seed, "Initial random seed" },
	{ '\0', "log-only", 0, set_log_only, "Generate logs only" },
	{ '\0', "num-threads", 1, get_num_threads, "Number of IO threads" },
	{ '\0', "min-io", 1, get_min_io, "Minimum IO size" },
	{ '\0', "max-io", 1, get_max_io, "Maximum IO size" },
	{ '\0', "max-span", 1, get_max_span, "Maximum span" },
	{ '\0', "min-span", 1, get_min_span, "Minimum span" },
	{ '\0', "rw", 1, get_rw_op, "One of: READ, WRITE, RW" },
};

#define NUM_OPTIONS	(sizeof(cmd_opts)/sizeof(cmd_opts[0]))

/* ---------- Thread ---------- */

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

	fprintf(fp, "Thread: pid: %d\n", getpid());
	fprintf(fp, "Seed: %u\n", thread->seed);
	fprintf(fp, "Log only: %d\n", thread->log_only);
	fprintf(fp, "Min io: %llu\n", thread->min_io);
	fprintf(fp, "Max io: %llu\n", thread->max_io);
	fprintf(fp, "Min span: %llu\n", thread->min_span);
	fprintf(fp, "Max span: %llu\n", thread->max_span);
	fprintf(fp, "rw: %s\n", thread->rw == READ ? "READ" :
		thread->rw == WRITE ? "WRITE" : "RW");
	fprintf(fp, "Device: %s\n", thread->device);

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

	if (!prog_opts.seed_set)
		prog_opts.seed = rand();
	srand(prog_opts.seed);

	sprintf(parent_name, "/tmp/iogen.%d", getpid());
	fp = fopen(parent_name, "w+");
	if (fp == NULL) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			parent_name, strerror(errno));
		exit(1);
	}
	setvbuf(fp, NULL, _IONBF, 1);

	fprintf(fp, "Seed: %u\n", prog_opts.seed);
	fprintf(fp, "Log only: %s\n", prog_opts.log_only ? "yes" : "no");
	fprintf(fp, "Num threads: %d\n", prog_opts.num_threads);
	fprintf(fp, "Min io: %llu\n", prog_opts.min_io);
	fprintf(fp, "Max io: %llu\n", prog_opts.max_io);
	fprintf(fp, "Min span: %llu\n", prog_opts.min_span);
	fprintf(fp, "Max span: %llu\n", prog_opts.max_span);
	fprintf(fp, "rw: %s\n", prog_opts.rw == READ ? "READ" :
		prog_opts.rw == WRITE ? "WRITE" : "RW");
	fprintf(fp, "Num devices: %d\n", prog_opts.num_devices);
	for (i = 0; i < prog_opts.num_devices; i++)
		fprintf(fp, "    Device%d: %s\n", i, prog_opts.devices[i]);

	for (i = 0; i < prog_opts.num_threads; i++) {
		pid_t pid;

		thread[i].index = i;
		thread[i].seed = rand();
		thread[i].log_only = prog_opts.log_only;
		thread[i].min_io = prog_opts.min_io;
		thread[i].max_io = prog_opts.max_io;
		thread[i].min_span = prog_opts.min_span;
		thread[i].max_span = prog_opts.max_span;
		thread[i].rw = prog_opts.rw;
		thread[i].device = prog_opts.devices[i%prog_opts.num_devices];

		if ((pid = fork()) == 0) {
			/* child, never returns */
			do_thread(&thread[i]);
		} else {
			thread[i].pid = pid;
			fprintf(fp, "Thread %d\n", pid);
		}
	}

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

	fclose(fp);
	free(thread);
	free_devices();

	return 0;
}
