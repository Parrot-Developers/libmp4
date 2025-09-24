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
#include <sys/stat.h>
#include <unistd.h>

#define FTYP_SIZE (32 / 4)
#define INITIAL_FREE_SIZE (8)
#define INITIAL_MDAT_SIZE (MP4_MUX_DEFAULT_TABLE_SIZE_MB * 1024 * 1024)
#define INITIAL_SIZE (FTYP_SIZE + INITIAL_FREE_SIZE + INITIAL_MDAT_SIZE)

#define VMETA_REC_META_KEY_MAKER "com.apple.quicktime.make"
#define VMETA_REC_META_KEY_MAKER_VALUE "Parrot"
#define VMETA_REC_META_KEY_MAKER_VALUE2 "Parrot2"
#define VMETA_REC_UDTA_KEY_FRIENDLY_NAME "\251ART"
#define VMETA_REC_UDTA_KEY_FRIENDLY_NAME_VALUE "friendly_name"
#define MP4_UDTA_KEY_LOCATION "\251xyz"
#define MP4_UDTA_KEY_LOCATION_VALUE "test_location"
#define VMETA_FRAME_PROTO_EMPTY_COOKIE (UINT64_C(0x5F4E4F4D4554415F))


static struct mp4_mux_config s_valid_config_recovery = {
	.filename = TEST_FILE_PATH,
	.filemode = 0644,
	.timescale = 90000,
	.creation_time = 1000,
	.modification_time = 1000,
	.tables_size_mbytes = MP4_MUX_DEFAULT_TABLE_SIZE_MB,
	.recovery.link_file = TEST_FILE_PATH_CHK,
	.recovery.tables_file = TEST_FILE_PATH_MRF,
	.recovery.check_storage_uuid = 0,
};


static struct mp4_mux_track_params s_params_track_video = {
	.type = MP4_TRACK_TYPE_VIDEO,
	.name = "video track",
	.enabled = true,
	.in_movie = true,
	.in_preview = true,
	.timescale = 90000,
	.creation_time = 0,
	.modification_time = 0,
};


static struct mp4_mux_track_params s_params_track_audio = {
	.type = MP4_TRACK_TYPE_AUDIO,
	.name = "audio track",
	.enabled = true,
	.in_movie = true,
	.in_preview = true,
	.timescale = 90000,
	.creation_time = 0,
	.modification_time = 0,
};


static void remove_tmp_files(void)
{
	int res = 0;

	res = remove(TEST_FILE_PATH);
	CU_ASSERT_EQUAL(res, 0);
	res = remove(TEST_FILE_PATH_MRF);
	CU_ASSERT_EQUAL(res, 0);
	res = remove(TEST_FILE_PATH_CHK);
	CU_ASSERT_EQUAL(res, 0);
}


static void test_mp4_mux_api_set_file_cover(void)
{
	int res = 0;
	struct mp4_mux *mux;
	enum mp4_metadata_cover_type cover_type = MP4_METADATA_COVER_TYPE_JPEG;
	size_t cover_size = 110;
	uint8_t *cover = calloc(cover_size, 1);

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	res = mp4_mux_set_file_cover(NULL, cover_type, cover, cover_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_set_file_cover(mux, cover_type, NULL, cover_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = MP4_METADATA_COVER_TYPE_UNKNOWN;
	     i <= MP4_METADATA_COVER_TYPE_BMP;
	     i++) {
		res = mp4_mux_set_file_cover(mux, i, cover, cover_size);
		CU_ASSERT_EQUAL(res,
				(i == MP4_METADATA_COVER_TYPE_UNKNOWN) ? -EINVAL
								       : 0);
	}

	res = mp4_mux_set_file_cover(mux, cover_type, cover, cover_size);
	CU_ASSERT_EQUAL(res, 0);

	(void)mp4_mux_close(mux);

	remove_tmp_files();
	free(cover);
}


static void test_mp4_mux_api_add_scattered_sample(void)
{
	int res = 0;
	struct mp4_mux *mux;
	struct stat st_file;
	const uint64_t empty_cookie = VMETA_FRAME_PROTO_EMPTY_COOKIE;
	const uint8_t *buffer = (const uint8_t *)&empty_cookie;
	size_t len = sizeof(empty_cookie);
	struct mp4_mux_scattered_sample empty_meta_sample = {
		.buffers = &buffer,
		.len = &len,
		.nbuffers = 1,
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_scattered_sample null_buffer_meta_sample = {
		.buffers = NULL,
		.len = &len,
		.nbuffers = 1,
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_scattered_sample null_len_meta_sample = {
		.buffers = &buffer,
		.len = NULL,
		.nbuffers = 1,
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_scattered_sample zero_buffer_meta_sample = {
		.buffers = &buffer,
		.len = &len,
		.nbuffers = 0,
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_scattered_sample empty_meta_sample_dts_10 = {
		.buffers = &buffer,
		.len = &len,
		.nbuffers = 1,
		.sync = 1,
		.dts = 10,
	};
	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	(void)mp4_mux_add_track(mux, &s_params_track_video);

	/* all null */
	res = mp4_mux_track_add_scattered_sample(NULL, 1, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* mux is null */
	res = mp4_mux_track_add_scattered_sample(NULL, 1, &empty_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* sample is null */
	res = mp4_mux_track_add_scattered_sample(mux, 1, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* buffer is null */
	res = mp4_mux_track_add_scattered_sample(
		mux, 1, &null_buffer_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* len is null */
	res = mp4_mux_track_add_scattered_sample(mux, 1, &null_len_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* no buffer */
	res = mp4_mux_track_add_scattered_sample(
		mux, 1, &zero_buffer_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* valid */
	res = mp4_mux_track_add_scattered_sample(mux, 1, &empty_meta_sample);
	CU_ASSERT_EQUAL(res, 0);
	/* size should grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE + sizeof(empty_cookie));

	/* valid */
	res = mp4_mux_track_add_scattered_sample(
		mux, 1, &empty_meta_sample_dts_10);
	CU_ASSERT_EQUAL(res, 0);
	/* size should grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size,
			INITIAL_SIZE + sizeof(empty_cookie) +
				sizeof(empty_cookie));

	/* invalid dts smaller than last one */
	res = mp4_mux_track_add_scattered_sample(mux, 1, &empty_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size,
			INITIAL_SIZE + sizeof(empty_cookie) +
				sizeof(empty_cookie));

	(void)mp4_mux_close(mux);

	remove_tmp_files();
}


static void test_mp4_mux_api_add_sample(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;
	const uint64_t empty_cookie = VMETA_FRAME_PROTO_EMPTY_COOKIE;
	struct mp4_mux_sample empty_meta_sample = {
		.buffer = (uint8_t *)(&empty_cookie),
		.len = sizeof(empty_cookie),
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_sample null_buffer_meta_sample = {
		.buffer = NULL,
		.len = sizeof(empty_cookie),
		.sync = 1,
		.dts = 0,
	};
	struct mp4_mux_sample zero_len_meta_sample = {
		.buffer = (uint8_t *)(&empty_cookie),
		.len = 0,
		.sync = 1,
		.dts = 0,
	};

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	(void)mp4_mux_add_track(mux, &s_params_track_video);

	/* all null */
	res = mp4_mux_track_add_sample(NULL, 1, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* mux is null */
	res = mp4_mux_track_add_sample(NULL, 1, &empty_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* sample is null */
	res = mp4_mux_track_add_sample(mux, 1, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* buffer is null */
	res = mp4_mux_track_add_sample(mux, 1, &null_buffer_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* len is zero */
	res = mp4_mux_track_add_sample(mux, 1, &zero_len_meta_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);
	/* size should not grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	/* valid */
	res = mp4_mux_track_add_sample(mux, 1, &empty_meta_sample);
	CU_ASSERT_EQUAL(res, 0);

	/* size should grow */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE + sizeof(empty_cookie));

	(void)mp4_mux_close(mux);

	remove_tmp_files();
}


static void test_mp4_mux_api_add_track_metadata(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	(void)mp4_mux_add_track(mux, &s_params_track_video);

	/* all null */
	res = mp4_mux_add_track_metadata(NULL, 1, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* mux is null */
	res = mp4_mux_add_track_metadata(NULL, 1, "key", "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* key is null */
	res = mp4_mux_add_track_metadata(mux, 1, NULL, "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* value is null */
	res = mp4_mux_add_track_metadata(mux, 1, "key", NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid meta (strlen("key") < 4) */
	res = mp4_mux_add_track_metadata(mux, 1, "key", "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid track handle */
	res = mp4_mux_add_track_metadata(mux,
					 2,
					 VMETA_REC_META_KEY_MAKER,
					 VMETA_REC_META_KEY_MAKER_VALUE);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* valid */
	res = mp4_mux_add_track_metadata(mux,
					 1,
					 VMETA_REC_META_KEY_MAKER,
					 VMETA_REC_META_KEY_MAKER_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	/* valid (test same key) */
	res = mp4_mux_add_track_metadata(mux,
					 1,
					 VMETA_REC_META_KEY_MAKER,
					 VMETA_REC_META_KEY_MAKER_VALUE2);
	CU_ASSERT_EQUAL(res, 0);

	/* valid (udta) */
	res = mp4_mux_add_track_metadata(
		mux,
		1,
		VMETA_REC_UDTA_KEY_FRIENDLY_NAME,
		VMETA_REC_UDTA_KEY_FRIENDLY_NAME_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	/* valid (udta in moov/udta instead of moov/udta/meta) */
	res = mp4_mux_add_track_metadata(
		mux, 1, MP4_UDTA_KEY_LOCATION, MP4_UDTA_KEY_LOCATION_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	(void)mp4_mux_close(mux);

	/* size should not change */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	remove_tmp_files();
}


static void test_mp4_mux_api_add_file_metadata(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	/* all null */
	res = mp4_mux_add_file_metadata(NULL, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* mux is null */
	res = mp4_mux_add_file_metadata(NULL, "key", "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* key is null */
	res = mp4_mux_add_file_metadata(mux, NULL, "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* value is null */
	res = mp4_mux_add_file_metadata(mux, "key", NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid meta (strlen("key") < 4) */
	res = mp4_mux_add_file_metadata(mux, "key", "value");
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* valid */
	res = mp4_mux_add_file_metadata(
		mux, VMETA_REC_META_KEY_MAKER, VMETA_REC_META_KEY_MAKER_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	/* valid */
	res = mp4_mux_add_file_metadata(
		mux, VMETA_REC_META_KEY_MAKER, VMETA_REC_META_KEY_MAKER_VALUE2);
	CU_ASSERT_EQUAL(res, 0);

	/* valid */
	res = mp4_mux_add_file_metadata(mux,
					VMETA_REC_UDTA_KEY_FRIENDLY_NAME,
					VMETA_REC_UDTA_KEY_FRIENDLY_NAME_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	/* valid */
	res = mp4_mux_add_file_metadata(
		mux, MP4_UDTA_KEY_LOCATION, MP4_UDTA_KEY_LOCATION_VALUE);
	CU_ASSERT_EQUAL(res, 0);

	(void)mp4_mux_close(mux);

	/* size should not change */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	remove_tmp_files();
}


static void test_mp4_mux_api_add_ref_to_track(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;
	struct mp4_mux_track_params params_track_metadata =
		s_params_track_video;
	params_track_metadata.type = MP4_TRACK_TYPE_METADATA;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	(void)mp4_mux_add_track(mux, &s_params_track_video);

	(void)mp4_mux_add_track(mux, &params_track_metadata);

	/* mux is null */
	res = mp4_mux_add_ref_to_track(NULL, 0, 1);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid track handles */
	res = mp4_mux_add_ref_to_track(mux, 0, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid track handles */
	res = mp4_mux_add_ref_to_track(mux, 1, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* invalid track handles */
	res = mp4_mux_add_ref_to_track(mux, 525, 2);
	CU_ASSERT_EQUAL(res, -ENOENT);

	res = mp4_mux_add_ref_to_track(mux, 1, 2);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_mux_add_ref_to_track(mux, 1, 2);
	CU_ASSERT_EQUAL(res, 0);

	for (size_t i = 0; i < MP4_TRACK_REF_MAX - 1; i++) {
		res = mp4_mux_add_ref_to_track(mux, 1, i + 3);
		CU_ASSERT_EQUAL(res, 0);
	}

	res = mp4_mux_add_ref_to_track(mux, 1, MP4_TRACK_REF_MAX);
	CU_ASSERT_EQUAL(res, -ENOBUFS);

	(void)mp4_mux_close(mux);

	/* size should not change */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	remove_tmp_files();
}


static void test_mp4_mux_track_set_audio_specific_config(void)
{
	int res = 0;
	struct mp4_mux *mux;
	size_t asc_size = 5;
	uint8_t *asc = calloc(asc_size, 1);
	uint32_t channel_count = 3;
	uint32_t sample_size = 5;
	float sample_rate = 1.;
	int handle_audio_track;
	int handle_video_track;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	handle_audio_track = mp4_mux_add_track(mux, &s_params_track_audio);

	handle_video_track = mp4_mux_add_track(mux, &s_params_track_video);

	res = mp4_mux_track_set_audio_specific_config(NULL,
						      1,
						      asc,
						      asc_size,
						      channel_count,
						      sample_size,
						      sample_rate);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_audio_specific_config(mux,
						      handle_audio_track,
						      NULL,
						      asc_size,
						      channel_count,
						      sample_size,
						      sample_rate);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_audio_specific_config(mux,
						      handle_video_track,
						      asc,
						      asc_size,
						      channel_count,
						      sample_size,
						      sample_rate);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_audio_specific_config(mux,
						      35,
						      asc,
						      asc_size,
						      channel_count,
						      sample_size,
						      sample_rate);
	CU_ASSERT_EQUAL(res, -ENOENT);

	res = mp4_mux_track_set_audio_specific_config(mux,
						      handle_audio_track,
						      asc,
						      asc_size,
						      channel_count,
						      sample_size,
						      sample_rate);
	CU_ASSERT_EQUAL(res, 0);

	(void)mp4_mux_close(mux);

	remove_tmp_files();
	free(asc);
}


static void test_mp4_mux_track_set_video_decoder_config(void)
{
	int res = 0;
	struct mp4_mux *mux;
	struct mp4_mux_track_params params_video_track_copy =
		s_params_track_video;
	params_video_track_copy.name = "track video 2";
	size_t sps_size = 5;
	uint8_t *sps = calloc(sps_size, 1);
	size_t pps_size = 5;
	uint8_t *pps = calloc(pps_size, 1);
	size_t vps_size = 5;
	uint8_t *vps = calloc(vps_size, 1);
	struct mp4_video_decoder_config vdc_avc = {
		.codec = MP4_VIDEO_CODEC_AVC,
		.width = 1280,
		.height = 720,
		.avc.sps_size = sps_size,
		.avc.sps = sps,
		.avc.pps_size = pps_size,
		.avc.pps = pps,
	};
	struct mp4_video_decoder_config vdc_hevc = {
		.codec = MP4_VIDEO_CODEC_HEVC,
		.width = 1280,
		.height = 720,
		.hevc.sps_size = sps_size,
		.hevc.sps = sps,
		.hevc.vps_size = vps_size,
		.hevc.vps = vps,
		.hevc.pps_size = pps_size,
		.hevc.pps = pps,
	};
	int handle_video_track1;
	int handle_video_track2;
	int handle_audio_track;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	handle_video_track2 = mp4_mux_add_track(mux, &s_params_track_video);

	handle_video_track1 = mp4_mux_add_track(mux, &params_video_track_copy);

	handle_audio_track = mp4_mux_add_track(mux, &s_params_track_audio);

	res = mp4_mux_track_set_video_decoder_config(
		NULL, handle_video_track1, &vdc_avc);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_video_decoder_config(
		NULL, handle_video_track1, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_video_decoder_config(
		mux, handle_audio_track, &vdc_hevc);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_track_set_video_decoder_config(mux, 36, &vdc_avc);
	CU_ASSERT_EQUAL(res, -ENOENT);

	res = mp4_mux_track_set_video_decoder_config(
		mux, handle_video_track1, &vdc_avc);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_mux_track_set_video_decoder_config(
		mux, handle_video_track2, &vdc_hevc);
	CU_ASSERT_EQUAL(res, 0);

	(void)mp4_mux_close(mux);

	remove_tmp_files();
	free(sps);
	free(pps);
	free(vps);
}


static void test_mp4_mux_api_add_track(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;
	struct mp4_mux_track_params params_video_track_copy =
		s_params_track_video;
	int track_count = 0;

	(void)mp4_mux_open(&s_valid_config_recovery, &mux);

	/* mux is null */
	res = mp4_mux_add_track(NULL, &params_video_track_copy);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* params is null */
	res = mp4_mux_add_track(mux, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = MP4_TRACK_TYPE_UNKNOWN; i <= MP4_TRACK_TYPE_UNKNOWN;
	     i++) {
		params_video_track_copy.type = i;
		res = mp4_mux_add_track(mux, &params_video_track_copy);

		switch (i) {
		case MP4_TRACK_TYPE_UNKNOWN:
		case MP4_TRACK_TYPE_HINT:
		case MP4_TRACK_TYPE_TEXT:
			CU_ASSERT_EQUAL(res, -EINVAL);
			break;
		case MP4_TRACK_TYPE_VIDEO:
		case MP4_TRACK_TYPE_AUDIO:
		case MP4_TRACK_TYPE_METADATA:
		case MP4_TRACK_TYPE_CHAPTERS:
			CU_ASSERT_EQUAL(res, ++track_count);
			break;
		}
	}

	(void)mp4_mux_close(mux);

	/* size should not change */
	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	remove_tmp_files();
}


static void test_mp4_mux_api_open_close(void)
{
	int res = 0;
	struct stat st_file;
	struct mp4_mux *mux;
	struct mp4_mux_config valid_config = s_valid_config_recovery;
	valid_config.recovery.link_file = NULL;
	valid_config.recovery.tables_file = NULL;
	struct mp4_mux_config invalid_config1 = s_valid_config_recovery;
	invalid_config1.filename = NULL;
	struct mp4_mux_config invalid_config2 = s_valid_config_recovery;
	invalid_config2.filename = "";
	struct mp4_mux_config invalid_config3 = s_valid_config_recovery;
	invalid_config3.tables_size_mbytes = 0;
	struct mp4_mux_config invalid_config4 = s_valid_config_recovery;
	invalid_config4.recovery.tables_file = NULL;
	struct mp4_mux_config invalid_config5 = s_valid_config_recovery;
	invalid_config5.recovery.link_file = NULL;

	/* both null */
	res = mp4_mux_open(NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* config null */
	res = mp4_mux_open(NULL, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* mux null */
	res = mp4_mux_open(&valid_config, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_close(NULL);
	CU_ASSERT_EQUAL(res, 0);

	/* invalid config */
	res = mp4_mux_open(&invalid_config1, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = mp4_mux_open(&invalid_config2, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = mp4_mux_open(&invalid_config3, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = mp4_mux_open(&invalid_config4, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);
	res = mp4_mux_open(&invalid_config5, &mux);
	CU_ASSERT_EQUAL(res, -EINVAL);

	res = mp4_mux_open(&valid_config, &mux);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH, F_OK), 0);

	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	res = mp4_mux_close(mux);
	CU_ASSERT_EQUAL(res, 0);

	stat(TEST_FILE_PATH, &st_file);
	CU_ASSERT_EQUAL(st_file.st_size, INITIAL_SIZE);

	res = remove(TEST_FILE_PATH);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_mux_open(&s_valid_config_recovery, &mux);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH, F_OK), 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH_MRF, F_OK), 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH_CHK, F_OK), 0);

	res = mp4_mux_close(mux);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH_MRF, F_OK), 0);
	CU_ASSERT_EQUAL(access(TEST_FILE_PATH_CHK, F_OK), 0);

	remove_tmp_files();
}


CU_TestInfo g_mp4_test_mux[] = {
	{FN("mp4-mux-api-open-close"), &test_mp4_mux_api_open_close},
	{FN("mp4-mux-api-add-track"), &test_mp4_mux_api_add_track},
	{FN("mp4-mux-api-set-video-decoder-config"),
	 &test_mp4_mux_track_set_video_decoder_config},
	{FN("mp4-mux-api-set-audio-specific-config"),
	 &test_mp4_mux_track_set_audio_specific_config},
	{FN("mp4-mux-api-add-ref-to-track"),
	 &test_mp4_mux_api_add_ref_to_track},
	{FN("mp4-mux-api-add-file-metadata"),
	 &test_mp4_mux_api_add_file_metadata},
	{FN("mp4-mux-api-add-track-metadata"),
	 &test_mp4_mux_api_add_track_metadata},
	{FN("mp4-mux-api-add-sample"), &test_mp4_mux_api_add_sample},
	{FN("mp4-mux-api-add-scattered-sample"),
	 &test_mp4_mux_api_add_scattered_sample},
	{FN("mp4-mux-api-set-file-cover"), &test_mp4_mux_api_set_file_cover},

	CU_TEST_INFO_NULL,
};
