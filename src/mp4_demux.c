/**
 * @file mp4_demux.c
 * @brief MP4 file library - demuxer implementation
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

#include "mp4.h"


static int mp4_metadata_build(
	struct mp4_file *mp4)
{
	unsigned int i, k = 0, metaCount = 0, udtaCount = 0, xyzCount = 0;

	for (i = 0; i < mp4->metaMetadataCount; i++) {
		if ((mp4->metaMetadataValue[i]) &&
			(strlen(mp4->metaMetadataValue[i]) > 0) &&
			(mp4->metaMetadataKey[i]) &&
			(strlen(mp4->metaMetadataKey[i]) > 0))
			metaCount++;
	}

	for (i = 0; i < mp4->udtaMetadataCount; i++) {
		if ((mp4->udtaMetadataValue[i]) &&
			(strlen(mp4->udtaMetadataValue[i]) > 0) &&
			(mp4->udtaMetadataKey[i]) &&
			(strlen(mp4->udtaMetadataKey[i]) > 0))
			udtaCount++;
	}

	if ((mp4->udtaLocationValue) &&
		(strlen(mp4->udtaLocationValue) > 0) &&
		(mp4->udtaLocationKey) &&
		(strlen(mp4->udtaLocationKey) > 0))
		xyzCount++;

	mp4->finalMetadataCount = metaCount + udtaCount + xyzCount;

	mp4->finalMetadataKey = calloc(
		mp4->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((mp4->finalMetadataKey != NULL), -ENOMEM);

	mp4->finalMetadataValue = calloc(
		mp4->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((mp4->finalMetadataValue != NULL), -ENOMEM);

	for (i = 0; i < mp4->metaMetadataCount; i++) {
		if ((mp4->metaMetadataValue[i]) &&
			(strlen(mp4->metaMetadataValue[i]) > 0) &&
			(mp4->metaMetadataKey[i]) &&
			(strlen(mp4->metaMetadataKey[i]) > 0)) {
			mp4->finalMetadataKey[k] =
				mp4->metaMetadataKey[i];
			mp4->finalMetadataValue[k] =
				mp4->metaMetadataValue[i];
			k++;
		}
	}

	for (i = 0; i < mp4->udtaMetadataCount; i++) {
		if ((mp4->udtaMetadataValue[i]) &&
			(strlen(mp4->udtaMetadataValue[i]) > 0) &&
			(mp4->udtaMetadataKey[i]) &&
			(strlen(mp4->udtaMetadataKey[i]) > 0)) {
			mp4->finalMetadataKey[k] =
				mp4->udtaMetadataKey[i];
			mp4->finalMetadataValue[k] =
				mp4->udtaMetadataValue[i];
			k++;
		}
	}

	if ((mp4->udtaLocationValue) &&
		(strlen(mp4->udtaLocationValue) > 0) &&
		(mp4->udtaLocationKey) &&
		(strlen(mp4->udtaLocationKey) > 0)) {
		mp4->finalMetadataKey[k] = mp4->udtaLocationKey;
		mp4->finalMetadataValue[k] = mp4->udtaLocationValue;
		k++;
	}

	if (mp4->metaCoverSize > 0) {
		mp4->finalCoverSize = mp4->metaCoverSize;
		mp4->finalCoverOffset = mp4->metaCoverOffset;
		mp4->finalCoverType = mp4->metaCoverType;
	} else if (mp4->udtaCoverSize > 0) {
		mp4->finalCoverSize = mp4->udtaCoverSize;
		mp4->finalCoverOffset = mp4->udtaCoverOffset;
		mp4->finalCoverType = mp4->udtaCoverType;
	}

	return 0;
}


struct mp4_demux *mp4_demux_open(
	const char *filename)
{
	int err = 0, ret;
	off_t retBytes;
	struct mp4_demux *demux;
	struct mp4_file *mp4;

	MP4_RETURN_VAL_IF_FAILED(filename != NULL, -EINVAL, NULL);
	MP4_RETURN_VAL_IF_FAILED(strlen(filename) != 0, -EINVAL, NULL);

	demux = malloc(sizeof(*demux));
	if (demux == NULL) {
		MP4_LOGE("allocation failed");
		err = -ENOMEM;
		goto error;
	}
	memset(demux, 0, sizeof(*demux));
	mp4 = &demux->mp4;
	list_init(&mp4->tracks);

	mp4->file = fopen(filename, "rb");
	if (mp4->file == NULL) {
		MP4_LOGE("failed to open file '%s'", filename);
		err = -errno;
		goto error;
	}

	ret = fseeko(mp4->file, 0, SEEK_END);
	if (ret != 0) {
		MP4_LOGE("failed to seek to end of file");
		err = -errno;
		goto error;
	}
	mp4->fileSize = ftello(mp4->file);
	ret = fseeko(mp4->file, 0, SEEK_SET);
	if (ret != 0) {
		MP4_LOGE("failed to seek to beginning of file");
		err = -errno;
		goto error;
	}

	mp4->root = mp4_box_new(NULL);
	if (mp4->root == NULL) {
		MP4_LOGE("allocation failed");
		err = -ENOMEM;
		goto error;
	}
	mp4->root->type = MP4_ROOT_BOX;
	mp4->root->size = 1;
	mp4->root->largesize = mp4->fileSize;

	retBytes = mp4_box_children_read(
		mp4, mp4->root, mp4->fileSize, NULL);
	if (retBytes < 0) {
		MP4_LOGE("mp4_box_children_read() failed (%" PRIi64 ")",
			(int64_t)retBytes);
		err = -EIO;
		goto error;
	} else {
		mp4->readBytes += retBytes;
	}

	ret = mp4_tracks_build(mp4);
	if (ret < 0) {
		MP4_LOGE("mp4_tracks_build() failed (%d)", ret);
		err = -EIO;
		goto error;
	}
	ret = mp4_metadata_build(mp4);
	if (ret < 0) {
		MP4_LOGE("mp4_metadata_build() failed (%d)", ret);
		err = -EIO;
		goto error;
	}

	mp4_box_log(mp4->root, 0, ULOG_DEBUG);

	return demux;

error:
	if (demux)
		mp4_demux_close(demux);

	MP4_RETURN_VAL_IF_FAILED(0, err, NULL);
	return NULL;
}


int mp4_demux_close(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	if (demux) {
		struct mp4_file *mp4 = &demux->mp4;
		if (mp4->file)
			fclose(mp4->file);
		mp4_box_destroy(mp4->root);
		mp4_tracks_destroy(mp4);
		unsigned int i;
		for (i = 0; i < mp4->chaptersCount; i++)
			free(mp4->chaptersName[i]);
		free(mp4->udtaLocationKey);
		free(mp4->udtaLocationValue);
		for (i = 0; i < mp4->udtaMetadataCount; i++) {
			free(mp4->udtaMetadataKey[i]);
			free(mp4->udtaMetadataValue[i]);
		}
		free(mp4->udtaMetadataKey);
		free(mp4->udtaMetadataValue);
		for (i = 0; i < mp4->metaMetadataCount; i++) {
			free(mp4->metaMetadataKey[i]);
			free(mp4->metaMetadataValue[i]);
		}
		free(mp4->metaMetadataKey);
		free(mp4->metaMetadataValue);
		free(mp4->finalMetadataKey);
		free(mp4->finalMetadataValue);
	}

	free(demux);

	return 0;
}


int mp4_demux_seek(
	struct mp4_demux *demux,
	uint64_t time_offset,
	int sync)
{
	struct mp4_track *tk = NULL;
	struct mp4_file *mp4;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	mp4 = &demux->mp4;

	list_walk_entry_forward(&mp4->tracks, tk, node) {
		if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
			continue;
		if ((tk->type == MP4_TRACK_TYPE_METADATA) && (tk->ref))
			continue;

		int found = 0, i;
		uint64_t ts = mp4_usec_to_sample_time(time_offset,
			tk->timescale);
		uint64_t newPendingSeekTime = 0;
		int start = (unsigned int)(((uint64_t)tk->sampleCount * ts
				+ tk->duration - 1) / tk->duration);
		if (start < 0)
			start = 0;
		if ((unsigned)start >= tk->sampleCount)
			start = tk->sampleCount - 1;
		while (((unsigned)start < tk->sampleCount - 1)
				&& (tk->sampleDecodingTime[start] < ts))
			start++;
		for (i = start; i >= 0; i--) {
			if (tk->sampleDecodingTime[i] <= ts) {
				int isSync, prevSync = -1;
				isSync = mp4_track_is_sync_sample(
					tk, i, &prevSync);
				if ((isSync) || (!sync)) {
					start = i;
					found = 1;
					break;
				} else if (prevSync >= 0) {
					start = prevSync;
					found = 1;
					newPendingSeekTime =
						tk->sampleDecodingTime[i];
					break;
				}
			}
		}
		if (found) {
			tk->nextSample = start;
			tk->pendingSeekTime = newPendingSeekTime;
			MP4_LOGI("seek to %" PRIu64
				" -> sample #%d time %" PRIu64,
				time_offset, start,
				mp4_sample_time_to_usec(
					tk->sampleDecodingTime[start],
					tk->timescale));
			if ((tk->metadata) &&
				((unsigned)start < tk->metadata->sampleCount) &&
				(tk->sampleDecodingTime[start] ==
				tk->metadata->sampleDecodingTime[start]))
				tk->metadata->nextSample = start;
			else
				MP4_LOGW("failed to sync metadata"
					" with ref track");
		} else {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0, -ENOENT,
				"unable to seek in track");
		}
	}

	return 0;
}


int mp4_demux_get_media_info(
	struct mp4_demux *demux,
	struct mp4_media_info *media_info)
{
	struct mp4_file *mp4;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(media_info != NULL, -EINVAL);

	mp4 = &demux->mp4;

	memset(media_info, 0, sizeof(*media_info));

	media_info->duration = mp4_sample_time_to_usec(
		mp4->duration, mp4->timescale);
	media_info->creation_time =
		mp4->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->modification_time =
		mp4->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->track_count = mp4->trackCount;

	return 0;
}


int mp4_demux_get_track_count(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	return demux->mp4.trackCount;
}


int mp4_demux_get_track_info(
	struct mp4_demux *demux,
	unsigned int track_idx,
	struct mp4_track_info *track_info)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_info != NULL, -EINVAL);

	mp4 = &demux->mp4;

	MP4_RETURN_ERR_IF_FAILED(track_idx < mp4->trackCount, -EINVAL);

	tk = mp4_track_find_by_idx(mp4, track_idx);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	memset(track_info, 0, sizeof(*track_info));
	track_info->id = tk->id;
	track_info->type = tk->type;
	track_info->duration = mp4_sample_time_to_usec(
		tk->duration, tk->timescale);
	track_info->creation_time =
		tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	track_info->modification_time =
		tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	track_info->sample_count = tk->sampleCount;
	track_info->has_metadata = (tk->metadata) ? 1 : 0;
	if (tk->metadata) {
		track_info->metadata_content_encoding =
			tk->metadata->metadataContentEncoding;
		track_info->metadata_mime_format =
			tk->metadata->metadataMimeFormat;
	} else if (tk->type == MP4_TRACK_TYPE_METADATA) {
		track_info->metadata_content_encoding =
			tk->metadataContentEncoding;
		track_info->metadata_mime_format =
			tk->metadataMimeFormat;
	}
	if (tk->type == MP4_TRACK_TYPE_VIDEO) {
		track_info->video_codec = tk->videoCodec;
		track_info->video_width = tk->videoWidth;
		track_info->video_height = tk->videoHeight;
	} else if (tk->type == MP4_TRACK_TYPE_AUDIO) {
		track_info->audio_codec = tk->audioCodec;
		track_info->audio_channel_count = tk->audioChannelCount;
		track_info->audio_sample_size = tk->audioSampleSize;
		track_info->audio_sample_rate =
			(float)tk->audioSampleRate / 65536.;
	}

	return 0;
}


int mp4_demux_get_track_avc_decoder_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t **sps,
	unsigned int *sps_size,
	uint8_t **pps,
	unsigned int *pps_size)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps_size != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps_size != NULL, -EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	if (tk->videoSps) {
		*sps = tk->videoSps;
		*sps_size = tk->videoSpsSize;
	}
	if (tk->videoPps) {
		*pps = tk->videoPps;
		*pps_size = tk->videoPpsSize;
	}

	return 0;
}


int mp4_demux_get_track_audio_specific_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t **audio_specific_config,
	unsigned int *asc_size)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(audio_specific_config != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(asc_size != NULL, -EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		tk->audioSpecificConfig != NULL, -ENOENT,
		"track does not have an AudioSpecificConfig");

	*audio_specific_config = tk->audioSpecificConfig;
	*asc_size = tk->audioSpecificConfigSize;

	return 0;
}


int mp4_demux_get_track_next_sample(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t *sample_buffer,
	unsigned int sample_buffer_size,
	uint8_t *metadata_buffer,
	unsigned int metadata_buffer_size,
	struct mp4_track_sample *track_sample)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t sampleTime;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_sample != NULL, -EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	memset(track_sample, 0, sizeof(*track_sample));

	if (tk->nextSample < tk->sampleCount) {
		track_sample->sample_size = tk->sampleSize[tk->nextSample];
		if ((sample_buffer) &&
			(tk->sampleSize[tk->nextSample] > 0) &&
			(tk->sampleSize[tk->nextSample] <=
			sample_buffer_size)) {
			int _ret = fseeko(mp4->file,
				tk->sampleOffset[tk->nextSample], SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIu64
				" bytes forward in file",
				tk->sampleOffset[tk->nextSample]);
			size_t count = fread(sample_buffer,
				tk->sampleSize[tk->nextSample],
				1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %d bytes from file",
				tk->sampleSize[tk->nextSample]);
		} else if ((sample_buffer) &&
			(tk->sampleSize[tk->nextSample] > sample_buffer_size)) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0, -ENOBUFS,
				"buffer too small (%d bytes, %d needed)",
				sample_buffer_size,
				tk->sampleSize[tk->nextSample]);
		}
		if (tk->metadata) {
			struct mp4_track *metatk = tk->metadata;
			/* TODO: check sync between metadata and ref track */
			track_sample->metadata_size =
				metatk->sampleSize[tk->nextSample];
			if ((metadata_buffer) &&
				(metatk->sampleSize[tk->nextSample] > 0) &&
				(metatk->sampleSize[tk->nextSample] <=
				metadata_buffer_size)) {
				int _ret = fseeko(mp4->file,
					metatk->sampleOffset[tk->nextSample],
					SEEK_SET);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					_ret == 0, -errno,
					"failed to seek %" PRIu64
					" bytes forward in file",
					metatk->sampleOffset[
						tk->nextSample]);
				size_t count = fread(metadata_buffer,
					metatk->sampleSize[tk->nextSample],
					1, mp4->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %d bytes from file",
					metatk->sampleSize[tk->nextSample]);
			} else if ((metadata_buffer) &&
				(metatk->sampleSize[tk->nextSample] >
				metadata_buffer_size)) {
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0,
					-ENOBUFS,
					"buffer too small for metadata "
					"(%d bytes, %d needed)",
					metadata_buffer_size,
					metatk->sampleSize[tk->nextSample]);
			}
		}
		sampleTime = tk->sampleDecodingTime[tk->nextSample];
		track_sample->silent = ((tk->pendingSeekTime) &&
			(sampleTime < tk->pendingSeekTime)) ? 1 : 0;
		if (sampleTime >= tk->pendingSeekTime)
			tk->pendingSeekTime = 0;
		track_sample->sample_dts = mp4_sample_time_to_usec(
			sampleTime, tk->timescale);
		track_sample->next_sample_dts =
			(tk->nextSample < tk->sampleCount - 1) ?
			mp4_sample_time_to_usec(
				tk->sampleDecodingTime[tk->nextSample + 1],
				tk->timescale) : 0;
		idx = mp4_track_find_sample_by_time(tk, sampleTime,
			MP4_TIME_CMP_LT, 1, tk->nextSample);
		if (idx >= 0) {
			track_sample->prev_sync_sample_dts =
				mp4_sample_time_to_usec(
					tk->sampleDecodingTime[idx],
					tk->timescale);
		}
		idx = mp4_track_find_sample_by_time(tk, sampleTime,
			MP4_TIME_CMP_GT, 1, tk->nextSample);
		if (idx >= 0) {
			track_sample->next_sync_sample_dts =
				mp4_sample_time_to_usec(
					tk->sampleDecodingTime[idx],
					tk->timescale);
		}
		tk->nextSample++;
	}

	return 0;
}


int mp4_demux_seek_to_track_prev_sample(
	struct mp4_demux *demux,
	unsigned int track_id)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	idx = (tk->nextSample >= 2) ? tk->nextSample - 2 : 0;
	ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
		tk->timescale);

	return mp4_demux_seek(demux, ts, 1);
}


uint64_t mp4_demux_get_track_next_sample_time(
	struct mp4_demux *demux,
	unsigned int track_id)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	uint64_t next_ts = 0;

	MP4_RETURN_VAL_IF_FAILED(demux != NULL, -EINVAL, 0);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(tk != NULL, -ENOENT,
		"track not found");

	if (tk->nextSample < tk->sampleCount) {
		next_ts = mp4_sample_time_to_usec(
			tk->sampleDecodingTime[tk->nextSample], tk->timescale);
	}

	return next_ts;
}


uint64_t mp4_demux_get_track_prev_sample_time_before(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint64_t time,
	int sync)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts = 0;

	MP4_RETURN_VAL_IF_FAILED(demux != NULL, -EINVAL, 0);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_VAL_IF_FAILED(tk != NULL, -ENOENT, 0,
		"track not found");

	ts = mp4_usec_to_sample_time(time, tk->timescale);
	idx = mp4_track_find_sample_by_time(tk, ts,
		MP4_TIME_CMP_LT, sync, -1);

	if (idx >= 0) {
		return mp4_sample_time_to_usec(
			tk->sampleDecodingTime[idx], tk->timescale);
	} else {
		return 0;
	}
}


uint64_t mp4_demux_get_track_next_sample_time_after(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint64_t time,
	int sync)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts = 0;

	MP4_RETURN_VAL_IF_FAILED(demux != NULL, -EINVAL, 0);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	MP4_LOG_ERR_AND_RETURN_VAL_IF_FAILED(tk != NULL, -ENOENT, 0,
		"track not found");

	ts = mp4_usec_to_sample_time(time, tk->timescale);
	idx = mp4_track_find_sample_by_time(tk, ts,
		MP4_TIME_CMP_GT, sync, -1);

	if (idx >= 0) {
		return mp4_sample_time_to_usec(
			tk->sampleDecodingTime[idx], tk->timescale);
	} else {
		return 0;
	}
}


int mp4_demux_get_chapters(
	struct mp4_demux *demux,
	unsigned int *chaptersCount,
	uint64_t **chaptersTime,
	char ***chaptersName)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersCount != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersTime != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersName != NULL, -EINVAL);

	*chaptersCount = demux->mp4.chaptersCount;
	*chaptersTime = demux->mp4.chaptersTime;
	*chaptersName = demux->mp4.chaptersName;

	return 0;
}


int mp4_demux_get_metadata_strings(
	struct mp4_demux *demux,
	unsigned int *count,
	char ***keys,
	char ***values)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(count != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(keys != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(values != NULL, -EINVAL);

	*count = demux->mp4.finalMetadataCount;
	*keys = demux->mp4.finalMetadataKey;
	*values = demux->mp4.finalMetadataValue;

	return 0;
}


int mp4_demux_get_metadata_cover(
	struct mp4_demux *demux,
	uint8_t *cover_buffer,
	unsigned int cover_buffer_size,
	unsigned int *cover_size,
	enum mp4_metadata_cover_type *cover_type)
{
	struct mp4_file *mp4;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	mp4 = &demux->mp4;

	if (mp4->finalCoverSize > 0) {
		if (cover_size)
			*cover_size = mp4->finalCoverSize;
		if (cover_type)
			*cover_type = mp4->finalCoverType;
		if ((cover_buffer) &&
			(mp4->finalCoverSize <= cover_buffer_size)) {
			int _ret = fseeko(mp4->file,
				mp4->finalCoverOffset, SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIi64
				" bytes forward in file",
				(int64_t)mp4->finalCoverOffset);
			size_t count = fread(cover_buffer,
				mp4->finalCoverSize, 1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %" PRIu32 " bytes from file",
				mp4->finalCoverSize);
		} else if (cover_buffer) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0, -ENOBUFS,
				"buffer too small (%d bytes, %d needed)",
				cover_buffer_size, mp4->finalCoverSize);
		}
	} else {
		*cover_size = 0;
	}

	return 0;
}
