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

#ifndef _FILE_OFFSET_BITS
#	define _FILE_OFFSET_BITS 64
#endif


#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//#include <futils/futils.h>
#include "libmp4.h"


#define ULOG_TAG mp4_mux_test
#include "ulog.h"
ULOG_DECLARE_TAG(mp4_mux_test);


char *mdata_video_keys[] = {"com.parrot.thermal.metaversion",
			    "com.parrot.thermal.alignment",
			    "com.parrot.thermal.scalefactor"};

char *mdata_video_values[] = {"2", "0.000000,0.000000,0.000000", "1.836559"};

unsigned int mdata_video_count = SIZEOF_ARRAY(mdata_video_keys);

static char *mdata_audio_keys[] = {
	"\xA9"
	"nam\0",
	"\xA9"
	"ART\0",
	"\xA9"
	"day\0",
	"\xA9"
	"too\0",
	"\xA9"
	"cmt\0"};

#if 0
static char *mdata_audio_values[] = {"incredible machine",
				     "3 years old scientist",
				     "2019",
				     "Lavf57.83.100",
				     "just a random test video"};
#endif

unsigned int mdata_audio_count = SIZEOF_ARRAY(mdata_audio_keys);


int main(int argc, char *argv[])
{
	int ret;
	uint64_t now;

	if (argc < 3) {
		ULOGE("usage: %s input_file output_file", argv[0]);
		return 1;
	}

	const char *in = argv[1];
	const char *out = argv[2];

	ULOGI("demux %s and remux into %s", in, out);

	now = time(NULL);

	unsigned int sample_buffer_size = 5 * 1024 * 1024;
	unsigned int metadata_buffer_size = 1 * 1024 * 1024;

	uint8_t *sample_buffer = NULL;
	uint8_t *metadata_buffer = NULL;

	struct mp4_mux *mux = NULL;
	struct mp4_demux *demux = NULL;

	int ntracks;

	unsigned int meta_file_count;
	char **meta_file_keys;
	char **meta_file_vals;

	int videotrack = -1;
	int metatrack = -1;
	int audiotrack = -1;

	int current_track = -1;
	int has_more_audio = 0;
	int has_more_video = 0;

	int vs_count = 0;
	int as_count = 0;
	uint64_t step_ts = 0;
	const uint64_t increment_ts = 100000; /* 100ms */
	unsigned int cover_size, j;
	enum mp4_metadata_cover_type cover_type;

	struct mp4_track_info info, video, audio;

	sample_buffer = malloc(sample_buffer_size);
	if (sample_buffer == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		goto out;
	}
	metadata_buffer = malloc(metadata_buffer_size);
	if (metadata_buffer == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		goto out;
	}

	ret = mp4_demux_open(in, &demux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_open", EIO);
		goto out;
	}
	ret = mp4_mux_open(out, 30000, now, now, &mux);
	if (ret != 0) {
		ULOG_ERRNO("mp4_mux_open", -ret);
		goto out;
	}

	/* Get number of tracks in input file */
	ntracks = mp4_demux_get_track_count(demux);
	ULOGD("%d tracks", ntracks);

	mp4_demux_get_metadata_strings(
		demux, &meta_file_count, &meta_file_keys, &meta_file_vals);
	for (unsigned int i = 0; i < meta_file_count; i++) {
		ULOGI("META: %s :: %s", meta_file_keys[i], meta_file_vals[i]);
		mp4_mux_add_file_metadata(
			mux, meta_file_keys[i], meta_file_vals[i]);
	}

	/* Find ID of first video / audio track */
	for (int i = 0; i < ntracks; i++) {
		current_track = -1;
		ret = mp4_demux_get_track_info(demux, i, &info);
		if (ret != 0) {
			ULOG_ERRNO("mp4_demux_get_track_info(%d)", -ret, i);
			continue;
		}
		struct mp4_mux_track_params params = {
			.type = info.type,
			.name = info.name,
			.enabled = info.enabled,
			.in_movie = info.in_movie,
			.in_preview = info.in_preview,
			.timescale = info.timescale,
			.creation_time = info.creation_time,
			.modification_time = info.modification_time,
		};
		if (info.type == MP4_TRACK_TYPE_VIDEO && videotrack == -1) {
			struct mp4_video_decoder_config vdc;
			video = info;
			videotrack = mp4_mux_add_track(mux, &params);
			ret = mp4_demux_get_track_video_decoder_config(
				demux, info.id, &vdc);
			if (ret != 0) {
				ULOG_ERRNO(
					"mp4_demux_get_track"
					"_avc_decoder_config",
					-ret);
				goto out;
			}
			mp4_mux_track_set_video_decoder_config(
				mux, videotrack, &vdc);
			has_more_video = info.sample_count > 0;
			current_track = videotrack;
		}
		if (info.type == MP4_TRACK_TYPE_AUDIO && audiotrack == -1) {
			uint8_t *audioSpecificConfig;
			unsigned int asc_size;
			audio = info;
			audiotrack = mp4_mux_add_track(mux, &params);
			ret = mp4_demux_get_track_audio_specific_config(
				demux,
				info.id,
				&audioSpecificConfig,
				&asc_size);
			if (ret != 0) {
				ULOG_ERRNO(
					"mp4_demux_get_track"
					"_audio_specific_config",
					-ret);
				goto out;
			}
			mp4_mux_track_set_audio_specific_config(
				mux,
				audiotrack,
				audioSpecificConfig,
				asc_size,
				info.audio_channel_count,
				info.audio_sample_size,
				info.audio_sample_rate);
			has_more_audio = info.sample_count > 0;
			current_track = audiotrack;
		}
		if (info.type == MP4_TRACK_TYPE_METADATA && metatrack == -1) {
			metatrack = mp4_mux_add_track(mux, &params);
			mp4_mux_track_set_metadata_mime_type(
				mux,
				metatrack,
				info.content_encoding,
				info.mime_format);
			current_track = metatrack;
		}

		/* Add track metada */
		if (current_track > 0) {

			unsigned int meta_count = 0;
			char **keys = NULL;
			char **values = NULL;

			ret = mp4_demux_get_track_metadata_strings(
				demux,
				info.id,
				&meta_count,
				&keys,
				&values);
			if (ret < 0) {
				ULOG_ERRNO(
					"mp4_demux_get_track_metadata_strings",
					-ret);
				continue;
			}

#if 0
			/* If no metadata found for this track, we add some
			hardsetted ones for audio and video tracks */
			/* TODO: remove once mux unit test exits */
			if (meta_count == 0) {
				if (current_track == videotrack) {
					meta_count = mdata_video_count;
					keys = mdata_video_keys;
					values = mdata_video_values;
				} else if (current_track == audiotrack) {
					meta_count = mdata_audio_count;
					values = mdata_audio_values;
					keys = mdata_audio_keys;
				}
			}
#endif

			for (j = 0; j < meta_count; j++) {
				if ((keys[j]) && (values[j])) {
					mp4_mux_add_track_metadata(
						mux,
						current_track,
						keys[j],
						values[j]);
				}
			}
		}
	}
	if (videotrack < 0) {
		ULOGE("no video track");
		goto out;
	}

	/* Add track reference */
	if (metatrack > 0) {
		ULOGI("metatrack = %d, videotrack = %d", metatrack, videotrack);
		ret = mp4_mux_add_ref_to_track(mux, metatrack, videotrack);
		if (ret != 0) {
			ULOG_ERRNO("mp4_mux_add_ref_to_track", -ret);
			goto out;
		}
	}

	/* Set cover, if available */
	ret = mp4_demux_get_metadata_cover(demux,
					   sample_buffer,
					   sample_buffer_size,
					   &cover_size,
					   &cover_type);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);
		goto out;
	}
	if (cover_size > 0)
		mp4_mux_set_file_cover(
			mux, cover_type, sample_buffer, cover_size);

	/* Iterate over samples, 100ms at a time */
	while (has_more_audio || has_more_video) {
		struct mp4_track_sample sample;
		struct mp4_mux_sample mux_sample;
		
		int lc_video = 0;
		int lc_audio = 0;
		step_ts += increment_ts;
		while (has_more_video) {
			memset(metadata_buffer, 0, 8);
			ret = mp4_demux_get_track_sample(demux,
							 video.id,
							 1,
							 sample_buffer,
							 sample_buffer_size,
							 metadata_buffer,
							 metadata_buffer_size,
							 &sample);

			if (ret != 0 || sample.size == 0) {
				has_more_video = 0;
				break;
			}
			ULOGD("got a video sample [%d] of size %" PRIu32
			      ", with meta of size %" PRIu32,
			      vs_count++,
			      sample.size,
			      sample.metadata_size);
			lc_video++;

			mux_sample.buffer = sample_buffer;
			mux_sample.len = sample.size;
			mux_sample.sync = sample.sync;
			mux_sample.dts = sample.dts;
			mp4_mux_track_add_sample(mux, videotrack, &mux_sample);
			if (sample.metadata_size > 0 && metatrack != -1) {
				mux_sample.buffer = metadata_buffer;
				mux_sample.len = sample.metadata_size;
				mp4_mux_track_add_sample(
					mux, metatrack, &mux_sample);
			}
			if (mp4_sample_time_to_usec(sample.next_dts,
						    info.timescale) > step_ts)
				break;
		}
		while (has_more_audio) {
			ret = mp4_demux_get_track_sample(demux,
							 audio.id,
							 1,
							 sample_buffer,
							 sample_buffer_size,
							 NULL,
							 0,
							 &sample);
			if (ret != 0 || sample.size == 0) {
				has_more_audio = 0;
				break;
			}
			ULOGD("got an audio sample [%d] of size %" PRIu32,
			      as_count++,
			      sample.size);
			lc_audio++;

			mux_sample.buffer = sample_buffer;
			mux_sample.len = sample.size;
			mux_sample.sync = 0;
			mux_sample.dts = sample.dts;
			mp4_mux_track_add_sample(mux, audiotrack, &mux_sample);
			if (mp4_sample_time_to_usec(sample.next_dts,
						    info.timescale) > step_ts)
				break;
		}
		ULOGD("added %d video samples and %d audio samples",
		      lc_video,
		      lc_audio);
	}

	if (as_count < 100 && vs_count < 100)
		mp4_mux_dump(mux);

out:
	free(metadata_buffer);
	free(sample_buffer);
	mp4_mux_close(mux);
	mp4_demux_close(demux);

	return 0;
}
