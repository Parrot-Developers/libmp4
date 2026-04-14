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

#define BIG_SAMPLE_COUNT 648000

struct expected_metadata {
	char *key;
	char *value;
};
const uint64_t empty_cookie = VMETA_FRAME_PROTO_EMPTY_COOKIE;


static const struct mp4_mux_sample empty_sample = {
	.buffer = (uint8_t *)(&empty_cookie),
	.len = sizeof(empty_cookie),
	.sync = 1,
	.dts = 0,
};


struct mp4_mux_sample empty_samples[] = {
	empty_sample,
	empty_sample,
	empty_sample,
};


struct expected_metadata track_metas[] = {
	{.key = "com.parrot.flight.id",
	 .value = "B808C64DC9F16932D67C3B13D3AF74F0"},
	{.key = "com.parrot.takeoff.loc",
	 .value = "+48.86024214+002.60770981+0.00/"},
};


struct expected_metadata metas[] = {
	{.key = "com.parrot.camera.type", .value = "front"},
	{.key = "com.parrot.camera.serial",
	 .value = "wide:PI020739AA3D004450;tele:PI020837AA3E000997"},
};


struct expected_track {
	struct mp4_mux_track_params params;
	struct mp4_mux_sample *samples;
	size_t sample_count;
	struct expected_metadata *metadatas;
	size_t metadata_count;
};


static struct expected_track tracks[] = {
	{
		.params =
			{
				.type = MP4_TRACK_TYPE_VIDEO,
				.name = "track 1",
				.enabled = true,
				.in_movie = true,
				.in_preview = true,
				.timescale = 90000,
				.creation_time = 0,
				.modification_time = 0,
			},
		.samples = empty_samples,
		.sample_count = SIZEOF_ARRAY(empty_samples),
		.metadatas = track_metas,
		.metadata_count = SIZEOF_ARRAY(track_metas),
	},
};

static const struct {
	struct mp4_mux_config config;
	struct expected_track *tracks;
	size_t track_count;
	struct expected_metadata *metadatas;
	size_t metadata_count;
} test_mux_demux_map[] = {
	{
		.config =
			{
				.filename = TEST_FILE_PATH,
				.filemode = 0644,
				.timescale = 90000,
				.creation_time = 1000,
				.modification_time = 1000,
				.tables_size_mbytes =
					MP4_MUX_DEFAULT_TABLE_SIZE_MB,
				.recovery =
					{
						.tables_file =
							TEST_FILE_PATH_MRF,
						.check_storage_uuid = 0,
					},
			},
		.tracks = tracks,
		.track_count = 1,
		.metadatas = metas,
		.metadata_count = 2,
	},
};


static void add_expected_track(struct mp4_mux *mux,
			       const struct expected_track *track)
{
	int res = 0;
	struct mp4_video_decoder_config video_config = {};
	int track_handle = mp4_mux_add_track(mux, &track->params);
	size_t sps_size = 5;
	uint8_t *sps = calloc(sps_size, 1);
	size_t pps_size = 5;
	uint8_t *pps = calloc(pps_size, 1);

	CU_ASSERT_PTR_NOT_NULL_FATAL(sps);
	CU_ASSERT_PTR_NOT_NULL_FATAL(pps);

	switch (track->params.type) {
	case MP4_TRACK_TYPE_VIDEO:
		video_config.codec = MP4_VIDEO_CODEC_AVC;
		video_config.width = 1280;
		video_config.height = 720;
		video_config.avc.sps_size = sps_size;
		video_config.avc.sps = sps;
		video_config.avc.c_sps = sps;
		video_config.avc.pps_size = pps_size;
		video_config.avc.pps = pps;
		video_config.avc.c_pps = pps;
		mp4_mux_track_set_video_decoder_config(
			mux, track_handle, &video_config);
		break;
	default:
		break;
	}

	for (size_t s = 0; s < track->sample_count; s++) {
		res = mp4_mux_track_add_sample(
			mux, track_handle, &track->samples[s]);
		CU_ASSERT_EQUAL(res, 0);
	}

	for (size_t m = 0; m < track->metadata_count; m++) {
		res = mp4_mux_add_track_metadata(mux,
						 track_handle,
						 track->metadatas[m].key,
						 track->metadatas[m].value);
		CU_ASSERT_EQUAL(res, 0);
	}
	free(sps);
	free(pps);
}

static struct mp4_mux **fill_muxer_list(bool close, bool write_tables)
{
	int res = 0;
	struct mp4_mux *mux;
	struct mp4_mux **muxers = NULL;

	if (!close) {
		muxers = calloc(SIZEOF_ARRAY(test_mux_demux_map),
				sizeof(*muxers));
		CU_ASSERT_PTR_NOT_NULL_FATAL(muxers);
	}

	for (size_t i = 0; i < SIZEOF_ARRAY(test_mux_demux_map); i++) {
		res = mp4_mux_open(&test_mux_demux_map[i].config, &mux);
		CU_ASSERT_EQUAL(res, 0);

		for (size_t t = 0; t < test_mux_demux_map[i].track_count; t++) {
			add_expected_track(mux,
					   &test_mux_demux_map[i].tracks[t]);
		}

		for (size_t m = 0; m < test_mux_demux_map[i].metadata_count;
		     m++) {
			res = mp4_mux_add_file_metadata(
				mux,
				test_mux_demux_map[i].metadatas[m].key,
				test_mux_demux_map[i].metadatas[m].value);
			CU_ASSERT_EQUAL(res, 0);
		}

		if (close) {
			res = mp4_mux_close(mux);
			CU_ASSERT_EQUAL(res, 0);
		} else {
			res = mp4_mux_sync(mux, write_tables);
			CU_ASSERT_EQUAL(res, 0);
			muxers[i] = mux;
		}
	}

	return muxers;
}


static void
test_demux_track(const struct mp4_demux *demux, unsigned int i, unsigned int t)
{
	int res = 0;
	struct mp4_track_info track_info;
	unsigned int meta_count = 0;
	char **keys = NULL;
	char **values = NULL;
	const struct expected_track *track = &test_mux_demux_map[i].tracks[t];
	uint8_t sample_buffer[1024];
	struct mp4_track_sample track_sample;

	mp4_demux_get_track_info(demux, t, &track_info);
	CU_ASSERT_EQUAL(track_info.type, track->params.type);
	CU_ASSERT_STRING_EQUAL(track_info.name, track->params.name);
	CU_ASSERT_EQUAL(track_info.enabled, track->params.enabled);
	CU_ASSERT_EQUAL(track_info.in_movie, track->params.in_movie);
	CU_ASSERT_EQUAL(track_info.in_preview, track->params.in_preview);
	CU_ASSERT_EQUAL(track_info.timescale, track->params.timescale);
	CU_ASSERT_EQUAL(track_info.creation_time, track->params.creation_time);
	CU_ASSERT_EQUAL(track_info.modification_time,
			track->params.modification_time);

	CU_ASSERT_EQUAL(track_info.sample_count, track->sample_count);

	for (int sample_index = 0; (size_t)sample_index < track->sample_count;
	     sample_index++) {
		const struct mp4_mux_sample *expected_sample =
			&track->samples[sample_index];

		res = mp4_demux_get_track_sample(demux,
						 track_info.id,
						 1,
						 sample_buffer,
						 sizeof(sample_buffer),
						 NULL,
						 0,
						 &track_sample);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(expected_sample->len, track_sample.size);
		CU_ASSERT_EQUAL(expected_sample->sync, track_sample.sync);
		CU_ASSERT_EQUAL(expected_sample->dts, track_sample.dts);
		CU_ASSERT_EQUAL(memcmp(expected_sample->buffer,
				       sample_buffer,
				       expected_sample->len),
				0);
	}

	res = mp4_demux_get_track_metadata_strings(
		demux, track_info.id, &meta_count, &keys, &values);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT(meta_count > 0);
	CU_ASSERT_EQUAL(meta_count, track->metadata_count);
	for (size_t m = 0; m < meta_count; m++) {
		CU_ASSERT_STRING_EQUAL(keys[m], track->metadatas[m].key);
		CU_ASSERT_STRING_EQUAL(values[m], track->metadatas[m].value);
	}
}


static void test_demux(void)
{
	int res = 0;
	struct mp4_demux *demux;
	int track_count = 0;
	unsigned int meta_count = 0;
	char **keys = NULL;
	char **values = NULL;

	for (size_t i = 0; i < SIZEOF_ARRAY(test_mux_demux_map); i++) {
		res = mp4_demux_open(test_mux_demux_map[i].config.filename,
				     &demux);
		CU_ASSERT_EQUAL(res, 0);

		res = mp4_demux_get_metadata_strings(
			demux, &meta_count, &keys, &values);
		CU_ASSERT_EQUAL(res, 0);

		CU_ASSERT(meta_count > 0);
		CU_ASSERT_EQUAL(meta_count,
				test_mux_demux_map[i].metadata_count);
		for (size_t m = 0; m < meta_count; m++) {
			CU_ASSERT_STRING_EQUAL(
				keys[m],
				test_mux_demux_map[i].metadatas[m].key);
			CU_ASSERT_STRING_EQUAL(
				values[m],
				test_mux_demux_map[i].metadatas[m].value);
		}

		track_count = mp4_demux_get_track_count(demux);
		CU_ASSERT(track_count > 0);
		CU_ASSERT_EQUAL(track_count, test_mux_demux_map[i].track_count);
		for (size_t t = 0; t < (size_t)track_count; t++)
			test_demux_track(demux, i, t);

		res = mp4_demux_close(demux);
		CU_ASSERT_EQUAL(res, 0);

		remove(test_mux_demux_map[i].config.filename);
	}
}


static void test_recovery(void)
{
	char *error_msg;
	for (size_t i = 0; i < SIZEOF_ARRAY(test_mux_demux_map); i++) {
		mp4_recovery_recover_file_from_paths(
			test_mux_demux_map[i].config.recovery.tables_file,
			test_mux_demux_map[i].config.filename,
			&error_msg,
			NULL);
		mp4_recovery_finalize(
			test_mux_demux_map[i].config.recovery.tables_file,
			false,
			NULL);
	}
}


static void test_mp4_mux_demux_test(void)
{
	(void)fill_muxer_list(true, false);
	test_demux();
}


static void test_mp4_mux_internal_sync_demux_test(void)
{
	struct mp4_mux **muxers = fill_muxer_list(false, true);
	test_demux();

	/* clean up */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_mux_demux_map); i++)
		mp4_mux_close(muxers[i]);

	free(muxers);
}


static void test_mp4_mux_recovery_test(void)
{
	struct mp4_mux **muxers = fill_muxer_list(false, false);

	test_recovery();
	test_demux();

	/* clean up */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_mux_demux_map); i++)
		mp4_mux_close(muxers[i]);

	free(muxers);
}


static void test_mp4_mux_demux_big_file(void)
{
	int res = 0;
	struct mp4_demux *demux;
	struct mp4_mux *mux;

	struct mp4_mux_config config = {
		.filename = "/tmp/test_mux_demux.MP4",
		.filemode = 0644,
		.timescale = 90000,
		.creation_time = 1000,
		.modification_time = 1000,
		.tables_size_mbytes = MP4_MUX_DEFAULT_TABLE_SIZE_MB,
		.recovery =
			{
				.tables_file = TEST_FILE_PATH_MRF,
				.check_storage_uuid = 0,
			},
	};

	res = mp4_mux_open(&config, &mux);
	CU_ASSERT_EQUAL(res, 0);

	for (int t = 0; (size_t)t < test_mux_demux_map[0].track_count; t++) {
		add_expected_track(mux, &test_mux_demux_map[0].tracks[t]);
		for (size_t s = 0; s < BIG_SAMPLE_COUNT; s++) {
			res = mp4_mux_track_add_sample(
				mux, t + 1, &empty_samples[0]);
			CU_ASSERT_EQUAL(res, 0);
		}
	}

	res = mp4_mux_close(mux);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_demux_open(config.filename, &demux);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_demux_close(demux);
	CU_ASSERT_EQUAL(res, 0);

	res = mp4_recovery_finalize(config.recovery.tables_file, false, NULL);
	CU_ASSERT_EQUAL(res, 0);

	remove(config.filename);
}


CU_TestInfo g_mp4_test_mux_demux[] = {
	{FN("mp4-mux-test-mux-demux"), &test_mp4_mux_demux_test},
	{FN("mp4-mux-test-mux-internal-sync-demux"),
	 &test_mp4_mux_internal_sync_demux_test},
	{FN("mp4-mux-test-mux-recovery"), &test_mp4_mux_recovery_test},
	{FN("mp4-mux-test-mux-demux-big-file"), &test_mp4_mux_demux_big_file},

	CU_TEST_INFO_NULL,
};
