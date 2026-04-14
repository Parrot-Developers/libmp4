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

#include "mp4_test.h"
#include <unistd.h>

static const struct {
	bool recovery_version_supported;
	const char *tables_file;
	const char *data_file;
	const char *copied_tables;
	const char *copied_data;
	struct mp4_recovery_tables_header header;
} assets_tests_mp4_recovery[] = {
	{
		.recovery_version_supported = true,
		.tables_file = "Tests/recovery/0000016_video.MRF",
		.data_file = "Tests/recovery/0000016_video.TMP",
		.copied_tables = "/tmp/0000016_video.MRF",
		.copied_data = "/tmp/0000016_videoTMP.CPY",
		.header =
			{
				.magic = MP4_RECOVERY_TABLES_HEADER_MAGIC,
				.tables_size = 324100,
				.mux_tables_size = 5,
				.version = 3,
				.data_path =
					"/tmp/regis-video/user/DCIM//Flights/"
					"2026.01.12 09h39/0000016_video.TMP",
				.data_path_length =
					sizeof("/tmp/regis-video/user/DCIM//"
					       "Flights/2026.01.12 09h39/"
					       "0000016_video.TMP") -
					1,
				.uuid = "",
				.uuid_length = 0,
			},
	},
	{
		.recovery_version_supported = false,
		.tables_file = "Tests/recovery/0000003_video.MRF",
		.data_file = "Tests/recovery/0000003_video.TMP",
		.copied_tables = NULL,
		.copied_data = NULL,
		.header = {},
	},
};


/*
 * From
 * https://stackoverflow.com/questions/23178191/
 */
static int copy_file(const char *src, const char *dst)
{
	FILE *in = fopen(src, "rb");
	FILE *out = fopen(dst, "wb");

	char buf[1024];
	int read = 0;
	ssize_t written = 0;

	CU_ASSERT_PTR_NOT_NULL_FATAL(in);
	CU_ASSERT_PTR_NOT_NULL_FATAL(out);

	/*  Read data in 1kb chunks and write to output file */
	while ((read = fread(buf, 1, 1024, in)) == 1024) {
		written = fwrite(buf, 1, 1024, out);
		CU_ASSERT_EQUAL_FATAL(written, read);
	}

	/*  If there is any data left over write it out */
	fwrite(buf, 1, read, out);

	fclose(out);
	fclose(in);

	return 0;
}


static void test_mp4_recovery_tables_header_read_file(void)
{
	int res = 0;
	char tables_file[200];
	struct mp4_recovery_tables_header header = {};

	GET_PATH(tables_file, 0, assets_tests_mp4_recovery, tables_file);

	/* tables file is null */
	res = mp4_recovery_tables_header_read_file(NULL, &header);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* header is null */
	res = mp4_recovery_tables_header_read_file(tables_file, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* tables path is invalid */
	res = mp4_recovery_tables_header_read_file("invalid_tables_file_path",
						   &header);
	CU_ASSERT_EQUAL(res, -ENOENT);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(assets_tests_mp4_recovery);
	     i++) {
		GET_PATH(
			tables_file, i, assets_tests_mp4_recovery, tables_file);

		res = mp4_recovery_tables_header_read_file(tables_file,
							   &header);
		if (!assets_tests_mp4_recovery[i].recovery_version_supported) {
			CU_ASSERT_EQUAL(res, -EPROTO);
			continue;
		}
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(header.magic,
				assets_tests_mp4_recovery[i].header.magic);
		CU_ASSERT_EQUAL(
			header.tables_size,
			assets_tests_mp4_recovery[i].header.tables_size);
		CU_ASSERT_EQUAL(
			header.mux_tables_size,
			assets_tests_mp4_recovery[i].header.mux_tables_size);
		CU_ASSERT_EQUAL(header.version,
				assets_tests_mp4_recovery[i].header.version);
		CU_ASSERT_STRING_EQUAL(
			header.data_path,
			assets_tests_mp4_recovery[i].header.data_path);
		CU_ASSERT_EQUAL(
			header.data_path_length,
			assets_tests_mp4_recovery[i].header.data_path_length);
		CU_ASSERT_STRING_EQUAL(
			header.uuid, assets_tests_mp4_recovery[i].header.uuid);
		CU_ASSERT_EQUAL(
			header.uuid_length,
			assets_tests_mp4_recovery[i].header.uuid_length);

		res = mp4_recovery_tables_header_clear(&header);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_PTR_NULL(header.uuid);
		CU_ASSERT_PTR_NULL(header.data_path);
	}
}


static void test_mp4_recovery_recover_file_from_paths(void)
{
	int res = 0;
	char tables_file[200];
	char data_file[200];
	char *error_msg = NULL;
	char *recovered_file;
	struct mp4_demux *demux = NULL;

	GET_PATH(tables_file, 0, assets_tests_mp4_recovery, tables_file);
	GET_PATH(data_file, 0, assets_tests_mp4_recovery, data_file);

	/* tables file is null */
	res = mp4_recovery_recover_file_from_paths(
		NULL, data_file, &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* tables path is invalid */
	res = mp4_recovery_recover_file_from_paths("invalid_tables_file_path",
						   data_file,
						   &error_msg,
						   &recovered_file);
	CU_ASSERT_EQUAL(res, -ENOENT);
	free(error_msg);
	error_msg = NULL;

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(assets_tests_mp4_recovery);
	     i++) {
		if (!assets_tests_mp4_recovery[i].recovery_version_supported)
			continue;

		GET_PATH(
			tables_file, i, assets_tests_mp4_recovery, tables_file);
		GET_PATH(data_file, i, assets_tests_mp4_recovery, data_file);

		(void)copy_file(tables_file,
				assets_tests_mp4_recovery[i].copied_tables);
		(void)copy_file(data_file,
				assets_tests_mp4_recovery[i].copied_data);

		/* data file is null */
		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_tables,
			NULL,
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -EINVAL);

		/* data path is invalid */
		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_tables,
			"invalid_data_file_path",
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -ENOENT);
		free(error_msg);
		error_msg = NULL;

		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_tables,
			assets_tests_mp4_recovery[i].copied_data,
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, 0);
		free(error_msg);
		error_msg = NULL;

		res = mp4_demux_open(recovered_file, &demux);
		CU_ASSERT_EQUAL(res, 0);
		res = mp4_demux_close(demux);
		CU_ASSERT_EQUAL(res, 0);
		free(recovered_file);

		remove(assets_tests_mp4_recovery[i].copied_tables);
		remove(assets_tests_mp4_recovery[i].copied_data);
	}
}


CU_TestInfo g_mp4_test_recovery[] = {
	{FN("mp4-recovery-tables-header-read-file"),
	 &test_mp4_recovery_tables_header_read_file},
	{FN("mp4-recovery-recover-file-from-paths"),
	 &test_mp4_recovery_recover_file_from_paths},
	CU_TEST_INFO_NULL,
};
