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
	       "  -l | --link                          "
		       "link file path (usually named *.CHK)\n"
		   "  -t | --tables                        "
		       "tables file path (usually named *.MRF)\n"
		   "  -d | --data                          "
		       "data file path (usually named *.MP4 or *.TMP)\n"
       "\n",
	       prog_name);
	/* clang-format on */
}


static const char short_options[] = "hl:t:d:";


static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"link", required_argument, NULL, 'l'},
	{"tables", required_argument, NULL, 't'},
	{"data", required_argument, NULL, 'd'},
	{0, 0, 0, 0},
};


int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS;
	int idx, c;
	char *tables_path = NULL;
	char *link_path = NULL;
	char *data_path = NULL;
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
		case 'l':
			link_path = optarg;
			break;
		case 't':
			tables_path = optarg;
			break;
		case 'd':
			data_path = optarg;
			break;
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

	if (link_path == NULL) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((tables_path == NULL || data_path == NULL) &&
	    (tables_path != data_path)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (tables_path != NULL) {
		ret = mp4_recovery_recover_file_from_paths(
			link_path, tables_path, data_path, &error_msg, NULL);
		if (ret < 0) {
			ULOG_ERRNO("mp4_recovery_recover_file_from_paths (%s)",
				   -ret,
				   error_msg);
		}
	} else {
		ret = mp4_recovery_recover_file(link_path, &error_msg, NULL);
		if (ret < 0) {
			ULOG_ERRNO("mp4_recovery_recover_file (%s)",
				   -ret,
				   error_msg);
		}
	}

	printf("recovery %s\n", ret >= 0 ? "succeeded" : "failed");

	ret = mp4_recovery_finalize(link_path, (ret < 0));
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_finalize", -ret);
		ret = -EXIT_FAILURE;
	}

	free(error_msg);
	return ret;
}
