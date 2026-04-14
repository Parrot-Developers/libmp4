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
#ifdef _WIN32
#	include <windows.h>
#	define DWORD_MAX 0xFFFFFFFFUL
#endif

#define RECOVERY_WRITE_VAL(_fd, _val)                                          \
	do {                                                                   \
		err = write(_fd, &_val, sizeof(_val));                         \
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


#define RECOVERY_WRITE_ARR(_fd, _val, _size)                                   \
	do {                                                                   \
		RECOVERY_WRITE_VAL(_fd, _size);                                \
		if (_size == 0)                                                \
			break;                                                 \
		err = write(_fd, _val, _size);                                 \
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


#ifdef _WIN32
/* Only works if fd points at the end of the file */
static ssize_t pwrite_win32(int fd, const void *buf, size_t count, off_t offset)
{
	off_t err = 0;

	if (offset < 0 || count > DWORD_MAX) {
		errno = EINVAL;
		return -1;
	}

	HANDLE hFile = (HANDLE)_get_osfhandle(fd);
	if (hFile == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	OVERLAPPED ov = {0};
	ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
	ov.OffsetHigh = (DWORD)((uint64_t)offset >> 32);

	DWORD written = 0;

	BOOL ok = WriteFile(hFile, buf, (DWORD)count, NULL, &ov);
	if (!ok) {
		DWORD err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			errno = EIO;
			return -1;
		}
	}

	if (!GetOverlappedResult(hFile, &ov, &written, TRUE)) {
		errno = EIO;
		return -1;
	}

	/* Security in case file is not opened with proper flags */
	err = lseek(fd, 0, SEEK_END);
	if (err < 0) {
		written = -errno;
		ULOG_ERRNO("lseek", -written);
	}
	return (ssize_t)written;
}
#endif


static ssize_t mp4_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
#ifdef _WIN32
	/* Only works if fd points at the end of the file */
	return pwrite_win32(fd, buf, count, offset);
#else
	return pwrite(fd, buf, count, offset);
#endif
}


#define RECOVERY_PWRITE_VAL(_fd, _val, _offset)                                \
	do {                                                                   \
		err = mp4_pwrite(_fd, &_val, sizeof(_val), _offset);           \
		if (err < 0) {                                                 \
			ret = -errno;                                          \
			ULOG_ERRNO("pwrite", errno);                           \
			goto out;                                              \
		} else if (err != (ssize_t)sizeof(_val)) {                     \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("pwrite", -ret);                            \
			goto out;                                              \
		}                                                              \
		_offset += sizeof(_val);                                       \
	} while (0)


static int mp4_mux_recovery_write_box_info(const struct mp4_mux *mux,
					   uint32_t track_handle,
					   uint32_t type,
					   uint32_t number)
{
	int ret = 0;
	ssize_t err = 0;

	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track_handle);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, type);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, number);

out:
	return ret;
}


static int
mp4_mux_recovery_write_audio_specific_config(const struct mp4_mux *mux,
					     const struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;

	/* audio codec */
	val32 = (uint32_t)track->audio.codec;
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

	/* audio specific config */
	val32 = track->audio.specific_config_size;
	RECOVERY_WRITE_ARR(
		mux->recovery.fd_tables, track->audio.specific_config, val32);

	/* channel count */
	val32 = track->audio.channel_count;
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

	/* sample size */
	val32 = track->audio.sample_size;
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

	/* sample rate */
	val32 = track->audio.sample_rate;
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

out:
	return ret;
}


static int mp4_mux_recovery_write_vdec(const struct mp4_mux *mux,
				       const struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;

	val32 = track->video.codec == MP4_VIDEO_CODEC_AVC ? MP4_AVC1 : MP4_HVC1;
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

	switch (track->video.codec) {
	case MP4_VIDEO_CODEC_AVC:
		RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
				   track->video.avc.sps,
				   track->video.avc.sps_size);
		RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
				   track->video.avc.pps,
				   track->video.avc.pps_size);
		break;
	case MP4_VIDEO_CODEC_HEVC:
		RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
				   track->video.hevc.sps,
				   track->video.hevc.sps_size);
		RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
				   track->video.hevc.pps,
				   track->video.hevc.pps_size);
		RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
				   track->video.hevc.vps,
				   track->video.hevc.vps_size);
		break;
	default:
		ULOGE("invalid video codec %d", track->video.codec);
		return -EINVAL;
	}
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->video.width);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->video.height);

out:
	return ret;
}


static int
mp4_mux_recovery_write_metadata_stsd(const struct mp4_mux *mux,
				     const struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t encoding_len = 0;
	uint32_t mime_len = 0;

	encoding_len = (uint32_t)mp4_validate_str_len(
		track->metadata.content_encoding, METADATA_VALUE_MAX);
	mime_len = (uint32_t)mp4_validate_str_len(track->metadata.mime_type,
						  METADATA_VALUE_MAX);

	/* content encoding */
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
			   track->metadata.content_encoding,
			   encoding_len);

	/* mime format */
	RECOVERY_WRITE_ARR(
		mux->recovery.fd_tables, track->metadata.mime_type, mime_len);

out:
	return ret;
}


static int mp4_mux_recovery_write_stsd(const struct mp4_mux *mux,
				       const struct mp4_mux_track *track)
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


static int mp4_mux_recovery_write_stco(const struct mp4_mux *mux,
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
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables,
				   track->chunks.offsets[i]);

		track->stbl_index_write_count.chunks++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stsz(const struct mp4_mux *mux,
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
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables,
				   track->samples.sizes[i]);

		/* 'entry offset' */
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables,
				   track->samples.offsets[i]);

		/* 'entry decoding time' */
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables,
				   track->samples.decoding_times[i]);

		track->stbl_index_write_count.samples++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stsc(const struct mp4_mux *mux,
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
		const struct mp4_sample_to_chunk_entry *entry;
		entry = &track->sample_to_chunk.entries[i];

		/* 'first_chunk' */
		val32 = entry->firstChunk;
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		/* 'samples_per_chunk' */
		val32 = entry->samplesPerChunk;
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		/* 'sample_description_id' */
		val32 = entry->sampleDescriptionIndex;
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		track->stbl_index_write_count.sample_to_chunk++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stss(const struct mp4_mux *mux,
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
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		track->stbl_index_write_count.sync++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_stts(const struct mp4_mux *mux,
				       struct mp4_mux_track *track)
{
	uint32_t val32;
	int ret = 0;
	ssize_t err = 0;
	struct mp4_time_to_sample_entry entry;

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
		entry = track->time_to_sample.entries[i];

		/* 'sample_count' */
		val32 = entry.sampleCount;
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		/* 'sample_delta' */
		val32 = entry.sampleDelta;
		RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

		track->stbl_index_write_count.time_to_sample++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_write_thumb(const struct mp4_mux *mux)
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
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables,
			   mux->file_metadata.cover_type);

	/* cover */
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
			   mux->file_metadata.cover,
			   mux->file_metadata.cover_size);

out:
	return ret;
}


static int mp4_mux_recovery_write_track(const struct mp4_mux *mux,
					struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t len_name =
		(uint32_t)mp4_validate_str_len(track->name, NAME_MAX);

	err = mp4_mux_recovery_write_box_info(
		mux, track->handle, MP4_TRACK_BOX, 1);
	if (err < 0) {
		ret = -errno;
		ULOG_ERRNO("mp4_mux_recovery_write_box_info", errno);
		goto out;
	}

	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->type);
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables, track->name, len_name);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->flags);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->timescale);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->creation_time);
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, track->modification_time);
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables,
			   track->referenceTrackHandle,
			   track->referenceTrackHandleCount);

	track->track_info_written = true;
out:
	return ret;
}


static int mp4_mux_recovery_write_meta(const struct mp4_mux *mux,
				       const struct mp4_mux_metadata *meta,
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
	RECOVERY_WRITE_VAL(mux->recovery.fd_tables, val32);

	/* key */
	val32 = (uint32_t)mp4_validate_str_len(meta->key, NAME_MAX);
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables, meta->key, val32);

	/* value */
	val32 = (uint32_t)mp4_validate_str_len(meta->value, METADATA_VALUE_MAX);
	RECOVERY_WRITE_ARR(mux->recovery.fd_tables, meta->value, val32);

out:
	return ret;
}


static inline int mp4_mux_sync_meta(const struct mp4_mux *mux,
				    const struct list_node *metadatas,
				    uint32_t *meta_write_count,
				    uint32_t track_handle)
{
	int ret = 0;
	const struct mp4_mux_metadata *meta;
	uint32_t meta_count = 0;

	/* Metadata */
	list_walk_entry_forward(metadatas, meta, node)
	{
		meta_count++;
		if (meta_count < *meta_write_count)
			continue;

		ret = mp4_mux_recovery_write_meta(mux, meta, track_handle);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_write_meta", -ret);
			goto out;
		}
	}
	*meta_write_count = meta_count + 1;

out:
	return ret;
}


static inline int mp4_mux_sync_track(const struct mp4_mux *mux,
				     struct mp4_mux_track *track)
{
	int ret = 0;

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

	ret = mp4_mux_sync_meta(mux,
				&track->metadatas,
				&track->meta_write_count,
				track->handle);
	if (ret < 0)
		goto out;

out:
	return ret;
}


int mp4_recovery_tables_header_write(
	int tables_file_fd,
	const struct mp4_recovery_tables_header *header,
	bool first_write)
{
	int ret = 0;
	int err = 0;
	off_t tables_size_offset =
		sizeof(header->magic) + sizeof(header->version);

	ULOG_ERRNO_RETURN_ERR_IF(tables_file_fd < 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	if (!first_write) {
		/* Only write the tables_size */
		RECOVERY_PWRITE_VAL(tables_file_fd,
				    header->tables_size,
				    tables_size_offset);
		return ret;
	}

	/* Write only the first time */
	RECOVERY_WRITE_VAL(tables_file_fd, header->magic);
	RECOVERY_WRITE_VAL(tables_file_fd, header->version);
	RECOVERY_WRITE_VAL(tables_file_fd, header->tables_size);
	RECOVERY_WRITE_VAL(tables_file_fd, header->mux_tables_size);
	RECOVERY_WRITE_ARR(
		tables_file_fd, header->data_path, header->data_path_length);
	RECOVERY_WRITE_ARR(tables_file_fd, header->uuid, header->uuid_length);

out:
	return ret;
}


int mp4_mux_incremental_sync(struct mp4_mux *mux)
{
	int ret = 0;
	struct mp4_mux_track *track;
	struct mp4_recovery_tables_header header = {};
	off_t curr_off;

	curr_off = lseek(mux->recovery.fd_tables, 0, SEEK_CUR);
	if (curr_off == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto out;
	}

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

		ret = mp4_mux_sync_track(mux, track);
		if (ret < 0)
			goto out;
	}

	ret = mp4_mux_sync_meta(
		mux, &mux->metadatas, &mux->recovery.meta_write_count, 0);
	if (ret < 0)
		goto out;

	/* thumbnail */
	if (!mux->recovery.thumb_written && mux->file_metadata.cover != NULL) {
		mp4_mux_recovery_write_thumb(mux);
		mux->recovery.thumb_written = true;
	}

	ret = mp4_mux_recovery_tables_header_fill(mux, &header);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_recovery_tables_header_fill", -ret);
		goto out;
	}

	ret = mp4_recovery_tables_header_write(
		mux->recovery.fd_tables, &header, false);
	if (ret < 0) {
		ULOG_ERRNO("mp4_recovery_tables_header_write", -ret);
		goto out;
	}

out:
	(void)mp4_recovery_tables_header_clear(&header);
	return ret;
}
