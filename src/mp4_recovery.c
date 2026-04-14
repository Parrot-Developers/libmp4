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
	const struct mntent *e;
	char *devname = NULL;
	size_t length_mnt = 0;
	size_t curr_len = 0;

	if (fstab == NULL || path == NULL)
		goto out;

	while ((e = getmntent(fstab))) {
		curr_len = mp4_validate_str_len(e->mnt_dir, PATH_MAX);
		if ((curr_len > length_mnt) &&
		    (strncmp(path, e->mnt_dir, curr_len) == 0)) {
			if (devname)
				free(devname);
			devname = strdup(e->mnt_fsname);
			length_mnt = curr_len;
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


int mp4_mux_recovery_tables_header_fill(
	const struct mp4_mux *mux,
	struct mp4_recovery_tables_header *header)
{
	int ret = 0;
	off_t curr_off = 0;
	char *fsname = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	/* Get current position */
	curr_off = lseek(mux->recovery.fd_tables, 0, SEEK_CUR);
	if (curr_off == -1) {
		ret = -errno;
		goto out;
	}

	header->magic = MP4_RECOVERY_TABLES_HEADER_MAGIC;
	header->version = MP4_RECOVERY_FORMAT_VERSION_CURRENT;
	header->tables_size = (uint64_t)curr_off;
	header->mux_tables_size = mux->data_offset / 1024 / 1024;
	if (header->data_path)
		free(header->data_path);
	header->data_path = strdup(mux->filename);
	header->data_path_length =
		(uint32_t)mp4_validate_str_len(mux->filename, PATH_MAX);

	if (!mux->recovery.check_storage_uuid) {
		header->uuid = NULL;
		goto out;
	}
	fsname = get_mnt_fsname(mux->filename);
	if (fsname == NULL) {
		ULOGE("get_mnt_fsname failed (%s)", mux->filename);
		goto out;
	}
	if (header->uuid)
		free(header->uuid);
	header->uuid = get_uuid_from_mnt_fsname(fsname);
	if (header->uuid == NULL) {
		ULOGE("%s: get_uuid_from_mnt_fsname %s failed.",
		      mux->filename,
		      fsname);
		goto out;
	}

out:
	free(fsname);
	return ret;
}


MP4_API int mp4_recovery_finalize(const char *tables_file,
				  bool truncate_file,
				  const char *override_data_path)
{
	int ret = 0;
	struct mp4_recovery_tables_header header = {};
	const char *data_path = override_data_path;

	ULOG_ERRNO_RETURN_ERR_IF(tables_file == NULL, EINVAL);

	if (truncate_file) {
		if (data_path == NULL) {
			ret = mp4_recovery_tables_header_read_file(tables_file,
								   &header);
			if (ret < 0) {
				ULOG_ERRNO(
					"mp4_recovery_tables_header_read_file",
					-ret);
				goto out;
			}
			data_path = header.data_path;
		}

		ret = truncate(data_path, 0);
		if (ret < 0) {
			ret = -errno;
			ULOG_ERRNO("truncate (%s)", -ret, data_path);
		}
	}

out:
	ret = remove(tables_file);
	if (ret < 0) {
		ret = -errno;
		ULOG_ERRNO("remove (%s)", -ret, tables_file);
	}

	(void)mp4_recovery_tables_header_clear(&header);

	return ret;
}


static int
mp4_recovery_mux_open(const char *data_file,
		      const char *tables_file,
		      const struct mp4_recovery_tables_header *header,
		      char **error_msg,
		      char **recovered_file)
{
	int ret;
	struct mp4_mux *mux = NULL;
	size_t tables_size_mbytes = header->mux_tables_size / 1024 / 1024;
	struct mp4_mux_config config = {
		.filename = data_file,
		.timescale = 1000000, /* unused - can't be 0 for mux_open */
		.creation_time = 1000, /* unused - can't be 0 for mux_open */
		.modification_time =
			1000, /* unused - can't be 0 for mux_open */
		.tables_size_mbytes = (tables_size_mbytes != 0)
					      ? tables_size_mbytes
					      : MP4_MUX_DEFAULT_TABLE_SIZE_MB,
		.recovery.tables_file = NULL,
	};

	ret = mp4_mux_open(&config, &mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_open", -ret);
		*error_msg = strdup("failed to open data_file");
		goto out;
	}

	ret = mp4_mux_fill_from_file(header, tables_file, mux, error_msg);
	if (ret < 0) {
		*error_msg = strdup("failed to open data_file");
		ULOG_ERRNO("recovery failed (%s)", -ret, *error_msg);
	}

	ret = mp4_mux_close(mux);
	if (ret < 0) {
		*error_msg = strdup("failed to rewrite data_file");
		ULOG_ERRNO("recovery failed (%s)", -ret, *error_msg);
		goto out;
	}

	if (recovered_file != NULL) {
		*recovered_file = strdup(data_file);
		if (*recovered_file == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("strdup", -ret);
		}
	}

out:
	return ret;
}


static int check_file_integrity(const char *file, bool tables, char **error_msg)
{
	int ret = 0;
	struct stat st_stats;

	if (file == NULL) {
		*error_msg = strdup("null tables file");
		ULOGE("%s", *error_msg);
		ret = -EINVAL;
		goto out;
	}

	if (access(file, F_OK)) {
		*error_msg = strdup(tables ? "failed to access tables file"
					   : "failed to access data file");
		ULOGE("%s (%s)", *error_msg, file);
		ret = -ENOENT;
		goto out;
	}

	if (stat(file, &st_stats) < 0) {
		ret = -errno;
		*error_msg = strdup(tables ? "invalid tables file"
					   : "invalid data file");
		ULOGE("%s (%s)", *error_msg, file);
		goto out;
	}

	if (tables && (st_stats.st_size == 0)) {
		/* Record was probably stopped before any sync */
		*error_msg = strdup("failed to parse tables file");
		ULOGE("%s (%s): empty tables file (record probably stopped"
		      " before any sync)",
		      *error_msg,
		      file);
		ret = -ENODATA;
		goto out;
	}

out:
	return ret;
}


MP4_API int mp4_recovery_recover_file_from_paths(const char *tables_file,
						 const char *data_file,
						 char **error_msg,
						 char **recovered_file)
{
	int ret = 0;
	struct mp4_recovery_tables_header header = {};
	int fd_tables;

	ULOG_ERRNO_RETURN_ERR_IF(tables_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(error_msg == NULL, EINVAL);

	fd_tables = open(tables_file, O_RDWR);
	if (fd_tables < 0) {
		ret = -errno;
		*error_msg = strdup("failed to parse tables file");
		ULOG_ERRNO("open:'%s'", -ret, tables_file);
		return ret;
	}

	ret = check_file_integrity(tables_file, true, error_msg);
	if (ret < 0)
		goto out;

	ret = check_file_integrity(data_file, false, error_msg);
	if (ret < 0)
		goto out;

	ret = mp4_recovery_tables_header_read_fd(fd_tables, &header);
	if (ret < 0) {
		*error_msg = strdup("failed to parse tables file");
		ULOGE("%s (%s)", *error_msg, tables_file);
		goto out;
	}

	if (header.tables_size == 0) {
		/* Record was probably stopped before any sync */
		*error_msg = strdup("failed to parse tables file");
		ULOGE("%s (%s): empty tables file (record probably stopped"
		      " before any sync)",
		      *error_msg,
		      tables_file);
		ret = -ENODATA;
		goto out;
	}

	ret = mp4_recovery_mux_open(
		data_file, tables_file, &header, error_msg, recovered_file);
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_mux_open", -ret);
		goto out;
	}

out:
	close(fd_tables);
	(void)mp4_recovery_tables_header_clear(&header);
	return ret;
}


MP4_API int mp4_recovery_recover_file(const char *tables_file,
				      char **error_msg,
				      char **recovered_file)
{
	int ret = 0;
	char *fsname = NULL;
	char *uuid2 = NULL;
	struct mp4_recovery_tables_header header = {};

	ULOG_ERRNO_RETURN_ERR_IF(tables_file == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(error_msg == NULL, EINVAL);

	*error_msg = NULL;
	if (recovered_file != NULL)
		*recovered_file = NULL;

	ret = check_file_integrity(tables_file, true, error_msg);
	if (ret < 0)
		goto out;


	ret = mp4_recovery_tables_header_read_file(tables_file, &header);
	if (ret < 0) {
		*error_msg = strdup("failed to parse link file");
		ULOG_ERRNO("mp4_recovery_parse_tables_file", -ret);
		ULOGE("%s (%s)", *error_msg, tables_file);
		goto out;
	}

	if (header.tables_size == 0) {
		/* Record was probably stopped before any sync */
		*error_msg = strdup("failed to parse tables file");
		ULOGE("%s (%s): empty tables file (record probably stopped"
		      " before any sync)",
		      *error_msg,
		      tables_file);
		ret = -ENODATA;
		goto out;
	}

	ret = check_file_integrity(header.data_path, false, error_msg);
	if (ret < 0)
		goto out;

	if (header.uuid_length != 0) {
		fsname = get_mnt_fsname(header.data_path);
		uuid2 = get_uuid_from_mnt_fsname(fsname);
		if (uuid2 == NULL) {
			*error_msg = strdup("cannot get storage UUID");
			ULOGE("%s (%s)", *error_msg, header.data_path);
			ret = -EAGAIN;
			goto out;
		}
		size_t len_uuid1 = mp4_validate_str_len(header.uuid, NAME_MAX);
		size_t len_uuid2 = mp4_validate_str_len(uuid2, NAME_MAX);
		if ((len_uuid1 == 0) || (len_uuid2 == 0) ||
		    (strcmp(header.uuid, uuid2) != 0)) {
			*error_msg = strdup("storage uuid doesn't match");
			ULOGE("%s (%s %s)", *error_msg, header.uuid, uuid2);
			ret = -EAGAIN;
			goto out;
		}
	}

	ret = mp4_recovery_mux_open(header.data_path,
				    tables_file,
				    &header,
				    error_msg,
				    recovered_file);
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_mux_open", -ret);
		goto out;
	}

out:
	free(fsname);
	free(uuid2);
	(void)mp4_recovery_tables_header_clear(&header);
	return ret;
}
