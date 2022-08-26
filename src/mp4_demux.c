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

#include "mp4_priv.h"


static int mp4_metadata_build(struct mp4_file *mp4)
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

	if ((mp4->udtaLocationValue) && (strlen(mp4->udtaLocationValue) > 0) &&
	    (mp4->udtaLocationKey) && (strlen(mp4->udtaLocationKey) > 0))
		xyzCount++;

	mp4->finalMetadataCount = metaCount + udtaCount + xyzCount;

	mp4->finalMetadataKey = calloc(mp4->finalMetadataCount, sizeof(char *));
	if (mp4->finalMetadataKey == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	mp4->finalMetadataValue =
		calloc(mp4->finalMetadataCount, sizeof(char *));
	if (mp4->finalMetadataValue == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	for (i = 0; i < mp4->metaMetadataCount; i++) {
		if ((mp4->metaMetadataValue[i]) &&
		    (strlen(mp4->metaMetadataValue[i]) > 0) &&
		    (mp4->metaMetadataKey[i]) &&
		    (strlen(mp4->metaMetadataKey[i]) > 0)) {
			mp4->finalMetadataKey[k] = mp4->metaMetadataKey[i];
			mp4->finalMetadataValue[k] = mp4->metaMetadataValue[i];
			k++;
		}
	}

	for (i = 0; i < mp4->udtaMetadataCount; i++) {
		if ((mp4->udtaMetadataValue[i]) &&
		    (strlen(mp4->udtaMetadataValue[i]) > 0) &&
		    (mp4->udtaMetadataKey[i]) &&
		    (strlen(mp4->udtaMetadataKey[i]) > 0)) {
			mp4->finalMetadataKey[k] = mp4->udtaMetadataKey[i];
			mp4->finalMetadataValue[k] = mp4->udtaMetadataValue[i];
			k++;
		}
	}

	if ((mp4->udtaLocationValue) && (strlen(mp4->udtaLocationValue) > 0) &&
	    (mp4->udtaLocationKey) && (strlen(mp4->udtaLocationKey) > 0)) {
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


int mp4_demux_open(const char *filename, struct mp4_demux **ret_obj)
{
	int ret;
	ptrdiff_t retBytes;
	struct mp4_demux *demux;
	struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(filename == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(filename) == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);

	demux = calloc(sizeof(*demux), 1);
	if (demux == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}
	mp4 = &demux->mp4;
	list_init(&mp4->tracks);

	mp4->file = fopen(filename, "rb");
	if (mp4->file == NULL) {
		ret = -errno;
		ULOG_ERRNO("fopen:'%s'", -ret, filename);
		goto error;
	}

	ret = fseeko(mp4->file, 0, SEEK_END);
	if (ret != 0) {
		ret = -errno;
		ULOG_ERRNO("fseeko", -ret);
		goto error;
	}
	mp4->fileSize = ftello(mp4->file);
	if (mp4->fileSize < 0) {
		ret = -errno;
		ULOG_ERRNO("ftello", -ret);
		goto error;
	} else if (mp4->fileSize == 0) {
		ret = -ENODATA;
		ULOGW("empty file: '%s'", filename);
		goto error;
	}
	ret = fseeko(mp4->file, 0, SEEK_SET);
	if (ret != 0) {
		ret = -errno;
		ULOG_ERRNO("fseeko", -ret);
		goto error;
	}

	mp4->root = mp4_box_new(NULL);
	if (mp4->root == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("mp4_box_new", -ret);
		goto error;
	}
	mp4->root->type = MP4_ROOT_BOX;
	mp4->root->size = 1;
	mp4->root->largesize = mp4->fileSize;

	retBytes = mp4_box_children_read(mp4, mp4->root, mp4->fileSize, NULL);
	if (retBytes < 0) {
		ret = retBytes;
		goto error;
	}
	mp4->readBytes += retBytes;

	ret = mp4_tracks_build(mp4);
	if (ret < 0)
		goto error;

	ret = mp4_metadata_build(mp4);
	if (ret < 0)
		goto error;

	mp4_box_log(mp4->root, 7); // ULOG_DEBUG = 7

	*ret_obj = demux;
	return 0;

error:
	mp4_demux_close(demux);
	*ret_obj = NULL;
	return ret;
}


int mp4_demux_close(struct mp4_demux *demux)
{
	if (demux == NULL)
		return 0;

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


static int
get_seek_sample(struct mp4_track *tk, int start, enum mp4_seek_method method)
{
	int is_sync, prev_sync = -1, next_sync;
	uint64_t ts = tk->sampleDecodingTime[start], prev_ts, next_ts;

	switch (method) {
	case MP4_SEEK_METHOD_PREVIOUS:
		return start;
	case MP4_SEEK_METHOD_PREVIOUS_SYNC:
		is_sync = mp4_track_is_sync_sample(tk, start, &prev_sync);
		if (is_sync)
			return start;
		else if (prev_sync >= 0)
			return prev_sync;
		else
			return -ENOENT;
	case MP4_SEEK_METHOD_NEXT_SYNC:
		is_sync = mp4_track_is_sync_sample(tk, start, &prev_sync);
		next_sync = mp4_track_find_sample_by_time(
			tk, ts, MP4_TIME_CMP_GT, 1, start);
		if (is_sync)
			return start;
		else if (next_sync >= 0)
			return next_sync;
		else
			return -ENOENT;
	case MP4_SEEK_METHOD_NEAREST_SYNC:
		is_sync = mp4_track_is_sync_sample(tk, start, &prev_sync);
		next_sync = mp4_track_find_sample_by_time(
			tk, ts, MP4_TIME_CMP_GT, 1, start);
		if (is_sync) {
			return start;
		} else if (prev_sync >= 0 && next_sync >= 0) {
			prev_ts = tk->sampleDecodingTime[prev_sync];
			next_ts = tk->sampleDecodingTime[next_sync];
			if (ts - prev_ts > next_ts - ts)
				return next_sync;
			else
				return prev_sync;
		} else if (prev_sync >= 0) {
			return prev_sync;
		} else if (next_sync >= 0) {
			return next_sync;
		} else {
			return -ENOENT;
		}
	default:
		ULOGE("unsupported seek method: %d", method);
		return -EINVAL;
	}
}


int mp4_demux_seek(struct mp4_demux *demux,
		   uint64_t time_offset,
		   enum mp4_seek_method method,
		   int *seekedToFrame)
{
	struct mp4_track *tk = NULL;
	struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	struct list_node *start = &mp4->tracks;
	custom_walk(start, tk, node, struct mp4_track)
	{
		if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
			continue;

		int found = 0, i, idx = 0;
		uint64_t ts =
			mp4_usec_to_sample_time(time_offset, tk->timescale);
		uint64_t newPendingSeekTime = 0;
		int start = (unsigned int)(((uint64_t)tk->sampleCount * ts +
					    tk->duration - 1) /
					   tk->duration);
		if (start < 0)
			start = 0;
		if ((unsigned)start >= tk->sampleCount) 
		{
			// start = tk->sampleCount - 1;
			return -ENFILE;
		}
		while (((unsigned)start < tk->sampleCount - 1) &&
		       (tk->sampleDecodingTime[start] < ts))
			start++;
		for (i = start; i >= 0; i--) {
			if (tk->sampleDecodingTime[i] <= ts) {
				idx = get_seek_sample(tk, i, method);
				if (idx < 0)
					break;
				newPendingSeekTime =
					(idx == i) ? 0
						   : tk->sampleDecodingTime[i];
				found = 1;
				break;
			}
		}
		if (found) {
			tk->nextSample = idx;
			tk->pendingSeekTime = newPendingSeekTime;
			ULOGD("seek to %" PRIu64 " -> sample #%d time %" PRIu64,
			      time_offset,
			      idx,
			      mp4_sample_time_to_usec(
				      tk->sampleDecodingTime[idx],
				      tk->timescale));
			*seekedToFrame = idx;
			if (tk->metadata) {
				if (((unsigned)idx <
				     tk->metadata->sampleCount) &&
				    (tk->sampleDecodingTime[idx] ==
				     tk->metadata->sampleDecodingTime[idx]))
					tk->metadata->nextSample = idx;
				else
					ULOGW("failed to sync metadata"
					      " with ref track");
			}
		} else {
			ULOGE("unable to seek in track");
			return -ENOENT;
		}
	}

	return 0;
}

int mp4_demux_seek_jpeg(struct mp4_demux *demux,
		   uint64_t time_offset,
		   enum mp4_seek_method method,
		   int *seekedToFrame)
{
	struct mp4_track *tk = NULL;
	struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	struct list_node *start = &mp4->tracks;
	custom_walk(start, tk, node, struct mp4_track)
	{
		if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
			continue;

		int found = 0, i, idx = 0;
		uint64_t ts =
			mp4_usec_to_sample_time(time_offset, tk->timescale);
		uint64_t newPendingSeekTime = 0;
		int start = (unsigned int)(((uint64_t)tk->sampleCount * ts +
					    tk->duration - 1) /
					   tk->duration);
		if (start < 0)
			start = 0;
		if ((unsigned)start >= tk->sampleCount) {
			// start = tk->sampleCount - 1;
			return -ENFILE;
		}
		while (((unsigned)start < tk->sampleCount - 1) &&
		       (tk->sampleDecodingTime[start] < ts))
			start++;
		for (i = start; i >= 0; i--) {
			if (tk->sampleDecodingTime[i] <= ts) {
				idx = get_seek_sample(tk, i, method);
				// Note - every frame is sync in jpeg, so we want the next frame for MP4_SEEK_METHOD_NEXT_SYNC
				if (method == MP4_SEEK_METHOD_NEXT_SYNC) {
					idx += 1;
				}
				if (idx < 0)
					break;
				newPendingSeekTime =
					(idx == i) ? 0
						   : tk->sampleDecodingTime[i];
				found = 1;
				break;
			}
		}
		if (found) {
			tk->nextSample = idx;
			tk->pendingSeekTime = newPendingSeekTime;
			ULOGD("seek to %" PRIu64 " -> sample #%d time %" PRIu64,
			      time_offset,
			      idx,
			      mp4_sample_time_to_usec(
				      tk->sampleDecodingTime[idx],
				      tk->timescale));
			*seekedToFrame = idx;
			if (tk->metadata) {
				if (((unsigned)idx <
				     tk->metadata->sampleCount) &&
				    (tk->sampleDecodingTime[idx] ==
				     tk->metadata->sampleDecodingTime[idx]))
					tk->metadata->nextSample = idx;
				else
					ULOGW("failed to sync metadata"
					      " with ref track");
			}
		} else {
			ULOGE("unable to seek in track");
			return -ENOENT;
		}
	}

	return 0;
}


int mp4_demux_get_media_info(struct mp4_demux *demux,
			     struct mp4_media_info *media_info)
{
	struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_info == NULL, EINVAL);

	mp4 = &demux->mp4;

	memset(media_info, 0, sizeof(*media_info));

	media_info->duration =
		mp4_sample_time_to_usec(mp4->duration, mp4->timescale);
	media_info->creation_time =
		mp4->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->modification_time =
		mp4->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->track_count = mp4->trackCount;

	return 0;
}


int mp4_demux_get_track_count(struct mp4_demux *demux)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	return demux->mp4.trackCount;
}


int mp4_demux_get_track_info(struct mp4_demux *demux,
			     unsigned int track_idx,
			     struct mp4_track_info *track_info)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_info == NULL, EINVAL);

	mp4 = &demux->mp4;

	ULOG_ERRNO_RETURN_ERR_IF(track_idx >= mp4->trackCount, EINVAL);

	tk = mp4_track_find_by_idx(mp4, track_idx);
	if (tk == NULL) {
		ULOGE("track index=%d not found", track_idx);
		return -ENOENT;
	}

	memset(track_info, 0, sizeof(*track_info));
	track_info->id = tk->id;
	track_info->name = tk->name;
	track_info->enabled = tk->enabled;
	track_info->in_movie = tk->in_movie;
	track_info->in_preview = tk->in_preview;
	track_info->type = tk->type;
	track_info->timescale = tk->timescale;
	track_info->duration = tk->duration;
	track_info->creation_time =
		tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	track_info->modification_time =
		tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	track_info->sample_count = tk->sampleCount;
	track_info->sample_max_size = tk->sampleMaxSize;
	track_info->sample_offsets = tk->sampleOffset;
	track_info->sample_sizes = tk->sampleSize;
	track_info->has_metadata = (tk->metadata) ? 1 : 0;
	if (tk->metadata) {
		track_info->metadata_content_encoding =
			tk->metadata->contentEncoding;
		track_info->metadata_mime_format = tk->metadata->mimeFormat;
	}
	if (tk->type == MP4_TRACK_TYPE_METADATA) {
		track_info->content_encoding = tk->contentEncoding;
		track_info->mime_format = tk->mimeFormat;
	} else if (tk->type == MP4_TRACK_TYPE_VIDEO) {
		track_info->video_codec = tk->vdc.codec;
		track_info->video_width = tk->vdc.width;
		track_info->video_height = tk->vdc.height;
	} else if (tk->type == MP4_TRACK_TYPE_AUDIO) {
		track_info->audio_codec = tk->audioCodec;
		track_info->audio_channel_count = tk->audioChannelCount;
		track_info->audio_sample_size = tk->audioSampleSize;
		track_info->audio_sample_rate =
			(float)tk->audioSampleRate / 65536.;
	}

	return 0;
}


int mp4_demux_get_track_video_decoder_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	struct mp4_video_decoder_config *vdc)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}
	if (tk->type != MP4_TRACK_TYPE_VIDEO) {
		ULOGE("track id=%d is not of video type", track_id);
		return -EINVAL;
	}

	vdc->width = tk->vdc.width;
	vdc->height = tk->vdc.height;

	switch (tk->vdc.codec) {
	case MP4_VIDEO_CODEC_HEVC:
		vdc->codec = MP4_VIDEO_CODEC_HEVC;
		vdc->hevc.hvcc_info = tk->vdc.hevc.hvcc_info;
		if (tk->vdc.hevc.vps) {
			vdc->hevc.vps = tk->vdc.hevc.vps;
			vdc->hevc.vps_size = tk->vdc.hevc.vps_size;
		}
		if (tk->vdc.hevc.sps) {
			vdc->hevc.sps = tk->vdc.hevc.sps;
			vdc->hevc.sps_size = tk->vdc.hevc.sps_size;
		}
		if (tk->vdc.hevc.pps) {
			vdc->hevc.pps = tk->vdc.hevc.pps;
			vdc->hevc.pps_size = tk->vdc.hevc.pps_size;
		}
		break;
	case MP4_VIDEO_CODEC_AVC:
		vdc->codec = MP4_VIDEO_CODEC_AVC;
		if (tk->vdc.avc.sps) {
			vdc->avc.sps = tk->vdc.avc.sps;
			vdc->avc.sps_size = tk->vdc.avc.sps_size;
		}
		if (tk->vdc.avc.pps) {
			vdc->avc.pps = tk->vdc.avc.pps;
			vdc->avc.pps_size = tk->vdc.avc.pps_size;
		}
		break;
	default:
		ULOGE("track id=%d video codec is neither AVC nor HEVC",
		      track_id);
		return -EINVAL;
	}

	return 0;
}


int mp4_demux_get_track_audio_specific_config(struct mp4_demux *demux,
					      unsigned int track_id,
					      uint8_t **audio_specific_config,
					      unsigned int *asc_size)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	if (audio_specific_config != NULL)
		*audio_specific_config = NULL;
	if (asc_size != NULL)
		*asc_size = 0;

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}
	if (tk->type != MP4_TRACK_TYPE_AUDIO) {
		ULOGE("track id=%d is not of audio type", track_id);
		return -EINVAL;
	}

	if (tk->audioSpecificConfig == NULL) {
		ULOGE("track does not have an AudioSpecificConfig");
		return -EPROTO;
	}

	if (audio_specific_config != NULL)
		*audio_specific_config = tk->audioSpecificConfig;

	if (asc_size != NULL)
		*asc_size = tk->audioSpecificConfigSize;

	return 0;
}


int mp4_demux_get_track_sample(struct mp4_demux *demux,
			       unsigned int track_id,
			       int advance,
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
	uint32_t sample_size, metadata_size;
	uint64_t sample_offset, metadata_offset;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_sample == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	memset(track_sample, 0, sizeof(*track_sample));

	if (tk->nextSample >= tk->sampleCount)
		return 0;

	sample_size = tk->sampleSize[tk->nextSample];
	sample_offset = tk->sampleOffset[tk->nextSample];
	track_sample->size = sample_size;
	if ((sample_buffer) && (sample_size > 0) &&
	    (sample_size <= sample_buffer_size)) {
		int _ret = fseeko(mp4->file, sample_offset, SEEK_SET);
		if (_ret != 0) {
			ULOG_ERRNO("fseeko", errno);
			return -errno;
		}
		size_t count = fread(sample_buffer, sample_size, 1, mp4->file);
		if (count != 1) {
			track_sample->size = 0;
			sample_size = 0;
			_ret = -errno;
			if (_ret == 0)
				_ret = -ENODATA;
			ULOG_ERRNO("fread", -_ret);
			return _ret;
		}
	} else if ((sample_buffer) && (sample_size > sample_buffer_size)) {
		ULOGE("buffer too small (%d bytes, %d needed)",
		      sample_buffer_size,
		      sample_size);
		return -ENOBUFS;
	}
	if ((tk->metadata) && (tk->nextSample < tk->metadata->sampleCount)) {
		struct mp4_track *metatk = tk->metadata;
		/* TODO: check sync between metadata and ref track */
		metadata_size = metatk->sampleSize[tk->nextSample];
		metadata_offset = metatk->sampleOffset[tk->nextSample];
		track_sample->metadata_size = metadata_size;
		if ((metadata_buffer) && (metadata_size > 0) &&
		    (metadata_size <= metadata_buffer_size)) {
			int _ret = fseeko(mp4->file, metadata_offset, SEEK_SET);
			if (_ret != 0) {
				ULOG_ERRNO("fseeko", errno);
				return -errno;
			}
			size_t count = fread(
				metadata_buffer, metadata_size, 1, mp4->file);
			if (count != 1) {
				track_sample->metadata_size = 0;
				_ret = -errno;
				if (_ret == 0)
					_ret = -ENODATA;
				ULOG_ERRNO("fread", -_ret);
				return _ret;
			}
		} else if ((metadata_buffer) &&
			   (metadata_size > metadata_buffer_size)) {
			ULOGE("buffer too small for metadata "
			      "(%d bytes, %d needed)",
			      metadata_buffer_size,
			      metadata_size);
			return -ENOBUFS;
		}
	}
	sampleTime = tk->sampleDecodingTime[tk->nextSample];
	track_sample->silent =
		((tk->pendingSeekTime) && (sampleTime < tk->pendingSeekTime));
	if (sampleTime >= tk->pendingSeekTime)
		tk->pendingSeekTime = 0;
	track_sample->dts = sampleTime;
	track_sample->next_dts =
		(tk->nextSample < tk->sampleCount - 1)
			? tk->sampleDecodingTime[tk->nextSample + 1]
			: 0;
	idx = mp4_track_find_sample_by_time(
		tk, sampleTime, MP4_TIME_CMP_LT, 1, tk->nextSample);
	if (idx >= 0)
		track_sample->prev_sync_dts = tk->sampleDecodingTime[idx];
	idx = mp4_track_find_sample_by_time(
		tk, sampleTime, MP4_TIME_CMP_GT, 1, tk->nextSample);
	if (idx >= 0)
		track_sample->next_sync_dts = tk->sampleDecodingTime[idx];
	track_sample->sync = mp4_track_is_sync_sample(tk, tk->nextSample, NULL);

	if (advance)
		tk->nextSample++;

	return 0;
}


int mp4_demux_seek_to_track_prev_sample(struct mp4_demux *demux,
					unsigned int track_id)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	idx = (tk->nextSample >= 2) ? tk->nextSample - 2 : 0;
	ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
				     tk->timescale);
	int seekedToFrame;
	return mp4_demux_seek(demux, ts, MP4_SEEK_METHOD_PREVIOUS_SYNC, &seekedToFrame);
}


int mp4_demux_seek_to_track_next_sample(struct mp4_demux *demux,
					unsigned int track_id)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	idx = (tk->nextSample < tk->sampleCount - 1) ? tk->nextSample + 1 : 0;
	ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
				     tk->timescale);

	int seekedToFrame;
	return mp4_demux_seek(demux, ts, MP4_SEEK_METHOD_PREVIOUS, &seekedToFrame);
}


int mp4_demux_get_track_prev_sample_time(struct mp4_demux *demux,
					 unsigned int track_id,
					 uint64_t *sample_time)
{
	int ret = 0;
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	uint64_t prev_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	if (tk->nextSample >= 2) {
		prev_ts = mp4_sample_time_to_usec(
			tk->sampleDecodingTime[tk->nextSample - 2],
			tk->timescale);
	} else {
		ret = -ENOENT;
	}

exit:
	*sample_time = prev_ts;
	return ret;
}


int mp4_demux_get_track_next_sample_time(struct mp4_demux *demux,
					 unsigned int track_id,
					 uint64_t *sample_time)
{
	int ret = 0;
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	uint64_t next_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	if (tk->nextSample < tk->sampleCount) {
		next_ts = mp4_sample_time_to_usec(
			tk->sampleDecodingTime[tk->nextSample], tk->timescale);
	} else {
		ret = -ENOENT;
	}

exit:
	*sample_time = next_ts;
	return ret;
}


static int mp4_demux_get_track_sample_time(struct mp4_demux *demux,
					   unsigned int track_id,
					   uint64_t time,
					   int sync,
					   enum mp4_time_cmp cmp,
					   uint64_t *sample_time)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx, ret;
	uint64_t ts = 0, sample_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	ts = mp4_usec_to_sample_time(time, tk->timescale);
	idx = mp4_track_find_sample_by_time(tk, ts, cmp, sync, -1);

	if (idx >= 0) {
		sample_ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
						    tk->timescale);
		ret = 0;
	} else {
		ULOGE("no sample found for the requested time");
		ret = -ENOENT;
	}

exit:
	*sample_time = sample_ts;
	return ret;
}

int mp4_demux_get_track_prev_sample_time_before(struct mp4_demux *demux,
						unsigned int track_id,
						uint64_t time,
						int sync,
						uint64_t *sample_time)
{
	return mp4_demux_get_track_sample_time(
		demux, track_id, time, sync, MP4_TIME_CMP_LT, sample_time);
}


int mp4_demux_get_track_next_sample_time_after(struct mp4_demux *demux,
					       unsigned int track_id,
					       uint64_t time,
					       int sync,
					       uint64_t *sample_time)
{
	return mp4_demux_get_track_sample_time(
		demux, track_id, time, sync, MP4_TIME_CMP_GT, sample_time);
}


int mp4_demux_get_chapters(struct mp4_demux *demux,
			   unsigned int *chapters_count,
			   uint64_t **chapters_time,
			   char ***chapters_name)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_time == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_name == NULL, EINVAL);

	*chapters_count = demux->mp4.chaptersCount;
	*chapters_time = demux->mp4.chaptersTime;
	*chapters_name = demux->mp4.chaptersName;

	return 0;
}


int mp4_demux_get_metadata_strings(struct mp4_demux *demux,
				   unsigned int *count,
				   char ***keys,
				   char ***values)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(keys == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(values == NULL, EINVAL);

	*count = demux->mp4.finalMetadataCount;
	*keys = demux->mp4.finalMetadataKey;
	*values = demux->mp4.finalMetadataValue;

	return 0;
}


int mp4_demux_get_track_metadata_strings(struct mp4_demux *demux,
					 unsigned int track_id,
					 unsigned int *count,
					 char ***keys,
					 char ***values)
{
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(keys == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(values == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	*count = tk->staticMetadataCount;
	*keys = tk->staticMetadataKey;
	*values = tk->staticMetadataValue;

	return 0;
}


int mp4_demux_get_metadata_cover(struct mp4_demux *demux,
				 uint8_t *cover_buffer,
				 unsigned int cover_buffer_size,
				 unsigned int *cover_size,
				 enum mp4_metadata_cover_type *cover_type)
{
	struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover_size == NULL, EINVAL);

	mp4 = &demux->mp4;

	if (mp4->finalCoverSize > 0) {
		*cover_size = mp4->finalCoverSize;
		if (cover_type)
			*cover_type = mp4->finalCoverType;
		if ((cover_buffer) &&
		    (mp4->finalCoverSize <= cover_buffer_size)) {
			int _ret = fseeko(
				mp4->file, mp4->finalCoverOffset, SEEK_SET);
			if (_ret != 0) {
				ULOG_ERRNO("fseeko", errno);
				return -errno;
			}
			size_t count = fread(cover_buffer,
					     mp4->finalCoverSize,
					     1,
					     mp4->file);
			if (count != 1) {
				ULOG_ERRNO("fread", errno);
				return -errno;
			}
		} else if (cover_buffer) {
			ULOGE("buffer too small (%d bytes, %d needed)",
			      cover_buffer_size,
			      mp4->finalCoverSize);
			return -ENOBUFS;
		}
	} else {
		*cover_size = 0;
	}

	return 0;
}
