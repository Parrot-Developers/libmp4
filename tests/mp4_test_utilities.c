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

static const struct {
	const char *path;
} nas_tests_mp4_utilities[] = {
	{
		"Tests/anafi/4k/video_recording/champs_1080p30.mp4",
	},
	{
		"Tests/anafi/4k/video_recording/jardin_2160p30.mp4",
	},
};


static void test_mp4_metadata_cover_type_str(void)
{
	CU_ASSERT_STRING_EQUAL(
		mp4_metadata_cover_type_str(MP4_METADATA_COVER_TYPE_JPEG),
		"JPEG");
	CU_ASSERT_STRING_EQUAL(
		mp4_metadata_cover_type_str(MP4_METADATA_COVER_TYPE_PNG),
		"PNG");
	CU_ASSERT_STRING_EQUAL(
		mp4_metadata_cover_type_str(MP4_METADATA_COVER_TYPE_BMP),
		"BMP");
	CU_ASSERT_STRING_EQUAL(
		mp4_metadata_cover_type_str(MP4_METADATA_COVER_TYPE_UNKNOWN),
		"UNKNOWN");
}


static void test_mp4_audio_codec_str(void)
{
	CU_ASSERT_STRING_EQUAL(mp4_audio_codec_str(MP4_AUDIO_CODEC_AAC_LC),
			       "AAC_LC");
	CU_ASSERT_STRING_EQUAL(mp4_audio_codec_str(MP4_AUDIO_CODEC_UNKNOWN),
			       "UNKNOWN");
}


static void test_mp4_video_codec_str(void)
{
	CU_ASSERT_STRING_EQUAL(mp4_video_codec_str(MP4_VIDEO_CODEC_AVC), "AVC");
	CU_ASSERT_STRING_EQUAL(mp4_video_codec_str(MP4_VIDEO_CODEC_HEVC),
			       "HEVC");
	CU_ASSERT_STRING_EQUAL(mp4_video_codec_str(MP4_VIDEO_CODEC_UNKNOWN),
			       "UNKNOWN");
}


static void test_mp4_track_type_str(void)
{
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_VIDEO),
			       "VIDEO");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_AUDIO),
			       "AUDIO");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_HINT), "HINT");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_METADATA),
			       "METADATA");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_TEXT), "TEXT");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_CHAPTERS),
			       "CHAPTERS");
	CU_ASSERT_STRING_EQUAL(mp4_track_type_str(MP4_TRACK_TYPE_UNKNOWN),
			       "UNKNOWN");
}


static void test_mp4_utilities_mp4_to_json(void)
{
	int res = 0;
	struct json_object *json_obj;
	char path[200];
	snprintf(path,
		 sizeof(path),
		 "%s/%s",
		 (getenv("ASSETS_ROOT") != NULL) ? getenv("ASSETS_ROOT")
						 : ASSETS_ROOT,
		 nas_tests_mp4_utilities[0].path);

	/* all null */
	res = mp4_file_to_json(NULL, false, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* filename null */
	res = mp4_file_to_json(NULL, false, &json_obj);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* json null */
	res = mp4_file_to_json(path, false, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	for (size_t i = 0; i < FUTILS_SIZEOF_ARRAY(nas_tests_mp4_utilities);
	     i++) {
		snprintf(path,
			 sizeof(path),
			 "%s/%s",
			 (getenv("ASSETS_ROOT") != NULL) ? getenv("ASSETS_ROOT")
							 : ASSETS_ROOT,
			 nas_tests_mp4_utilities[i].path);

		res = mp4_file_to_json(path, false, &json_obj);
		CU_ASSERT_EQUAL(res, 0);
		CU_ASSERT_PTR_NOT_NULL(json_obj);
		json_object_put(json_obj);
	}
}


static void test_mp4_utilities_generate_chapter_sample(void)
{
	int res = 0;
	char *chapter_str = "chapter_name";
	uint8_t *buffer;
	unsigned int buffer_size;

	/* all null */
	res = mp4_generate_chapter_sample(NULL, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* buffer and buffer_size null */
	res = mp4_generate_chapter_sample(chapter_str, NULL, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* buffer null */
	res = mp4_generate_chapter_sample(chapter_str, NULL, &buffer_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* buffer size null */
	res = mp4_generate_chapter_sample(chapter_str, &buffer, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* name null */
	res = mp4_generate_chapter_sample(NULL, &buffer, &buffer_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* name and buffer null */
	res = mp4_generate_chapter_sample(NULL, NULL, &buffer_size);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* name and buffer_size null */
	res = mp4_generate_chapter_sample(NULL, &buffer, NULL);
	CU_ASSERT_EQUAL(res, -EINVAL);

	/* valid */
	res = mp4_generate_chapter_sample(chapter_str, &buffer, &buffer_size);
	CU_ASSERT_EQUAL(res, 0);
	CU_ASSERT_PTR_NOT_NULL(buffer);
	CU_ASSERT(buffer_size > 0);

	free(buffer);
}


CU_TestInfo g_mp4_test_utilities[] = {
	{FN("mp4-utilities-generate-chapter-sample"),
	 &test_mp4_utilities_generate_chapter_sample},
	{FN("mp4-utilities-mp4-to-json"), &test_mp4_utilities_mp4_to_json},
	{FN("mp4-utilities-mp4-track-type-str"), &test_mp4_track_type_str},
	{FN("mp4-utilities-mp4-video-codec-str"), &test_mp4_video_codec_str},
	{FN("mp4-utilities-mp4-audio-codec-str"), &test_mp4_audio_codec_str},
	{FN("mp4-utilities-mp4-metadata-cover-type-str"),
	 &test_mp4_metadata_cover_type_str},

	CU_TEST_INFO_NULL,
};
