#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "clparse.h"

static struct prog_opts {
	unsigned int	seed;
	int	log_only;
	int	num_threads;
	unsigned long	min_io;
	unsigned long	max_io;
} prog_opts;

/* ---------- Get program arguments ---------- */

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
	char *end;

	opts->min_io = strtoul(value, &end, 0);
	if (end == value) {
		fprintf(stderr, "Incorrect min_io: %s\n", value);
		return -1;
	} else if (*end == ' ' || *end == '\0') {
		return 0;
	} else {
		switch (tolower(*end)) {
		case 'k':
			opts->min_io *= 1024;
			break;
		case 'm':
			opts->min_io *= 1024*1024;
			break;
		case 'g':
			opts->min_io *= 1024*1024*1024;
			break;
		default:
			fprintf(stderr, "Incorrect min_io: %s\n", value);
			return -1;
		}
	}

	return 0;
}

int get_max_io(char *value, void *_opts)
{
	struct prog_opts *opts = _opts;
	char *end;

	opts->max_io = strtoul(value, &end, 0);
	if (end == value) {
		fprintf(stderr, "Incorrect max_io: %s\n", value);
		return -1;
	} else if (*end == ' ' || *end == '\0') {
		return 0;
	} else {
		switch (tolower(*end)) {
		case 'k':
			opts->max_io *= 1024;
			break;
		case 'm':
			opts->max_io *= 1024*1024;
			break;
		case 'g':
			opts->max_io *= 1024*1024*1024;
			break;
		default:
			fprintf(stderr, "Incorrect max_io: %s\n", value);
			return -1;
		}
	}

	return 0;
}

int get_devices(int index_last, int argc, char *argv[])
{
	return 0;
}

const struct clparse_opt cmd_opts[] = {
	{ '\0', "seed", 1, get_seed, "Initial random seed" },
	{ '\0', "log-only", 0, set_log_only, "Generate logs only" },
	{ '\0', "threads", 1, get_num_threads, "Number of IO threads" },
	{ '\0', "min-io", 1, get_min_io, "Minimum IO size" },
	{ '\0', "max-io", 1, get_max_io, "Maximum IO size" }
};

#define NUM_OPTIONS	(sizeof(cmd_opts)/sizeof(cmd_opts[0]))

int main(int argc, char *argv[])
{
	int res;
	int index_last = -1;

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

	return 0;
}


