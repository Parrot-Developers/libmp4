/**
 * Copyright (c) 2023 Parrot Drones SAS
 */

#include <errno.h>
#include <getopt.h>
#include <libmp4.h>
#include <stdio.h>
#include <ulog.h>
#include <unistd.h>

#ifndef ULOG_TAG
#	define ULOG_TAG larry_recovery
#endif


static void welcome(const char *prog_name)
{
	printf("\n%s - MP4 file recovery program\n"
	       "Copyright (c) 2023 Parrot Drones SAS\n",
	       prog_name);
}


static void usage(const char *prog_name)
{
	/* clang-format off */
	printf("Usage: %s [options]\n"
	       "Options:\n"
		"  -h | --help                          "
		       "Print this message\n"
		"  -t | --tables                        "
		       "tables file path (usually named *.MRF)\n"
		"  -d | --data                          "
		       "data file path (usually named *.MP4 or *.TMP)\n"
		"  -p | --print                          "
		       "dump the given tables file and return\n"
       		"\n",
	       prog_name);
	/* clang-format on */
}


static const char short_options[] = "hl:t:d:p:";


static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"tables", required_argument, NULL, 't'},
	{"data", required_argument, NULL, 'd'},
	{"print", required_argument, NULL, 'p'},
	{0, 0, 0, 0},
};


int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS;
	int idx;
	int c;
	const char *tables_path = NULL;
	const char *data_path = NULL;
	char *error_msg = NULL;

	/* Command-line parameters */
	while ((c = getopt_long(
			argc, argv, short_options, long_options, &idx)) != -1) {
		switch (c) {
		case 0:
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case 't':
			tables_path = optarg;
			break;
		case 'd':
			data_path = optarg;
			break;
		case 'p':
			ret = mp4_recovery_dump_tables_file(optarg);
			if (ret < 0) {
				ULOG_ERRNO("mp4_recovery_dump_tables_file",
					   -ret);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	if (argc != optind) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (tables_path != NULL && data_path != NULL) {
		ret = mp4_recovery_recover_file_from_paths(
			tables_path, data_path, &error_msg, NULL);
		if (ret < 0) {
			ULOG_ERRNO("mp4_recovery_recover_file_from_paths (%s)",
				   -ret,
				   error_msg);
		}
	} else if (tables_path != NULL) {
		ret = mp4_recovery_recover_file(tables_path, &error_msg, NULL);
		if (ret < 0) {
			ULOG_ERRNO("mp4_recovery_recover_file (%s)",
				   -ret,
				   error_msg);
		}
	} else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("recovery %s\n", ret >= 0 ? "succeeded" : "failed");

	ret = mp4_recovery_finalize(tables_path, (ret < 0), data_path);
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_finalize", -ret);
		ret = EXIT_FAILURE;
	}

	free(error_msg);
	return ret;
}
