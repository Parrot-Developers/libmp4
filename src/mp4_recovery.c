/**
 * Copyright (c) 2018 Parrot Drones SAS
 * Copyright (c) 2016 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mp4_priv.h"
#if BUILD_UTIL_LINUX_NG
#	include <blkid/blkid.h>
#	include <mntent.h>
#endif /* BUILD_UTIL_LINUX_NG */
#include <dirent.h>
#include <fcntl.h>
#include <futils/fs.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MP4_MUX_TABLES_GROW_SIZE 128
#define MS_TO_S 1000
#define SECONDS_IN_MONTH 267840
#define FTYP_SIZE 32
#define DEFAULT_UUID_MSG "DON'T CHECK UUID"

#ifndef PATH_MAX
#	define PATH_MAX 4096
#endif

#ifdef WIN32
#	include <io.h>
#	define F_OK 0
#	define access _access
#else
#	include <unistd.h>
#endif

static const uint32_t recovery_version = 2;

/* recovery version 1: moov atom entirely written in mrf file. Not supported
 * anymore */
/* recovery version 2: allows incremental tables */

#ifdef _WIN32
static ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	char *bufptr = NULL;
	char *p = bufptr;
	size_t size;
	int c;

	if (lineptr == NULL || stream == NULL || n == NULL)
		return -EINVAL;

	bufptr = *lineptr;
	size = *n;

	c = fgetc(stream);
	if (c == EOF)
		return -1;
	if (bufptr == NULL) {
		bufptr = malloc(128);
		if (bufptr == NULL)
			return -1;
		size = 128;
	}
	p = bufptr;
	while (c != EOF) {
		if ((size_t)(p - bufptr) > (size_t)(size - 1)) {
			size = size + 128;
			bufptr = realloc(bufptr, size);
			if (bufptr == NULL)
				return -1;
		}
		*p++ = c;
		if (c == '\n')
			break;
		c = fgetc(stream);
	}
	*p++ = '\0';
	*lineptr = bufptr;
	*n = size;

	return p - bufptr - 1;
}
#endif


static char *get_mnt_fsname(const char *path)
{
#if BUILD_UTIL_LINUX_NG
	FILE *fstab = setmntent("/etc/mtab", "r");
	struct mntent *e;
	char *devname = NULL;
	size_t length_mnt = 0;
	size_t curr_len = 0;

	if (fstab == NULL || path == NULL)
		goto out;

	while ((e = getmntent(fstab))) {
		curr_len = strlen(e->mnt_dir);
		if (curr_len > length_mnt) {
			if (strncmp(path, e->mnt_dir, curr_len) == 0) {
				if (devname)
					free(devname);
				devname = strdup(e->mnt_fsname);
				length_mnt = curr_len;
			}
		}
	}
out:
	endmntent(fstab);
	return devname;
#else
	return NULL;
#endif
}


static char *get_uuid_from_mnt_fsname(const char *path)
{
#if BUILD_UTIL_LINUX_NG
	blkid_probe pr = NULL;
	const char *uuid;
	char *ret = NULL;
	int err = 0;

	if (path == NULL)
		goto out;
	pr = blkid_new_probe_from_filename(path);
	if (pr == NULL)
		goto out;
	err = blkid_do_probe(pr);
	if (err < 0) {
		ULOGE("blkid_do_probe");
		goto out;
	}
	err = blkid_probe_lookup_value(pr, "UUID", &uuid, NULL);
	if (err < 0) {
		ULOGE("blkid_probe_lookup_value");
		goto out;
	}
	ret = strdup(uuid);
	if (ret == NULL)
		ULOG_ERRNO("strdup", ENOMEM);
out:
	if (pr)
		blkid_free_probe(pr);
	return ret;
#else
	return NULL;
#endif
}


int mp4_prepare_link_file(int fd_link_file,
			  const char *tables_file,
			  const char *filepath,
			  off_t tables_size_bytes,
			  bool check_storage_uuid)
{
	int ret = 0;
	char bl = '\n';
	ssize_t written = 0;
	char *str = NULL;
	char *fsname = NULL;
	char *uuid = NULL;

	written = asprintf(&str, "%d\n", recovery_version);
	if (written < 0) {
		ret = -ENOMEM;
		goto out;
	}
	written = write(fd_link_file, str, strlen(str));
	if (written != (ssize_t)strlen(str)) {
		ret = -errno;
		goto out;
	}
	free(str);

	written = asprintf(&str, "%s\n", filepath);
	if (written < 0) {
		ret = -ENOMEM;
		goto out;
	}
	written = write(fd_link_file, str, strlen(str));
	if (written != (ssize_t)strlen(str)) {
		ret = -errno;
		goto out;
	}
	free(str);

	written = asprintf(&str, "%s\n", tables_file);
	if (written < 0) {
		ret = -ENOMEM;
		goto out;
	}
	written = write(fd_link_file, str, strlen(str));
	if (written != (ssize_t)strlen(str)) {
		ret = -errno;
		goto out;
	}
	free(str);

	written = asprintf(&str, "%jd\n", (intmax_t)tables_size_bytes);
	if (written < 0) {
		ret = -ENOMEM;
		goto out;
	}
	written = write(fd_link_file, str, strlen(str));
	if (written != (ssize_t)strlen(str)) {
		ret = -errno;
		goto out;
	}

	if (check_storage_uuid) {
		fsname = get_mnt_fsname(filepath);
		if (fsname == NULL) {
			ULOGE("get_mnt_fsname failed (%s)", filepath);
			goto out;
		} else {
			uuid = get_uuid_from_mnt_fsname(fsname);
			if (uuid == NULL) {
				ULOGE("%s: get_uuid_from_mnt_fsname %s failed.",
				      filepath,
				      fsname);
				goto out;
			}
		}
	} else {
		uuid = strdup(DEFAULT_UUID_MSG);
	}

	written = write(fd_link_file, uuid, strlen(uuid));
	if (written != (ssize_t)strlen(uuid)) {
		ret = -errno;
		goto out;
	}
	written = write(fd_link_file, &bl, 1);
	if (written != (ssize_t)1) {
		ret = -errno;
		goto out;
	}

out:
	free(str);
	free(fsname);
	free(uuid);
	return ret;
}


MP4_API int mp4_recovery_parse_link_file(const char *link_file,
					 struct link_file_info *info)
{
	int ret = 0;
	int err = 0;
	ssize_t read_char = 0;
	FILE *f_link = NULL;
	char *curr_recovery_version = NULL;
	char *tables_size_b = NULL;
	size_t len = 0;
	char *end_ptr = NULL;
	unsigned long parse_long;

	ULOG_ERRNO_RETURN_ERR_IF(link_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(info == NULL, EINVAL);

	memset(info, 0, sizeof(*info));
	f_link = fopen(link_file, "r");
	if (f_link == NULL) {
		ret = -errno;
		ULOG_ERRNO("fopen ('%s')", -ret, link_file);
		return ret;
	}

	/* recovery version */
	if (getline(&curr_recovery_version, &len, f_link) < 0) {
		ret = -EINVAL;
		goto out;
	}
	info->recovery_version = atoi(curr_recovery_version);
	if (info->recovery_version != recovery_version) {
		ULOGE("unsupported recovery version (%d)",
		      (int)info->recovery_version);
		ret = -EINVAL;
		goto out;
	}

	/* data_file */
	read_char = getline(&info->data_file, &len, f_link);
	if (read_char <= 0 || info->data_file == NULL) {
		ULOGE("getline");
		ret = -EINVAL;
		goto out;
	}
	info->data_file[strlen(info->data_file) - 1] = '\0';

	/* tables_file */
	read_char = getline(&info->tables_file, &len, f_link);
	if (read_char <= 0 || info->tables_file == NULL) {
		ULOGE("getline");
		ret = -EINVAL;
		goto out;
	}
	info->tables_file[strlen(info->tables_file) - 1] = '\0';

	/* tables size */
	read_char = getline(&tables_size_b, &len, f_link);
	if (read_char <= 0 || tables_size_b == NULL) {
		ULOGE("getline");
		ret = -EINVAL;
		goto out;
	}
	tables_size_b[strlen(tables_size_b) - 1] = '\0';
	errno = 0;
	parse_long = strtoul(tables_size_b, &end_ptr, 0);
	if (errno != 0 || tables_size_b[0] == '\0' || end_ptr[0] != '\0') {
		ret = -errno;
		ULOG_ERRNO("strtoul", -ret);
		goto out;
	}
	if (parse_long == 0) {
		ret = -EINVAL;
		ULOGE("invalid tables size (%ld)", parse_long);
		goto out;
	}
	info->tables_size_b = (size_t)parse_long;

	/* uuid */
	read_char = getline(&info->uuid, &len, f_link);
	if (read_char > 0 && info->uuid != NULL) {
		info->uuid[strlen(info->uuid) - 1] = '\0';
		if (strncmp(DEFAULT_UUID_MSG,
			    info->uuid,
			    MIN(sizeof(DEFAULT_UUID_MSG),
				strlen(info->uuid))) == 0) {
			/* don't check storage uuid */
			free(info->uuid);
			info->uuid = NULL;
		}
	} else {
		ULOGW("invalid storage uuid");
		free(info->uuid);
		info->uuid = NULL;
	}

out:
	if (info->data_file == NULL || info->tables_file == NULL)
		ret = -EINVAL;
	if (f_link != NULL) {
		err = fclose(f_link);
		if (err < 0) {
			ULOG_ERRNO("fclose", errno);
			if (ret == 0)
				ret = -errno;
		}
	}
	free(curr_recovery_version);
	free(tables_size_b);
	if (ret < 0)
		(void)mp4_recovery_link_file_info_destroy(info);
	return ret;
}


MP4_API int mp4_recovery_link_file_info_destroy(struct link_file_info *info)
{
	ULOG_ERRNO_RETURN_ERR_IF(info == NULL, EINVAL);

	free(info->tables_file);
	info->tables_file = NULL;

	free(info->data_file);
	info->data_file = NULL;

	free(info->uuid);
	info->uuid = NULL;

	return 0;
}


MP4_API int mp4_recovery_finalize(const char *link_file, bool truncate_file)
{
	struct link_file_info info = {0};
	int ret = 0;
	int err = 0;

	ULOG_ERRNO_RETURN_ERR_IF(link_file == NULL, EINVAL);

	ret = mp4_recovery_parse_link_file(link_file, &info);
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_parse_link_file", -ret);
		goto out;
	}

	if (truncate_file) {
		err = truncate(info.data_file, 0);
		if (err < 0)
			ULOG_ERRNO("truncate (%s)", errno, info.data_file);
	}

	err = remove(info.tables_file);
	if (err < 0)
		ULOG_ERRNO("remove (%s)", errno, info.tables_file);

out:
	err = remove(link_file);
	if (err < 0)
		ULOG_ERRNO("remove (%s)", errno, link_file);

	(void)mp4_recovery_link_file_info_destroy(&info);
	return ret;
}


MP4_API int mp4_recovery_recover_file(const char *link_file,
				      char **error_msg,
				      char **recovered_file)
{
	int ret = 0;
	char *fsname = NULL;
	char *uuid2 = NULL;
	struct mp4_mux *mux;
	struct mp4_mux_config config = {};
	struct stat st_tables;
	struct stat st_data;
	struct link_file_info info = {0};

	ULOG_ERRNO_RETURN_ERR_IF(error_msg == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(link_file == NULL, EINVAL);

	*error_msg = NULL;
	if (recovered_file != NULL)
		*recovered_file = NULL;

	ret = mp4_recovery_parse_link_file(link_file, &info);
	if (ret < 0) {
		*error_msg = strdup("failed to parse link file");
		ULOG_ERRNO("mp4_recovery_parse_link_file", -ret);
		ULOGE("%s (%s)", *error_msg, link_file);
		goto out;
	}

	if (access(info.data_file, F_OK)) {
		*error_msg = strdup("failed to find data file");
		ULOGE("%s (%s)", *error_msg, info.data_file);
		ret = -ENOENT;
		goto out;
	}

	if (info.uuid != NULL) {
		fsname = get_mnt_fsname(info.data_file);
		uuid2 = get_uuid_from_mnt_fsname(fsname);
		if (uuid2 == NULL) {
			*error_msg = strdup("cannot get storage UUID");
			ULOGE("%s (%s)", *error_msg, info.data_file);
			ret = -EAGAIN;
			goto out;
		}
		if (strlen(uuid2) != strlen(info.uuid) ||
		    strcmp(info.uuid, uuid2) != 0) {
			*error_msg = strdup("storage uuid doesn't match");
			ULOGE("%s (%s %s)", *error_msg, info.uuid, uuid2);
			ret = -EAGAIN;
			goto out;
		}
	}

	if (access(info.tables_file, F_OK)) {
		*error_msg = strdup("failed to find tables file");
		ULOGE("%s (%s)", *error_msg, info.tables_file);
		ret = -ENOENT;
		goto out;
	}

	if (stat(info.tables_file, &st_tables) < 0) {
		ret = -errno;
		*error_msg = strdup("invalid tables file");
		ULOGE("%s (%s)", *error_msg, info.tables_file);
		goto out;
	}

	if (stat(info.data_file, &st_data) < 0) {
		ret = -errno;
		*error_msg = strdup("invalid data file");
		ULOGE("%s (%s)", *error_msg, info.data_file);
		goto out;
	}

	if (st_tables.st_size == 0) {
		/* Record was probably stopped before any sync */
		*error_msg = strdup("failed to parse tables file");
		ULOGE("%s (%s): empty tables file (record probably stopped"
		      " before any sync)",
		      *error_msg,
		      info.tables_file);
		ret = -ENODATA;
		goto out;
	}

	ULOGI("starting recovery of file: %s "
	      "using recovery file path: %s",
	      info.data_file,
	      info.tables_file);

	config.filename = info.data_file;
	config.timescale = 1000000; /* unused - can't be 0 for mux_open */
	config.creation_time = 1000; /* unused - can't be 0 for mux_open */
	config.modification_time = 1000; /* unused - can't be 0 for mux_open */
	config.tables_size_mbytes = info.tables_size_b / 1024 / 1024;
	if (config.tables_size_mbytes == 0)
		config.tables_size_mbytes = MP4_MUX_DEFAULT_TABLE_SIZE_MB;
	config.recovery.link_file = NULL;
	config.recovery.tables_file = NULL;
	ret = mp4_mux_open(&config, &mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_open", -ret);
		*error_msg = strdup("failed to open data_file");
		goto out;
	}

	ret = mp4_mux_fill_from_file(info.tables_file, mux, error_msg);
	if (ret < 0)
		ULOG_ERRNO("recovery failed (%s)", -ret, *error_msg);

	ret = mp4_mux_close(mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_close", -ret);
		goto out;
	}

	if (recovered_file != NULL) {
		*recovered_file = strdup(info.data_file);
		if (*recovered_file == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("strdup", -ret);
		}
	}

out:
	free(fsname);
	free(uuid2);
	(void)mp4_recovery_link_file_info_destroy(&info);
	return ret;
}


MP4_API int mp4_recovery_recover_file_from_paths(const char *link_file,
						 const char *tables_file,
						 const char *data_file,
						 char **error_msg,
						 char **recovered_file)
{
	int ret = 0;
	struct link_file_info info = {0};
	int fd_link;
	struct stat st_tables;
	struct stat st_data;

	ULOG_ERRNO_RETURN_ERR_IF(link_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(tables_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(error_msg == NULL, EINVAL);

	fd_link = open(link_file, O_WRONLY);
	if (fd_link < 0) {
		ret = -errno;
		*error_msg = strdup("failed to parse link file");
		ULOG_ERRNO("open:'%s'", -ret, link_file);
		return ret;
	}

	if (stat(tables_file, &st_tables) < 0) {
		ret = -errno;
		*error_msg = strdup("invalid tables file");
		ULOGE("%s (%s)", *error_msg, tables_file);
		goto error;
	}

	if (stat(data_file, &st_data) < 0) {
		ret = -errno;
		*error_msg = strdup("invalid data file");
		ULOGE("%s (%s)", *error_msg, data_file);
		goto error;
	}

	ret = mp4_recovery_parse_link_file(link_file, &info);
	if (ret < 0) {
		*error_msg = strdup("failed to parse link file");
		ULOGE("%s (%s)", *error_msg, link_file);
		goto error;
	}

	ret = mp4_prepare_link_file(
		fd_link, tables_file, data_file, info.tables_size_b, false);
	if (ret < 0) {
		ULOG_ERRNO("mp4_prepare_link_file", -ret);
		goto error;
	}
	close(fd_link);
	(void)mp4_recovery_link_file_info_destroy(&info);
	return mp4_recovery_recover_file(link_file, error_msg, recovered_file);

error:
	close(fd_link);
	return ret;
}
