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

static const char *chapters_240p[] = {
	"Start",	 "Disconnection", "Connection",	   "Takeoff",
	"Disconnection", "Connection",	  "Disconnection", "Connection",
	"Disconnection", "Connection",	  "Disconnection", "Connection",
	"Disconnection", "Connection",	  "Connection",	   "Land",
	"Disconnection", "Connection",	  "Disconnection", "Connection",
	"Takeoff",	 "Land",	  "Disconnection", "Connection",
	"Takeoff",	 "Disconnection", "Connection",	   "Land",
	"Disconnection"};


struct meta_container {
	const char *key;
	const char *value;
};


static const struct meta_container meta_240p[] = {
	{
		.key = "com.apple.quicktime.artist",
		.value = "ANAFI UKR-000586",
	},
	{
		.key = "com.apple.quicktime.title",
		.value = "Mon, 23 Dec 2024 13:13:15 +0100",
	},
	{
		.key = "com.apple.quicktime.creationdate",
		.value = "2024-12-23T13:13:15+01:00",
	},
	{
		.key = "com.apple.quicktime.make",
		.value = "Parrot",
	},
	{
		.key = "com.apple.quicktime.model",
		.value = "ANAFI UKR",
	},
	{
		.key = "com.apple.quicktime.software",
		.value = "8.2.0-alpha9",
	},
	{
		.key = "com.parrot.serial",
		.value = "PI040461AC4I000586",
	},
	{
		.key = "com.parrot.model.id",
		.value = "0920",
	},
	{.key = "com.parrot.build.id", .value = "anafi3-classic-8.2.0-alpha9"},
	{.key = "com.parrot.boot.date", .value = "2024-12-23T12:22:34+01:00"},
	{.key = "com.parrot.boot.id",
	 .value = "BAA3C49F6F2F2C28EA5B1BDFD69351EB"},
	{.key = "com.parrot.camera.type", .value = "front"},
	{.key = "com.parrot.camera.serial",
	 .value = "wide:PI020739AA3D004450;tele:PI020837AA3E000997"},
	{.key = "com.parrot.camera.model.type", .value = "perspective"},
	{.key = "com.parrot.perspective.distortion",
	 .value = "0.00000000,0.00000000,0.00000000,0.00000000,0.00000000"},
	{.key = "com.parrot.video.mode", .value = "streamrec"},
	{.key = "com.parrot.thermal.camserial", .value = "324508"},
	{.key = "com.parrot.first.frame.capture.ts", .value = "107115688"},
	{.key = "com.parrot.flight.id",
	 .value = "B808C64DC9F16932D67C3B13D3AF74F0"},
	{.key = "com.parrot.takeoff.loc",
	 .value = "+48.86024214+002.60770981+0.00/"},
	{.key = "com.apple.quicktime.location.ISO6709",
	 .value = "+48.86024214+002.60770981+0.00/"},
	{.key = "com.parrot.flight.date", .value = "2024-12-23T13:06:30+01:00"},
	{.key = NULL, .value = "ANAFI UKR-000586"},
	{.key = NULL, .value = "Mon, 23 Dec 2024 13:13:15 +0100"},
	{.key = NULL, .value = "2024-12-23T13:13:15+01:00"},
	{.key = NULL, .value = "Parrot"},
	{.key = NULL, .value = "ANAFI UKR"},
	{.key = NULL, .value = "8.2.0-alpha9"},
	{.key = NULL, .value = "PI040461AC4I000586"},
	{.key = NULL, .value = "+48.8602+002.6077/"},
};


static struct {
	const char relative_path[100];
	char absolute_path[200];
	struct {
		struct mp4_media_info media_info;
		unsigned int chapters_count;
		const char **chapter_names;
		const struct meta_container *metas;
		unsigned int meta_count;
		unsigned int meta_to_find_count;
		unsigned int cover_size;
		enum mp4_metadata_cover_type cover_type;
		bool test_all_samples;
	} expected_results;
} nas_tests_mp4_demux[] = {
	{
		"Tests/anafi/4k/video_recording/champs_1080p30.mp4",
		"",
		{
			.media_info.duration = 224917333,
			.media_info.creation_time = 1527952380,
			.media_info.modification_time = 1527952380,
			.media_info.track_count = 3,
			.chapters_count = 0,
			.chapter_names = NULL,
			.metas = NULL,
			.meta_count = 21,
			.meta_to_find_count = 0,
			.cover_size = 13932,
			.cover_type = MP4_METADATA_COVER_TYPE_JPEG,
			.test_all_samples = false,
		},
	},
	{
		"Tests/anafi/4k/video_recording/jardin_2160p30.mp4",
		"",
		{
			.media_info.duration = 123584000,
			.media_info.creation_time = 1538842372,
			.media_info.modification_time = 1538842372,
			.media_info.track_count = 3,
			.chapters_count = 0,
			.chapter_names = NULL,
			.metas = NULL,
			.meta_count = 21,
			.meta_to_find_count = 0,
			.cover_size = 29542,
			.cover_type = MP4_METADATA_COVER_TYPE_JPEG,
			.test_all_samples = false,
		},
	},
	{
		"Tests/anafi3/classic/video_recording/fosses_2160p9_thermal.mp4",
		"",
		{
			.media_info.duration = 170024011,
			.media_info.creation_time = 1701948666,
			.media_info.modification_time = 1701948666,
			.media_info.track_count = 6,
			.chapters_count = 0,
			.chapter_names = NULL,
			.metas = NULL,
			.meta_count = 26,
			.meta_to_find_count = 0,
			.cover_size = 42673,
			.cover_type = MP4_METADATA_COVER_TYPE_JPEG,
			.test_all_samples = true,
		},
	},
	{
		"Tests/anafi3/classic/stream_sharing/240p.MP4",
		"",
		{
			.media_info.duration = 3006915289,
			.media_info.creation_time = 1734953063,
			.media_info.modification_time = 1734953063,
			.media_info.track_count = 3,
			.chapters_count = 29,
			.chapter_names = chapters_240p,
			.metas = meta_240p,
			.meta_count = 30,
			.meta_to_find_count = 30,
			.cover_size = 0,
			.cover_type = MP4_METADATA_COVER_TYPE_UNKNOWN,
			.test_all_samples = false,
		},
	},
};


static char *get_path(size_t index)
{
	if (index >= FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux))
		return NULL;

	if (nas_tests_mp4_demux[index].absolute_path[0] == '\0') {
		snprintf(nas_tests_mp4_demux[index].absolute_path,
			 sizeof(nas_tests_mp4_demux[index].absolute_path),
			 "%s/%s",
			 (getenv("ASSETS_ROOT") != NULL) ? getenv("ASSETS_ROOT")
							 : ASSETS_ROOT,
			 nas_tests_mp4_demux[index].relative_path);
	}
	int file_read_access =
		access(nas_tests_mp4_demux[index].absolute_path, R_OK);
	CU_ASSERT_FATAL(file_read_access == 0);
	return nas_tests_mp4_demux[index].absolute_path;
}


static bool find_meta(const struct meta_container meta,
		      unsigned int count,
		      char **keys,
		      char **values)
{
	for (size_t i = 0; i < count; i++) {
		if (meta.key != NULL && keys[i] != NULL &&
		    strcmp(meta.key, keys[i]) != 0)
			continue;
		if (meta.value != NULL && values[i] != NULL &&
		    strcmp(meta.value, values[i]) != 0)
			continue;
		return true;
	}
	return false;
}


static void test_mp4_demux_get_track_next_sample_time_after(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	uint64_t ts_sample = 0;
	uint64_t time = 0;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_track_next_sample_time_after(
		NULL, 0, time, 0, &ts_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* sample_time is null */
	res = mp4_demux_get_track_next_sample_time_after(
		demux, 0, time, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track id */
		res = mp4_demux_get_track_next_sample_time_after(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			time,
			0,
			&ts_sample);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {

			(void)mp4_demux_get_track_info(demux, j, &track_info);

			/* valid */
			res = mp4_demux_get_track_next_sample_time_after(
				demux, track_info.id, time, 0, &ts_sample);
			CU_ASSERT_EQUAL(res, 0);
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_next_sample_time_before(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	uint64_t ts_sample = 0;
	uint64_t time = 0;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_track_prev_sample_time_before(
		NULL, 0, time, 0, &ts_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* sample_time is null */
	res = mp4_demux_get_track_prev_sample_time_before(
		demux, 0, time, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track id */
		res = mp4_demux_get_track_prev_sample_time_before(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			time,
			0,
			&ts_sample);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {


			(void)mp4_demux_get_track_info(demux, j, &track_info);
			time = track_info.duration;

			/* valid */
			res = mp4_demux_get_track_prev_sample_time_before(
				demux, track_info.id, time, 0, &ts_sample);
			CU_ASSERT_EQUAL(res, 0);
		}
		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_sample_time(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	uint64_t ts_sample_0 = 0;
	uint64_t ts_sample_1 = 0;
	uint64_t ts_sample_2 = 0;
	uint64_t ts_sample_3 = 0;
	uint64_t ts_sample_1_prev = 0;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_track_next_sample_time(NULL, 0, &ts_sample_0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* ts is null */
	res = mp4_demux_get_track_next_sample_time(demux, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_track_prev_sample_time(NULL, 0, &ts_sample_0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* ts is null */
	res = mp4_demux_get_track_prev_sample_time(demux, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			/* need to open / close for each track since seek
			 * affects all tracks */
			(void)mp4_demux_open(path, &demux);

			(void)mp4_demux_get_track_info(demux, j, &track_info);

			if (!nas_tests_mp4_demux[i]
				     .expected_results.test_all_samples) {
				(void)mp4_demux_close(demux);
				continue;
			}

			res = mp4_demux_get_track_prev_sample_time(
				demux, track_info.id, &ts_sample_0);
			CU_ASSERT_EQUAL(res, -ENOENT);

			res = mp4_demux_get_track_next_sample_time(
				demux, track_info.id, &ts_sample_1);
			CU_ASSERT_EQUAL(res, 0);

			(void)mp4_demux_seek_to_track_next_sample(
				demux, track_info.id);

			res = mp4_demux_get_track_next_sample_time(
				demux, track_info.id, &ts_sample_2);
			CU_ASSERT_EQUAL(res, 0);
			CU_ASSERT(ts_sample_1 < ts_sample_2);

			(void)mp4_demux_seek_to_track_next_sample(
				demux, track_info.id);

			res = mp4_demux_get_track_prev_sample_time(
				demux, track_info.id, &ts_sample_1_prev);
			CU_ASSERT_EQUAL(res, 0);
			CU_ASSERT_EQUAL(ts_sample_1, ts_sample_1_prev);

			res = mp4_demux_get_track_next_sample_time(
				demux, track_info.id, &ts_sample_3);
			CU_ASSERT_EQUAL(res, 0);
			CU_ASSERT(ts_sample_2 < ts_sample_3);

			(void)mp4_demux_close(demux);
		}
	}
}


static void test_mp4_demux_seek(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	uint64_t ts = 0;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_seek(NULL, 0, MP4_SEEK_METHOD_PREVIOUS);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_track_next_sample_time(NULL, 0, &ts);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* ts is null */
	res = mp4_demux_get_track_next_sample_time(demux, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			/* need to open / close for each track since seek
			 * affects all tracks */
			(void)mp4_demux_open(path, &demux);

			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_get_track_next_sample_time(
				demux, track_info.id, &ts);
			CU_ASSERT_EQUAL(res, 0);

			res = mp4_demux_seek(
				demux, ts, MP4_SEEK_METHOD_PREVIOUS);
			CU_ASSERT_EQUAL(res, 0);

			if (!nas_tests_mp4_demux[i]
				     .expected_results.test_all_samples) {
				(void)mp4_demux_close(demux);
				continue;
			}

			for (size_t k = 0; k < track_info.sample_count - 1;
			     k++) {
				res = mp4_demux_get_track_next_sample_time(
					demux, track_info.id, &ts);
				CU_ASSERT_EQUAL(res, 0);

				res = mp4_demux_seek(
					demux, ts, MP4_SEEK_METHOD_PREVIOUS);
				CU_ASSERT_EQUAL(res, 0);
			}

			(void)mp4_demux_close(demux);
		}
	}
}


static void test_mp4_demux_seek_to_track_prev_sample(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_seek_to_track_prev_sample(NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* first sample */
	res = mp4_demux_seek_to_track_prev_sample(demux, 0);
	CU_ASSERT_EQUAL(res, -ENOENT);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			/* need to open / close for each track since seek
			 * affects all tracks */
			(void)mp4_demux_open(path, &demux);

			(void)mp4_demux_get_track_info(demux, j, &track_info);

			if (!nas_tests_mp4_demux[i]
				     .expected_results.test_all_samples) {
				(void)mp4_demux_close(demux);
				continue;
			}

			(void)mp4_demux_seek_to_track_next_sample(
				demux, track_info.id);

			res = mp4_demux_seek_to_track_prev_sample(
				demux, track_info.id);
			CU_ASSERT_EQUAL(res, 0);

			(void)mp4_demux_close(demux);
		}
	}
}


static void test_mp4_demux_seek_to_track_next_sample(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;

	/* demux is null */
	res = mp4_demux_seek_to_track_next_sample(NULL, 0);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			/* need to open / close for each track since seek
			 * affects all tracks */
			(void)mp4_demux_open(path, &demux);

			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_seek_to_track_next_sample(
				demux, track_info.id);
			CU_ASSERT_EQUAL(res, 0);

			if (!nas_tests_mp4_demux[i]
				     .expected_results.test_all_samples) {
				(void)mp4_demux_close(demux);
				continue;
			}

			for (size_t k = 0; k < track_info.sample_count - 1;
			     k++) {
				res = mp4_demux_seek_to_track_next_sample(
					demux, track_info.id);
				CU_ASSERT_EQUAL(res, 0);
			}

			(void)mp4_demux_close(demux);
		}
	}
}


static void test_mp4_demux_get_track_sample(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	struct mp4_track_sample track_sample;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_track_sample(
		NULL, 0, 1, NULL, 0, NULL, 0, &track_sample);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* track_sample is null */
	res = mp4_demux_get_track_sample(demux, 0, 1, NULL, 0, NULL, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_get_track_sample(demux,
							 track_info.id,
							 0,
							 NULL,
							 0,
							 NULL,
							 0,
							 &track_sample);
			CU_ASSERT_EQUAL(res, 0);

			if (!nas_tests_mp4_demux[i]
				     .expected_results.test_all_samples)
				continue;

			for (size_t k = 0; k < track_info.sample_count + 1;
			     k++) {
				res = mp4_demux_get_track_sample(demux,
								 track_info.id,
								 1,
								 NULL,
								 0,
								 NULL,
								 0,
								 &track_sample);
				if (k <= track_info.sample_count) {
					CU_ASSERT_EQUAL(res, 0);
				} else {
					CU_ASSERT_EQUAL(res, -ENOENT);
				}
			}
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_metadata_cover(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	uint8_t *cover_buffer = NULL;
	unsigned int cover_buffer_size;
	unsigned int cover_size;
	enum mp4_metadata_cover_type cover_type;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* valid to get size */
	res = mp4_demux_get_metadata_cover(demux, NULL, 0, &cover_size, NULL);
	CU_ASSERT_EQUAL(res, 0);
	cover_buffer = calloc(1, cover_size);
	cover_buffer_size = cover_size;

	/* demux is null */
	res = mp4_demux_get_metadata_cover(NULL,
					   cover_buffer,
					   cover_buffer_size,
					   &cover_size,
					   &cover_type);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* cover size is null */
	res = mp4_demux_get_metadata_cover(
		demux, cover_buffer, cover_buffer_size, NULL, &cover_type);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* cover_buffer_size too small is null */
	res = mp4_demux_get_metadata_cover(
		demux, cover_buffer, 0, &cover_size, &cover_type);
	CU_ASSERT_EQUAL(res, -ENOBUFS);

	(void)mp4_demux_close(demux);

	free(cover_buffer);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* valid to get size */
		res = mp4_demux_get_metadata_cover(
			demux, NULL, 0, &cover_size, NULL);
		CU_ASSERT_EQUAL(res, 0);
		cover_buffer = calloc(1, cover_size);
		cover_buffer_size = cover_size;

		res = mp4_demux_get_metadata_cover(demux,
						   cover_buffer,
						   cover_buffer_size,
						   &cover_size,
						   &cover_type);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(
			cover_size,
			nas_tests_mp4_demux[i].expected_results.cover_size);
		if (nas_tests_mp4_demux[i].expected_results.cover_size > 0) {
			CU_ASSERT_EQUAL(cover_type,
					nas_tests_mp4_demux[i]
						.expected_results.cover_type);
		}

		free(cover_buffer);

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_metadata_strings(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	unsigned int count;
	char **keys;
	char **values;
	bool found_meta;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_metadata_strings(NULL, &count, &keys, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* count is null */
	res = mp4_demux_get_metadata_strings(demux, NULL, &keys, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* keys is null */
	res = mp4_demux_get_metadata_strings(demux, &count, NULL, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* values is null */
	res = mp4_demux_get_metadata_strings(demux, &count, &keys, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		res = mp4_demux_get_metadata_strings(
			demux, &count, &keys, &values);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(
			nas_tests_mp4_demux[i].expected_results.meta_count,
			count);

		for (size_t j = 0;
		     j <
		     nas_tests_mp4_demux[i].expected_results.meta_to_find_count;
		     j++) {
			found_meta =
				find_meta(nas_tests_mp4_demux[i]
						  .expected_results.metas[j],
					  count,
					  keys,
					  values);
			CU_ASSERT_EQUAL(found_meta, true);
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_metadata_strings(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	unsigned int count;
	char **keys;
	char **values;
	struct mp4_track_info track_info;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_track_metadata_strings(
		NULL, 0, &count, &keys, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* count is null */
	res = mp4_demux_get_track_metadata_strings(
		demux, 0, NULL, &keys, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* keys is null */
	res = mp4_demux_get_track_metadata_strings(
		demux, 0, &count, NULL, &values);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* values is null */
	res = mp4_demux_get_track_metadata_strings(
		demux, 0, &count, &keys, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track_id */
		res = mp4_demux_get_track_metadata_strings(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			&count,
			&keys,
			&values);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_get_track_metadata_strings(
				demux, track_info.id, &count, &keys, &values);
			CU_ASSERT_EQUAL(res, 0);
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_chapters(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	unsigned int chapters_count;
	uint64_t *chapters_time;
	char **chapters_name;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* demux is null */
	res = mp4_demux_get_chapters(
		NULL, &chapters_count, &chapters_time, &chapters_name);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* chapters_count is null */
	res = mp4_demux_get_chapters(
		demux, NULL, &chapters_time, &chapters_name);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* chapters_time is null */
	res = mp4_demux_get_chapters(
		demux, &chapters_count, NULL, &chapters_name);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* chapters_name is null */
	res = mp4_demux_get_chapters(
		demux, &chapters_count, &chapters_time, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		res = mp4_demux_get_chapters(
			demux, &chapters_count, &chapters_time, &chapters_name);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_EQUAL(
			chapters_count,
			nas_tests_mp4_demux[i].expected_results.chapters_count);

		for (size_t j = 0;
		     j < chapters_count &&
		     j < nas_tests_mp4_demux[i].expected_results.chapters_count;
		     j++) {
			CU_ASSERT_STRING_EQUAL(
				chapters_name[j],
				nas_tests_mp4_demux[i]
					.expected_results.chapter_names[j]);
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_audio_specific_config(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	uint8_t *audio_specific_config;
	unsigned int asc_size;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* all null */
	res = mp4_demux_get_track_audio_specific_config(NULL, 0, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_track_audio_specific_config(
		NULL, 0, &audio_specific_config, &asc_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track_idx */
		res = mp4_demux_get_track_audio_specific_config(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			&audio_specific_config,
			&asc_size);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_get_track_audio_specific_config(
				demux,
				track_info.id,
				&audio_specific_config,
				&asc_size);
			CU_ASSERT_EQUAL(res,
					(track_info.type == MP4_TRACK_TYPE_AUDIO
						 ? 0
						 : -EINVAL));
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_video_decoder_config(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;
	struct mp4_video_decoder_config vdc;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* both null */
	res = mp4_demux_get_track_video_decoder_config(NULL, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_track_video_decoder_config(NULL, 0, &vdc);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* vdc is null */
	res = mp4_demux_get_track_video_decoder_config(demux, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track_idx */
		res = mp4_demux_get_track_video_decoder_config(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			&vdc);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			(void)mp4_demux_get_track_info(demux, j, &track_info);

			res = mp4_demux_get_track_video_decoder_config(
				demux, track_info.id, &vdc);
			CU_ASSERT_EQUAL(res,
					(track_info.type == MP4_TRACK_TYPE_VIDEO
						 ? 0
						 : -EINVAL));
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_info(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;
	struct mp4_track_info track_info;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* both null */
	res = mp4_demux_get_track_info(NULL, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* both null */
	res = mp4_demux_get_track_info(NULL, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_track_info(NULL, 0, &track_info);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* track_info is null */
	res = mp4_demux_get_track_info(demux, 0, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		/* invalid track_idx */
		res = mp4_demux_get_track_info(
			demux,
			nas_tests_mp4_demux[i]
					.expected_results.media_info
					.track_count +
				1,
			&track_info);
		CU_ASSERT_EQUAL(res, -ENOENT);

		for (size_t j = 0;
		     j < nas_tests_mp4_demux[i]
				 .expected_results.media_info.track_count;
		     j++) {
			res = mp4_demux_get_track_info(demux, j, &track_info);
			CU_ASSERT_EQUAL(res, 0);
		}

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_track_count(void)
{
	int res = 0;
	char *path;
	struct mp4_demux *demux;

	/* demux is null */
	res = mp4_demux_get_track_count(NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		res = mp4_demux_get_track_count(demux);
		CU_ASSERT_EQUAL(
			res,
			nas_tests_mp4_demux[i]
				.expected_results.media_info.track_count);

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_get_media_info(void)
{
	int res = 0;
	struct mp4_media_info media_info;
	char *path;
	struct mp4_demux *demux;

	path = get_path(0);

	(void)mp4_demux_open(path, &demux);

	/* both null */
	res = mp4_demux_get_media_info(NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_get_media_info(NULL, &media_info);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* media_info is null */
	res = mp4_demux_get_media_info(demux, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	(void)mp4_demux_close(demux);


	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		(void)mp4_demux_open(path, &demux);

		res = mp4_demux_get_media_info(demux, &media_info);
		CU_ASSERT_EQUAL(res, 0);

		CU_ASSERT_EQUAL(media_info.duration,
				nas_tests_mp4_demux[i]
					.expected_results.media_info.duration);
		CU_ASSERT_EQUAL(
			media_info.creation_time,
			nas_tests_mp4_demux[i]
				.expected_results.media_info.creation_time);
		CU_ASSERT_EQUAL(
			media_info.modification_time,
			nas_tests_mp4_demux[i]
				.expected_results.media_info.modification_time);
		CU_ASSERT_EQUAL(
			media_info.track_count,
			nas_tests_mp4_demux[i]
				.expected_results.media_info.track_count);

		(void)mp4_demux_close(demux);
	}
}


static void test_mp4_demux_open_close(void)
{
	int res = 0;
	struct mp4_demux *demux;
	char *path;

	path = get_path(0);

	/* both null */
	res = mp4_demux_open(NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* path is null */
	res = mp4_demux_open(NULL, &demux);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* demux is null */
	res = mp4_demux_open(path, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* path is invalid */
	res = mp4_demux_open("", &demux);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* path is invalid */
	res = mp4_demux_open("./", &demux);
	CU_ASSERT_EQUAL(res, -EISDIR);

	/* path is invalid */
	res = mp4_demux_open("./wrong_path", &demux);
	CU_ASSERT_EQUAL(res, -ENOENT);

	/* demux is null */
	res = mp4_demux_close(NULL);
	CU_ASSERT_EQUAL(res, 0);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_demux); i++) {
		path = get_path(i);

		res = mp4_demux_open(path, &demux);
		CU_ASSERT_EQUAL(res, 0);

		res = mp4_demux_close(demux);
		CU_ASSERT_EQUAL(res, 0);
	}
}

CU_TestInfo g_mp4_test_demux[] = {
	{FN("mp4-demux-open-close"), &test_mp4_demux_open_close},
	{FN("mp4-demux-get-media-info"), &test_mp4_demux_get_media_info},
	{FN("mp4-demux-get-track-count"), &test_mp4_demux_get_track_count},
	{FN("mp4-demux-get-track-info"), &test_mp4_demux_get_track_info},
	{FN("mp4-demux-get-track-video-decoder-config"),
	 &test_mp4_demux_get_track_video_decoder_config},
	{FN("mp4-demux-get-track-audio-specific-config"),
	 &test_mp4_demux_get_track_audio_specific_config},
	{FN("mp4-demux-get-chapters"), &test_mp4_demux_get_chapters},
	{FN("mp4-demux-get-metadata-strings"),
	 &test_mp4_demux_get_metadata_strings},
	{FN("mp4-demux-get-track-metadata-strings"),
	 &test_mp4_demux_get_track_metadata_strings},
	{FN("mp4-demux-get-metadata-cover"),
	 &test_mp4_demux_get_metadata_cover},
	{FN("mp4-demux-get-track-sample"), &test_mp4_demux_get_track_sample},
	{FN("mp4-demux-seek-to-track-next-sample"),
	 &test_mp4_demux_seek_to_track_next_sample},
	{FN("mp4-demux-seek-to-track-prev-sample"),
	 &test_mp4_demux_seek_to_track_prev_sample},
	{FN("mp4-demux-seek"), &test_mp4_demux_seek},
	{FN("mp4-demux-get-sample-time"), &test_mp4_demux_get_sample_time},
	{FN("mp4-demux-get-track-next-sample-time-after"),
	 &test_mp4_demux_get_track_next_sample_time_after},
	{FN("mp4-demux-get-track-next-sample-time-before"),
	 &test_mp4_demux_get_track_next_sample_time_before},

	CU_TEST_INFO_NULL,
};
