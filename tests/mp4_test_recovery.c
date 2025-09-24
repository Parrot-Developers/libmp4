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
	const char *link_file;
	const char *tables_file;
	const char *broken_file;
	const char *copied_file;
	const char *copied_link;
	const char *given_data;
	const char *given_mrf;
	const char *uuid;
	size_t tables_size_b;
	uint32_t recovery_version;
} assets_tests_mp4_recovery[] = {
	{
		.link_file = "Tests/recovery/0000003_video.CHK",
		.tables_file = "Tests/recovery/0000003_video.MRF",
		.broken_file = "Tests/recovery/0000003_video.TMP",
		.copied_file = "/tmp/0000003_videoTMP.CPY",
		.copied_link = "/tmp/0000003_videoCHK.CPY",
		.given_data =
			"/tmp/regis-video/user/DCIM//Flights/2025.01.21 09h28/"
			"0000003_video.TMP",
		.given_mrf = "/tmp/regis-video/recovery/0000003_video.MRF",
		.uuid = NULL,
		.tables_size_b = 5242880,
		.recovery_version = 2,
	},
};

static const struct {
	const char *link_file;
	const char *copied_link_file;
	const char *broken_file;
	const char *copied_broken_file;
	const char *tables_file;
	const char *copied_tables_file;
	const char *error_msg;
	int expected_result;
} invalid_link_files[] = {
	{
		.link_file = "Tests/recovery/invalid.CHK",
		.copied_link_file = "/tmp/invalid.CHK",
		.broken_file = NULL,
		.copied_broken_file = NULL,
		.tables_file = NULL,
		.copied_tables_file = NULL,
		.expected_result = -ENOENT,
	},
	{
		.link_file = "Tests/recovery/invalid_2.CHK",
		.copied_link_file = "/tmp/invalid_2.CHK",
		.broken_file = NULL,
		.copied_broken_file = NULL,
		.tables_file = "Tests/recovery/0000003_video.MRF",
		.copied_tables_file = "/tmp/0000003_video.MRF",
		.expected_result = -ENOENT,
	},
	{
		.link_file = "Tests/recovery/invalid_3.CHK",
		.copied_link_file = "/tmp/invalid_3.CHK",
		.broken_file = "Tests/recovery/0000003_video.TMP",
		.copied_broken_file = "/tmp/0000003_video.TMP",
		.tables_file = NULL,
		.copied_tables_file = NULL,
		.expected_result = -ENOENT,
	},
	{
		.link_file = "Tests/recovery/invalid_4.CHK",
		.copied_link_file = "/tmp/invalid_4.CHK",
		.broken_file = "Tests/recovery/0000003_video.TMP",
		.copied_broken_file = "/tmp/0000003_video.TMP",
		.tables_file = "Tests/recovery/0000003_video.MRF",
		.copied_tables_file = "/tmp/0000003_video.MRF",
		.expected_result = -EAGAIN,
	},
	{
		.link_file = "Tests/recovery/invalid_5.CHK",
		.copied_link_file = "/tmp/invalid_5.CHK",
		.broken_file = NULL,
		.copied_broken_file = NULL,
		.tables_file = NULL,
		.copied_tables_file = NULL,
		.expected_result = -EINVAL,
	},
	{
		.link_file = "Tests/recovery/invalid_6.CHK",
		.copied_link_file = "/tmp/invalid_6.CHK",
		.broken_file = NULL,
		.copied_broken_file = NULL,
		.tables_file = NULL,
		.copied_tables_file = NULL,
		.expected_result = -EINVAL,
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


static void test_mp4_recovery_recover_file(void)
{
	int res = 0;
	char link_file[200];
	char *error_msg = NULL;
	char *recovered_file;
	char data_file[200];
	char tables_file[200];

	res = mp4_recovery_recover_file(NULL, &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_recovery_recover_file(
		"invalid_link_file_path", &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -ENOENT);
	free(error_msg);
	error_msg = NULL;

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(invalid_link_files); i++) {
		GET_PATH(link_file, i, invalid_link_files, link_file);

		(void)copy_file(link_file,
				invalid_link_files[i].copied_link_file);

		if (invalid_link_files[i].broken_file &&
		    invalid_link_files[i].copied_broken_file) {
			GET_PATH(data_file, i, invalid_link_files, broken_file);

			(void)copy_file(
				data_file,
				invalid_link_files[i].copied_broken_file);
		}

		if (invalid_link_files[i].tables_file &&
		    invalid_link_files[i].copied_tables_file) {
			GET_PATH(tables_file,
				 i,
				 invalid_link_files,
				 tables_file);
			(void)copy_file(
				tables_file,
				invalid_link_files[i].copied_tables_file);
		}

		/* error_msg is null */
		res = mp4_recovery_recover_file(
			invalid_link_files[i].copied_link_file,
			NULL,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -EINVAL);

		res = mp4_recovery_recover_file(
			invalid_link_files[i].copied_link_file,
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, invalid_link_files[i].expected_result);
		free(error_msg);
		error_msg = NULL;

		if (invalid_link_files[i].tables_file &&
		    invalid_link_files[i].copied_tables_file)
			remove(invalid_link_files[i].copied_tables_file);

		if (invalid_link_files[i].broken_file &&
		    invalid_link_files[i].copied_broken_file)
			remove(invalid_link_files[i].copied_broken_file);

		remove(invalid_link_files[i].copied_link_file);
	}
}


static void test_mp4_recovery_recover_file_from_paths(void)
{
	int res = 0;
	char link_file[200];
	char tables_file[200];
	char data_file[200];
	char *error_msg;
	char *recovered_file;
	struct mp4_demux *demux;

	GET_PATH(link_file, 0, assets_tests_mp4_recovery, link_file);
	GET_PATH(tables_file, 0, assets_tests_mp4_recovery, tables_file);
	GET_PATH(data_file, 0, assets_tests_mp4_recovery, broken_file);

	/* all null */
	res = mp4_recovery_recover_file_from_paths(
		NULL, NULL, NULL, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* link_file null */
	res = mp4_recovery_recover_file_from_paths(
		NULL, tables_file, data_file, &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* tables_file null */
	res = mp4_recovery_recover_file_from_paths(
		link_file, NULL, data_file, &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* data_file null */
	res = mp4_recovery_recover_file_from_paths(
		link_file, tables_file, NULL, &error_msg, &recovered_file);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(assets_tests_mp4_recovery);
	     i++) {
		GET_PATH(link_file, i, assets_tests_mp4_recovery, link_file);
		GET_PATH(
			tables_file, i, assets_tests_mp4_recovery, tables_file);
		GET_PATH(data_file, i, assets_tests_mp4_recovery, broken_file);

		res = copy_file(data_file,
				assets_tests_mp4_recovery[i].copied_file);
		CU_ASSERT_EQUAL(res, 0);

		res = copy_file(link_file,
				assets_tests_mp4_recovery[i].copied_link);
		CU_ASSERT_EQUAL(res, 0);

		/* invalid link file path */
		res = mp4_recovery_recover_file_from_paths(
			"invalid_link_file_path",
			tables_file,
			assets_tests_mp4_recovery[i].copied_file,
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -ENOENT);
		CU_ASSERT_STRING_EQUAL(error_msg, "failed to parse link file");
		free(error_msg);
		error_msg = NULL;

		/* invalid tables file path */
		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_link,
			"invalid_tables_file_path",
			assets_tests_mp4_recovery[i].copied_file,
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -ENOENT);
		CU_ASSERT_STRING_EQUAL(error_msg, "invalid tables file");
		free(error_msg);
		error_msg = NULL;

		/* invalid data file path */
		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_link,
			tables_file,
			"invalid_data_file_path",
			&error_msg,
			&recovered_file);
		CU_ASSERT_EQUAL(res, -ENOENT);
		CU_ASSERT_STRING_EQUAL(error_msg, "invalid data file");
		free(error_msg);
		error_msg = NULL;

		res = mp4_recovery_recover_file_from_paths(
			assets_tests_mp4_recovery[i].copied_link,
			tables_file,
			assets_tests_mp4_recovery[i].copied_file,
			&error_msg,
			NULL);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_PTR_NULL(error_msg);

		/* check file is valid */
		res = mp4_demux_open(assets_tests_mp4_recovery[i].copied_file,
				     &demux);
		CU_ASSERT_EQUAL(res, 0);

		res = mp4_demux_close(demux);
		CU_ASSERT_EQUAL(res, 0);

		remove(assets_tests_mp4_recovery[i].copied_file);
		remove(assets_tests_mp4_recovery[i].copied_link);
	}
}


static void test_mp4_recovery_parse_link_file(void)
{
	int res = 0;
	struct link_file_info info;
	char link_file[200];

	GET_PATH(link_file, 0, assets_tests_mp4_recovery, link_file);

	/* both null */
	res = mp4_recovery_parse_link_file(NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* path null */
	res = mp4_recovery_parse_link_file(NULL, &info);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* info is null */
	res = mp4_recovery_parse_link_file(link_file, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid link file path */
	res = mp4_recovery_parse_link_file("invalid_filepath", &info);
	CU_ASSERT_EQUAL(res, -ENOENT);

	res = mp4_recovery_link_file_info_destroy(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(assets_tests_mp4_recovery);
	     i++) {
		GET_PATH(link_file, i, assets_tests_mp4_recovery, link_file);

		res = mp4_recovery_parse_link_file(link_file, &info);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_STRING_EQUAL(info.tables_file,
				       assets_tests_mp4_recovery[i].given_mrf);
		CU_ASSERT_STRING_EQUAL(info.data_file,
				       assets_tests_mp4_recovery[i].given_data);
		if (info.uuid == NULL) {
			CU_ASSERT_EQUAL(assets_tests_mp4_recovery[i].uuid,
					NULL);
		} else if (assets_tests_mp4_recovery[i].uuid == NULL) {
			CU_ASSERT_EQUAL(info.uuid, NULL);
		} else {
			CU_ASSERT_STRING_EQUAL(
				info.uuid, assets_tests_mp4_recovery[i].uuid);
		}
		CU_ASSERT_EQUAL(info.tables_size_b,
				assets_tests_mp4_recovery[i].tables_size_b);
		CU_ASSERT_EQUAL(info.recovery_version,
				assets_tests_mp4_recovery[i].recovery_version);

		/* valid */
		res = mp4_recovery_link_file_info_destroy(&info);
		CU_ASSERT_EQUAL(res, 0);
	}
}


CU_TestInfo g_mp4_test_recovery[] = {
	{FN("mp4-recovery-parse-link-file"),
	 &test_mp4_recovery_parse_link_file},
	{FN("mp4-recovery-recover-file-from-paths"),
	 &test_mp4_recovery_recover_file_from_paths},
	{FN("mp4-recovery-recover-file"), &test_mp4_recovery_recover_file},

	CU_TEST_INFO_NULL,
};
