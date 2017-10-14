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


static int mp4_demux_build_metadata(
	struct mp4_demux *demux)
{
	unsigned int i, k = 0, metaCount = 0, udtaCount = 0, xyzCount = 0;

	for (i = 0; i < demux->metaMetadataCount; i++) {
		if ((demux->metaMetadataValue[i]) &&
			(strlen(demux->metaMetadataValue[i]) > 0) &&
			(demux->metaMetadataKey[i]) &&
			(strlen(demux->metaMetadataKey[i]) > 0))
			metaCount++;
	}

	for (i = 0; i < demux->udtaMetadataCount; i++) {
		if ((demux->udtaMetadataValue[i]) &&
			(strlen(demux->udtaMetadataValue[i]) > 0) &&
			(demux->udtaMetadataKey[i]) &&
			(strlen(demux->udtaMetadataKey[i]) > 0))
			udtaCount++;
	}

	if ((demux->udtaLocationValue) &&
		(strlen(demux->udtaLocationValue) > 0) &&
		(demux->udtaLocationKey) &&
		(strlen(demux->udtaLocationKey) > 0))
		xyzCount++;

	demux->finalMetadataCount = metaCount + udtaCount + xyzCount;

	demux->finalMetadataKey = calloc(
		demux->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->finalMetadataKey != NULL), -ENOMEM);

	demux->finalMetadataValue = calloc(
		demux->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->finalMetadataValue != NULL), -ENOMEM);

	for (i = 0; i < demux->metaMetadataCount; i++) {
		if ((demux->metaMetadataValue[i]) &&
			(strlen(demux->metaMetadataValue[i]) > 0) &&
			(demux->metaMetadataKey[i]) &&
			(strlen(demux->metaMetadataKey[i]) > 0)) {
			demux->finalMetadataKey[k] =
				demux->metaMetadataKey[i];
			demux->finalMetadataValue[k] =
				demux->metaMetadataValue[i];
			k++;
		}
	}

	for (i = 0; i < demux->udtaMetadataCount; i++) {
		if ((demux->udtaMetadataValue[i]) &&
			(strlen(demux->udtaMetadataValue[i]) > 0) &&
			(demux->udtaMetadataKey[i]) &&
			(strlen(demux->udtaMetadataKey[i]) > 0)) {
			demux->finalMetadataKey[k] =
				demux->udtaMetadataKey[i];
			demux->finalMetadataValue[k] =
				demux->udtaMetadataValue[i];
			k++;
		}
	}

	if ((demux->udtaLocationValue) &&
		(strlen(demux->udtaLocationValue) > 0) &&
		(demux->udtaLocationKey) &&
		(strlen(demux->udtaLocationKey) > 0)) {
		demux->finalMetadataKey[k] = demux->udtaLocationKey;
		demux->finalMetadataValue[k] = demux->udtaLocationValue;
		k++;
	}

	if (demux->metaCoverSize > 0) {
		demux->finalCoverSize = demux->metaCoverSize;
		demux->finalCoverOffset = demux->metaCoverOffset;
		demux->finalCoverType = demux->metaCoverType;
	} else if (demux->udtaCoverSize > 0) {
		demux->finalCoverSize = demux->udtaCoverSize;
		demux->finalCoverOffset = demux->udtaCoverOffset;
		demux->finalCoverType = demux->udtaCoverType;
	}

	return 0;
}


struct mp4_demux *mp4_demux_open(
	const char *filename)
{
	int err = 0, ret;
	off_t retBytes;
	struct mp4_demux *demux;

	MP4_RETURN_VAL_IF_FAILED(filename != NULL, -EINVAL, NULL);
	MP4_RETURN_VAL_IF_FAILED(strlen(filename) != 0, -EINVAL, NULL);

	demux = malloc(sizeof(*demux));
	if (demux == NULL) {
		MP4_LOGE("allocation failed");
		err = -ENOMEM;
		goto error;
	}
	memset(demux, 0, sizeof(*demux));

	demux->file = fopen(filename, "rb");
	if (demux->file == NULL) {
		MP4_LOGE("failed to open file '%s'", filename);
		err = -errno;
		goto error;
	}

	ret = fseeko(demux->file, 0, SEEK_END);
	if (ret != 0) {
		MP4_LOGE("failed to seek to end of file");
		err = -errno;
		goto error;
	}
	demux->fileSize = ftello(demux->file);
	ret = fseeko(demux->file, 0, SEEK_SET);
	if (ret != 0) {
		MP4_LOGE("failed to seek to beginning of file");
		err = -errno;
		goto error;
	}

	retBytes = mp4_demux_parse_children(
		demux, &demux->root, demux->fileSize, NULL);
	if (retBytes < 0) {
		MP4_LOGE("mp4_demux_parse_children() failed (%" PRIi64 ")",
			(int64_t)retBytes);
		err = -EIO;
		goto error;
	} else {
		demux->readBytes += retBytes;
	}

	ret = mp4_demux_build_tracks(demux);
	if (ret < 0) {
		MP4_LOGE("mp4_demux_build_tracks() failed (%d)", ret);
		err = -EIO;
		goto error;
	}
	ret = mp4_demux_build_metadata(demux);
	if (ret < 0) {
		MP4_LOGE("mp4_demux_build_metadata() failed (%d)", ret);
		err = -EIO;
		goto error;
	}

	mp4_demux_print_children(demux, &demux->root, 0);

	return demux;

error:
	if (demux)
		mp4_demux_close(demux);

	MP4_RETURN_VAL_IF_FAILED(1, err, NULL);
	return NULL;
}


int mp4_demux_close(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	if (demux) {
		if (demux->file)
			fclose(demux->file);
		mp4_demux_free_children(demux, &demux->root);
		mp4_demux_free_tracks(demux);
		unsigned int i;
		for (i = 0; i < demux->chaptersCount; i++)
			free(demux->chaptersName[i]);
		free(demux->udtaLocationKey);
		free(demux->udtaLocationValue);
		for (i = 0; i < demux->udtaMetadataCount; i++) {
			free(demux->udtaMetadataKey[i]);
			free(demux->udtaMetadataValue[i]);
		}
		free(demux->udtaMetadataKey);
		free(demux->udtaMetadataValue);
		for (i = 0; i < demux->metaMetadataCount; i++) {
			free(demux->metaMetadataKey[i]);
			free(demux->metaMetadataValue[i]);
		}
		free(demux->metaMetadataKey);
		free(demux->metaMetadataValue);
		free(demux->finalMetadataKey);
		free(demux->finalMetadataValue);
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

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
			continue;
		if ((tk->type == MP4_TRACK_TYPE_METADATA) && (tk->ref))
			continue;

		int found = 0, i;
		uint64_t ts = (time_offset * tk->timescale + 500000) / 1000000;
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
				isSync = mp4_demux_is_sync_sample(
					demux, tk, i, &prevSync);
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
				(tk->sampleDecodingTime[start] * 1000000 +
				tk->timescale / 2) / tk->timescale);
			if ((tk->metadata) &&
				((unsigned)start < tk->metadata->sampleCount) &&
				(tk->sampleDecodingTime[start] ==
				tk->metadata->sampleDecodingTime[start]))
				tk->metadata->nextSample = start;
			else
				MP4_LOGW("failed to sync metadata"
					" with ref track");
		} else {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
				"unable to seek in track");
		}
	}

	return 0;
}


int mp4_demux_get_media_info(
	struct mp4_demux *demux,
	struct mp4_media_info *media_info)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(media_info != NULL, -EINVAL);

	memset(media_info, 0, sizeof(*media_info));

	media_info->duration =
		(demux->duration * 1000000 + demux->timescale / 2) /
		demux->timescale;
	media_info->creation_time =
		demux->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->modification_time =
		demux->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->track_count = demux->trackCount;

	return 0;
}


int mp4_demux_get_track_count(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	return demux->trackCount;
}


int mp4_demux_get_track_info(
	struct mp4_demux *demux,
	unsigned int track_idx,
	struct mp4_track_info *track_info)
{
	struct mp4_track *tk = NULL;
	unsigned int k;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_info != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_idx < demux->trackCount, -EINVAL);

	memset(track_info, 0, sizeof(*track_info));

	for (tk = demux->track, k = 0; (tk) && (k < track_idx);
		tk = tk->next, k++)
		;

	if (tk) {
		track_info->id = tk->id;
		track_info->type = tk->type;
		track_info->duration =
			(tk->duration * 1000000 + tk->timescale / 2) /
			tk->timescale;
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
	} else {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
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
	struct mp4_track *tk = NULL;
	int found = 0;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps_size != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps_size != NULL, -EINVAL);

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

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


int mp4_demux_get_track_next_sample(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t *sample_buffer,
	unsigned int sample_buffer_size,
	uint8_t *metadata_buffer,
	unsigned int metadata_buffer_size,
	struct mp4_track_sample *track_sample)
{
	struct mp4_track *tk = NULL;
	int found = 0;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_sample != NULL, -EINVAL);

	memset(track_sample, 0, sizeof(*track_sample));

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

	if (tk->nextSample < tk->sampleCount) {
		track_sample->sample_size = tk->sampleSize[tk->nextSample];
		if ((sample_buffer) &&
			(tk->sampleSize[tk->nextSample] <=
			sample_buffer_size)) {
			int _ret = fseeko(demux->file,
				tk->sampleOffset[tk->nextSample], SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIu64
				" bytes forward in file",
				tk->sampleOffset[tk->nextSample]);
			size_t count = fread(sample_buffer,
				tk->sampleSize[tk->nextSample],
				1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %d bytes from file",
				tk->sampleSize[tk->nextSample]);
		} else if (sample_buffer) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOBUFS,
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
				(metatk->sampleSize[tk->nextSample] <=
				metadata_buffer_size)) {
				int _ret = fseeko(demux->file,
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
					1, demux->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %d bytes from file",
					metatk->sampleSize[tk->nextSample]);
			}
		}
		track_sample->silent = ((tk->pendingSeekTime) &&
			(tk->sampleDecodingTime[tk->nextSample] <
				tk->pendingSeekTime)) ? 1 : 0;
		if (tk->sampleDecodingTime[tk->nextSample] >=
			tk->pendingSeekTime)
			tk->pendingSeekTime = 0;
		track_sample->sample_dts =
			(tk->sampleDecodingTime[tk->nextSample] * 1000000 +
			tk->timescale / 2) / tk->timescale;
		track_sample->next_sample_dts =
			(tk->nextSample < tk->sampleCount - 1) ?
			(tk->sampleDecodingTime[tk->nextSample + 1] *
			1000000 + tk->timescale / 2) / tk->timescale : 0;
		tk->nextSample++;
	}

	return 0;
}


int mp4_demux_seek_to_track_prev_sample(
	struct mp4_demux *demux,
	unsigned int track_id)
{
	struct mp4_track *tk = NULL;
	int found = 0, idx;
	uint64_t ts;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

	idx = (tk->nextSample >= 2) ? tk->nextSample - 2 : 0;
	ts = (tk->sampleDecodingTime[idx] * 1000000 + tk->timescale / 2) /
		tk->timescale;

	return mp4_demux_seek(demux, ts, 1);
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

	*chaptersCount = demux->chaptersCount;
	*chaptersTime = demux->chaptersTime;
	*chaptersName = demux->chaptersName;

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

	*count = demux->finalMetadataCount;
	*keys = demux->finalMetadataKey;
	*values = demux->finalMetadataValue;

	return 0;
}


int mp4_demux_get_metadata_cover(
	struct mp4_demux *demux,
	uint8_t *cover_buffer,
	unsigned int cover_buffer_size,
	unsigned int *cover_size,
	enum mp4_metadata_cover_type *cover_type)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	if (demux->finalCoverSize > 0) {
		if (cover_size)
			*cover_size = demux->finalCoverSize;
		if (cover_type)
			*cover_type = demux->finalCoverType;
		if ((cover_buffer) &&
			(demux->finalCoverSize <= cover_buffer_size)) {
			int _ret = fseeko(demux->file,
				demux->finalCoverOffset, SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIi64
				" bytes forward in file",
				(int64_t)demux->finalCoverOffset);
			size_t count = fread(cover_buffer,
				demux->finalCoverSize, 1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %" PRIu32 " bytes from file",
				demux->finalCoverSize);
		} else if (cover_buffer) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOBUFS,
				"buffer too small (%d bytes, %d needed)",
				cover_buffer_size, demux->finalCoverSize);
		}
	} else {
		*cover_size = 0;
	}

	return 0;
}
