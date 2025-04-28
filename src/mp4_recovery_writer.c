/**
 * Copyright (c) 2023 Parrot Drones SAS
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


#define RECOVERY_WRITE_VAL(_val)                                               \
	do {                                                                   \
		err = write(mux->recovery.fd_tables, &_val, sizeof(_val));     \
		if (err < 0) {                                                 \
			ret = -errno;                                          \
			ULOG_ERRNO("write", errno);                            \
			goto out;                                              \
		} else if (err != (ssize_t)sizeof(_val)) {                     \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("write", -ret);                             \
			goto out;                                              \
		}                                                              \
	} while (0)


#define RECOVERY_WRITE_ARR(_val, _size)                                        \
	do {                                                                   \
		RECOVERY_WRITE_VAL(_size);                                     \
		if (_size == 0)                                                \
			break;                                                 \
		err = write(mux->recovery.fd_tables, _val, _size);             \
		if (err < 0) {                                                 \
			ret = -errno;                                          \
			ULOG_ERRNO("write", errno);                            \
			goto out;                                              \
		} else if (err != (ssize_t)_size) {                            \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("write", -ret);                             \
			goto out;                                              \
		}                                                              \
	} while (0)


static int mp4_mux_recovery_write_box_info(struct mp4_mux *mux,
					   uint32_t track_handle,
					   uint32_t type,
					   uint32_t number)
{
	int ret = 0;
	ssize_t err = 0;

	RECOVERY_WRITE_VAL(track_handle);
	RECOVERY_WRITE_VAL(type);
	RECOVERY_WRITE_VAL(number);

out:
	return ret;
}


static int
mp4_mux_recovery_write_audio_specific_config(struct mp4_mux *mux,
					     struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;

	/* audio codec */
	val32 = (uint32_t)track->audio.codec;
	RECOVERY_WRITE_VAL(val32);

	/* audio specific config */
	val32 = track->audio.specific_config_size;
	RECOVERY_WRITE_ARR(track->audio.specific_config, val32);

	/* channel count */
	val32 = track->audio.channel_count;
	RECOVERY_WRITE_VAL(val32);

	/* sample size */
	val32 = track->audio.sample_size;
	RECOVERY_WRITE_VAL(val32);

	/* sample rate */
	val32 = track->audio.sample_rate;
	RECOVERY_WRITE_VAL(val32);

out:
	return ret;
}


static int mp4_mux_recovery_write_vdec(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;

	val32 = track->video.codec == MP4_VIDEO_CODEC_AVC ? MP4_AVC1 : MP4_HVC1;
	RECOVERY_WRITE_VAL(val32);

	switch (track->video.codec) {
	case MP4_VIDEO_CODEC_AVC:
		RECOVERY_WRITE_ARR(track->video.avc.sps,
				   track->video.avc.sps_size);
		RECOVERY_WRITE_ARR(track->video.avc.pps,
				   track->video.avc.pps_size);
		break;
	case MP4_VIDEO_CODEC_HEVC:
		RECOVERY_WRITE_ARR(track->video.hevc.sps,
				   track->video.hevc.sps_size);
		RECOVERY_WRITE_ARR(track->video.hevc.pps,
				   track->video.hevc.pps_size);
		RECOVERY_WRITE_ARR(track->video.hevc.vps,
				   track->video.hevc.vps_size);
		break;
	default:
		ULOGE("invalid video codec %d", track->video.codec);
		return -EINVAL;
	}
	RECOVERY_WRITE_VAL(track->video.width);
	RECOVERY_WRITE_VAL(track->video.height);

out:
	return ret;
}


static int mp4_mux_recovery_write_metadata_stsd(struct mp4_mux *mux,
						struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t encoding_len = 0;
	uint32_t mime_len = 0;

	if (track->metadata.content_encoding != NULL)
		encoding_len = strlen(track->metadata.content_encoding);
	if (track->metadata.mime_type != NULL)
		mime_len = strlen(track->metadata.mime_type);

	/* content encoding */
	RECOVERY_WRITE_ARR(track->metadata.content_encoding, encoding_len);

	/* mime format */
	RECOVERY_WRITE_ARR(track->metadata.mime_type, mime_len);

out:
	return ret;
}


static int mp4_mux_recovery_write_stsd(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux, track->handle, MP4_SAMPLE_DESCRIPTION_BOX, 1);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	switch (track->type) {
	case MP4_TRACK_TYPE_VIDEO:
		ret = mp4_mux_recovery_write_vdec(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_box_info", -ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_AUDIO:
		ret = mp4_mux_recovery_write_audio_specific_config(mux, track);
		if (ret < 0) {
			ULOG_ERRNO(
				"mp4_mux_recovery_write_audio_specific_config",
				-ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_METADATA:
		ret = mp4_mux_recovery_write_metadata_stsd(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_metadata_stsd",
				   -ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_CHAPTERS:
		break;
	default:
		ULOGE("invalid track type %d", track->type);
		return -EINVAL;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stco(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	bool co64 = track->chunks.offsets[track->chunks.count - 1] > UINT32_MAX;

	err = mp4_mux_recovery_write_box_info(
		mux,
		track->handle,
		co64 ? MP4_CHUNK_OFFSET_64_BOX : MP4_CHUNK_OFFSET_BOX,
		track->chunks.count - track->stbl_index_write_count.chunks);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	for (uint32_t i = track->stbl_index_write_count.chunks;
	     i < track->chunks.count;
	     i++) {
		/* 64 bits written whether it's co or co64 */
		RECOVERY_WRITE_VAL(track->chunks.offsets[i]);

		track->stbl_index_write_count.chunks++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stsz(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux,
		track->handle,
		MP4_SAMPLE_SIZE_BOX,
		track->samples.count - track->stbl_index_write_count.samples);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	for (uint32_t i = track->stbl_index_write_count.samples;
	     i < track->samples.count;
	     i++) {
		/* 'entry size' */
		RECOVERY_WRITE_VAL(track->samples.sizes[i]);

		/* 'entry offset' */
		RECOVERY_WRITE_VAL(track->samples.offsets[i]);

		/* 'entry decoding time' */
		RECOVERY_WRITE_VAL(track->samples.decoding_times[i]);

		track->stbl_index_write_count.samples++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stsc(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux,
		track->handle,
		MP4_SAMPLE_TO_CHUNK_BOX,
		track->sample_to_chunk.count -
			track->stbl_index_write_count.sample_to_chunk);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	for (uint32_t i = track->stbl_index_write_count.sample_to_chunk;
	     i < track->sample_to_chunk.count;
	     i++) {
		struct mp4_sample_to_chunk_entry *entry;
		entry = &track->sample_to_chunk.entries[i];

		/* 'first_chunk' */
		val32 = entry->firstChunk;
		RECOVERY_WRITE_VAL(val32);

		/* 'samples_per_chunk' */
		val32 = entry->samplesPerChunk;
		RECOVERY_WRITE_VAL(val32);

		/* 'sample_description_id' */
		val32 = entry->sampleDescriptionIndex;
		RECOVERY_WRITE_VAL(val32);

		track->stbl_index_write_count.sample_to_chunk++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stss(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux,
		track->handle,
		MP4_SYNC_SAMPLE_BOX,
		track->sync.count - track->stbl_index_write_count.sync);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	for (uint32_t i = track->stbl_index_write_count.sync;
	     i < track->sync.count;
	     i++) {
		/* 'sample_number' */
		val32 = track->sync.entries[i];
		RECOVERY_WRITE_VAL(val32);

		track->stbl_index_write_count.sync++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stts(struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux,
		track->handle,
		MP4_DECODING_TIME_TO_SAMPLE_BOX,
		track->time_to_sample.count -
			track->stbl_index_write_count.time_to_sample);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	for (uint32_t i = track->stbl_index_write_count.time_to_sample;
	     i < track->time_to_sample.count;
	     i++) {
		struct mp4_time_to_sample_entry *entry;
		entry = &track->time_to_sample.entries[i];

		/* 'sample_count' */
		val32 = entry->sampleCount;
		RECOVERY_WRITE_VAL(val32);

		/* 'sample_delta' */
		val32 = entry->sampleDelta;
		RECOVERY_WRITE_VAL(val32);

		track->stbl_index_write_count.time_to_sample++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_thumb(struct mp4_mux *mux)
{
	int ret = 0;
	ssize_t err = 0;

	err = mp4_mux_recovery_write_box_info(
		mux, 0, MP4_METADATA_TAG_TYPE_COVER, 1);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	/* cover type */
	RECOVERY_WRITE_VAL(mux->file_metadata.cover_type);

	/* cover */
	RECOVERY_WRITE_ARR(mux->file_metadata.cover,
			   mux->file_metadata.cover_size);

out:
	return ret;
}


static int mp4_mux_recovery_write_track(struct mp4_mux *mux,
					struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t len_name = track->name == NULL ? 0 : strlen(track->name);

	err = mp4_mux_recovery_write_box_info(
		mux, track->handle, MP4_TRACK_BOX, 1);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	RECOVERY_WRITE_VAL(track->type);
	RECOVERY_WRITE_ARR(track->name, len_name);
	RECOVERY_WRITE_VAL(track->flags);
	RECOVERY_WRITE_VAL(track->timescale);
	RECOVERY_WRITE_VAL(track->creation_time);
	RECOVERY_WRITE_VAL(track->modification_time);
	RECOVERY_WRITE_ARR(track->referenceTrackHandle,
			   track->referenceTrackHandleCount);

	track->track_info_written = true;
out:
	return ret;
}


static int mp4_mux_recovery_write_meta(struct mp4_mux *mux,
				       struct mp4_mux_metadata *meta,
				       uint32_t track_handle)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;

	ret = mp4_mux_recovery_write_box_info(
		mux, track_handle, MP4_META_BOX, 1);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", -ret);
		goto out;
	}

	/* storage */
	val32 = meta->storage;
	RECOVERY_WRITE_VAL(val32);

	/* key */
	val32 = strlen(meta->key);
	RECOVERY_WRITE_ARR(meta->key, val32);

	/* value */
	val32 = strlen(meta->value);
	RECOVERY_WRITE_ARR(meta->value, val32);

out:
	return ret;
}


int mp4_mux_incremental_sync(struct mp4_mux *mux)
{
	int ret = 0;
	struct mp4_mux_track *track;
	struct mp4_mux_metadata *meta;
	uint32_t meta_count = 0;
	uint32_t track_meta_count = 0;

	list_walk_entry_forward(&mux->tracks, track, node)
	{
		/* write only once */
		if (!track->track_info_written) {
			ret = mp4_mux_recovery_write_track(mux, track);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_recovery_write_track",
					   -ret);
				goto out;
			}
			ret = mp4_mux_recovery_write_stsd(mux, track);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_recovery_write_stsd", -ret);
				goto out;
			}
		}

		/* Skip empty tracks */
		if (track->samples.count == 0)
			continue;

		ret = mp4_mux_recovery_write_stts(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_stts", -ret);
			goto out;
		}
		ret = mp4_mux_recovery_write_stss(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_stss", -ret);
			goto out;
		}
		ret = mp4_mux_recovery_write_stsc(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_stsc", -ret);
			goto out;
		}
		ret = mp4_mux_recovery_write_stsz(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_stsz", -ret);
			goto out;
		}
		ret = mp4_mux_recovery_write_stco(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_stco", -ret);
			goto out;
		}

		/* Metadata */
		list_walk_entry_forward(&track->metadatas, meta, node)
		{
			track_meta_count++;
			if (track_meta_count < track->meta_write_count)
				continue;

			ret = mp4_mux_recovery_write_meta(
				mux, meta, track->handle);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_recovery_write_meta", -ret);
				goto out;
			}
		}
		track->meta_write_count = track_meta_count + 1;
	}

	/* Metadata */
	list_walk_entry_forward(&mux->metadatas, meta, node)
	{
		meta_count++;
		if (meta_count < mux->recovery.meta_write_count)
			continue;

		ret = mp4_mux_recovery_write_meta(mux, meta, 0);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_meta", -ret);
			goto out;
		}
	}
	mux->recovery.meta_write_count = meta_count + 1;

	/* thumbnail */
	if (!mux->recovery.thumb_written && mux->file_metadata.cover != NULL) {
		mp4_mux_recovery_write_thumb(mux);
		mux->recovery.thumb_written = true;
	}

out:
	return ret;
}
