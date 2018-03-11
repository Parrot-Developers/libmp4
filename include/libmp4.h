/**
 * @file libmp4.h
 * @brief MP4 file library - interface
 * @date 07/11/2016
 * @author aurelien.barre@akaaba.net
 *
 * Copyright (c) 2016 Aurelien Barre <aurelien.barre@akaaba.net>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of the copyright holder nor the names of the
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBMP4_H_
#define _LIBMP4_H_

#ifdef __cplusplus
extern "C"  {
#endif /* __cplusplus */

#include <inttypes.h>


enum mp4_track_type {
	MP4_TRACK_TYPE_UNKNOWN = 0,
	MP4_TRACK_TYPE_VIDEO,
	MP4_TRACK_TYPE_AUDIO,
	MP4_TRACK_TYPE_HINT,
	MP4_TRACK_TYPE_METADATA,
	MP4_TRACK_TYPE_TEXT,
	MP4_TRACK_TYPE_CHAPTERS,

	MP4_TRACK_TYPE_MAX,
};


enum mp4_video_codec {
	MP4_VIDEO_CODEC_UNKNOWN = 0,
	MP4_VIDEO_CODEC_AVC,

	MP4_VIDEO_CODEC_MAX,
};


enum mp4_audio_codec {
	MP4_AUDIO_CODEC_UNKNOWN = 0,
	MP4_AUDIO_CODEC_AAC,

	MP4_AUDIO_CODEC_MAX,
};


enum mp4_metadata_cover_type {
	MP4_METADATA_COVER_TYPE_JPEG = 0,
	MP4_METADATA_COVER_TYPE_PNG,
	MP4_METADATA_COVER_TYPE_BMP,

	MP4_METADATA_COVER_TYPE_MAX,
};


struct mp4_media_info {
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t track_count;
};


struct mp4_track_info {
	uint32_t id;
	enum mp4_track_type type;
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t sample_count;
	enum mp4_video_codec video_codec;
	uint32_t video_width;
	uint32_t video_height;
	enum mp4_audio_codec audio_codec;
	uint32_t audio_channel_count;
	uint32_t audio_sample_size;
	float audio_sample_rate;
	int has_metadata;
	char *metadata_content_encoding;
	char *metadata_mime_format;
};


struct mp4_track_sample {
	uint32_t sample_size;
	uint32_t metadata_size;
	int silent;
	uint64_t sample_dts;
	uint64_t next_sample_dts;
	uint64_t prev_sync_sample_dts;
	uint64_t next_sync_sample_dts;
};


struct mp4_demux;


struct mp4_demux *mp4_demux_open(
	const char *filename);


int mp4_demux_close(
	struct mp4_demux *demux);


int mp4_demux_seek(
	struct mp4_demux *demux,
	uint64_t time_offset,
	int sync);


int mp4_demux_get_media_info(
	struct mp4_demux *demux,
	struct mp4_media_info *media_info);


int mp4_demux_get_track_count(
	struct mp4_demux *demux);


int mp4_demux_get_track_info(
	struct mp4_demux *demux,
	unsigned int track_idx,
	struct mp4_track_info *track_info);


int mp4_demux_get_track_avc_decoder_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t **sps,
	unsigned int *sps_size,
	uint8_t **pps,
	unsigned int *pps_size);

int mp4_demux_get_track_audio_specific_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t **audio_specific_config,
	unsigned int *asc_size);


int mp4_demux_get_track_next_sample(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t *sample_buffer,
	unsigned int sample_buffer_size,
	uint8_t *metadata_buffer,
	unsigned int metadata_buffer_size,
	struct mp4_track_sample *track_sample);


int mp4_demux_seek_to_track_prev_sample(
	struct mp4_demux *demux,
	unsigned int track_id);


uint64_t mp4_demux_get_track_next_sample_time(
	struct mp4_demux *demux,
	unsigned int track_id);


uint64_t mp4_demux_get_track_prev_sample_time_before(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint64_t time,
	int sync);


uint64_t mp4_demux_get_track_next_sample_time_after(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint64_t time,
	int sync);


int mp4_demux_get_chapters(
	struct mp4_demux *demux,
	unsigned int *chaptersCount,
	uint64_t **chaptersTime,
	char ***chaptersName);


int mp4_demux_get_metadata_strings(
	struct mp4_demux *demux,
	unsigned int *count,
	char ***keys,
	char ***values);


int mp4_demux_get_metadata_cover(
	struct mp4_demux *demux,
	uint8_t *cover_buffer,
	unsigned int cover_buffer_size,
	unsigned int *cover_size,
	enum mp4_metadata_cover_type *cover_type);


int mp4_generate_avc_decoder_config(
	uint8_t *sps,
	unsigned int sps_size,
	uint8_t *pps,
	unsigned int pps_size,
	uint8_t *avcc,
	unsigned int *avcc_size);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_LIBMP4_H_ */
