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

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_ITEM_NUMBER 1000000

#define RECOVERY_READ_VAL(_val)                                                \
	do {                                                                   \
		err = read(file_fd, &_val, sizeof(_val));                      \
		if (err < 0) {                                                 \
			ret = -errno;                                          \
			ULOG_ERRNO("read", errno);                             \
			goto out;                                              \
		} else if ((size_t)err != sizeof(_val)) {                      \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("read", -ret);                              \
			goto out;                                              \
		}                                                              \
	} while (0)


#define RECOVERY_READ_ARR(_val, _size)                                         \
	do {                                                                   \
		RECOVERY_READ_VAL(_size);                                      \
		if (_size > sizeof(_val)) {                                    \
			ret = -EPROTO;                                         \
			ULOGE("'%s': read size (%zu) exceeds "                 \
			      "size (%zu)",                                    \
			      #_val,                                           \
			      (size_t)_size,                                   \
			      sizeof(_val));                                   \
			goto out;                                              \
		}                                                              \
		if (_size == 0)                                                \
			break;                                                 \
		err = read(file_fd, _val, _size);                              \
		if (err < 0) {                                                 \
			ret = -errno;                                          \
			ULOG_ERRNO("read", errno);                             \
			goto out;                                              \
		} else if ((size_t)err != _size) {                             \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("read", -ret);                              \
			goto out;                                              \
		}                                                              \
	} while (0)


#define RECOVERY_READ_PTR(_val, _size)                                         \
	do {                                                                   \
		RECOVERY_READ_VAL(_size);                                      \
		if (_size > MAX_ALLOC_SIZE) {                                  \
			ret = -EPROTO;                                         \
			ULOGE("'%s': read size (%zu) exceeds "                 \
			      "maximum allocation size (%zu)",                 \
			      #_val,                                           \
			      (size_t)_size,                                   \
			      (size_t)MAX_ALLOC_SIZE);                         \
			goto out;                                              \
		}                                                              \
		free(_val);                                                    \
		_val = calloc(_size, 1);                                       \
		if (_val == NULL) {                                            \
			ret = -ENOMEM;                                         \
			ULOG_ERRNO("calloc", -ret);                            \
			goto out;                                              \
		}                                                              \
		err = read(file_fd, _val, _size);                              \
		if (err < 0) {                                                 \
			free(_val);                                            \
			_val = NULL;                                           \
			ret = -errno;                                          \
			ULOG_ERRNO("read", errno);                             \
			goto out;                                              \
		} else if ((size_t)err != _size) {                             \
			free(_val);                                            \
			_val = NULL;                                           \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("read", -ret);                              \
			goto out;                                              \
		}                                                              \
	} while (0)


#define RECOVERY_READ_STR(_val, _size)                                         \
	do {                                                                   \
		RECOVERY_READ_VAL(_size);                                      \
		if (_size > MAX_ALLOC_SIZE) {                                  \
			ret = -EPROTO;                                         \
			ULOGE("'%s': read size (%zu) exceeds "                 \
			      "maximum allocation size (%zu)",                 \
			      #_val,                                           \
			      (size_t)_size,                                   \
			      (size_t)MAX_ALLOC_SIZE);                         \
			goto out;                                              \
		}                                                              \
		free(_val);                                                    \
		_val = calloc(_size + 1, 1);                                   \
		if (_val == NULL) {                                            \
			ret = -ENOMEM;                                         \
			ULOG_ERRNO("calloc", -ret);                            \
			goto out;                                              \
		}                                                              \
		err = read(file_fd, _val, _size);                              \
		if (err < 0) {                                                 \
			free(_val);                                            \
			_val = NULL;                                           \
			ret = -errno;                                          \
			ULOG_ERRNO("read", errno);                             \
			goto out;                                              \
		} else if ((size_t)err != _size) {                             \
			free(_val);                                            \
			_val = NULL;                                           \
			ret = -ENODATA;                                        \
			ULOG_ERRNO("read", -ret);                              \
			goto out;                                              \
		}                                                              \
		_val[_size] = '\0';                                            \
	} while (0)


struct recovery_box_info {
	/* track id or 0 if parent is not a track */
	uint32_t track_handle;
	/* MP4 box type */
	uint32_t type;
	/* number of elements to read */
	uint32_t number;
};


static int mp4_mux_recovery_read_stsc(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	struct mp4_mux_track *track;
	struct mp4_sample_to_chunk_entry entry;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	for (size_t i = 0; i < item->number; i++) {
		RECOVERY_READ_VAL(entry.firstChunk);
		RECOVERY_READ_VAL(entry.samplesPerChunk);
		RECOVERY_READ_VAL(entry.sampleDescriptionIndex);

		if (track->sample_to_chunk.count + 1 >
		    track->sample_to_chunk.capacity) {
			ret = mp4_mux_grow_stc(track, 1);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_grow_stc", -ret);
				goto out;
			}
			track->sample_to_chunk.count++;
		}
		track->sample_to_chunk
			.entries[track->sample_to_chunk.count - 1] = entry;
	}

out:
	return ret;
}


static int mp4_mux_recovery_read_stsz(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	struct mp4_mux_track *track;
	uint32_t sample_size;
	uint64_t sample_offset;
	uint64_t sample_decoding_time;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	for (size_t i = 0; i < item->number; i++) {
		RECOVERY_READ_VAL(sample_size);
		RECOVERY_READ_VAL(sample_offset);
		RECOVERY_READ_VAL(sample_decoding_time);

		if (track->samples.count + 1 > track->samples.capacity) {
			ret = mp4_mux_grow_samples(track, 1);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_grow_samples", -ret);
				goto out;
			}
		}

		track->samples.sizes[track->samples.count] = sample_size;
		track->samples.offsets[track->samples.count] = sample_offset;
		track->samples.decoding_times[track->samples.count] =
			sample_decoding_time;
		track->samples.count++;
	}

out:
	return ret;
}


static int
mp4_mux_recovery_read_audio_specific_config(int file_fd,
					    struct mp4_mux *mux,
					    struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;

	RECOVERY_READ_VAL(val32);
	track->audio.codec = (enum mp4_audio_codec)val32;

	RECOVERY_READ_PTR(track->audio.specific_config,
			  track->audio.specific_config_size);

	RECOVERY_READ_VAL(track->audio.channel_count);

	RECOVERY_READ_VAL(track->audio.sample_size);

	RECOVERY_READ_VAL(track->audio.sample_rate);

out:
	return ret;
}


static int mp4_mux_recovery_read_vdec(int file_fd,
				      struct mp4_mux *mux,
				      struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t codec;

	RECOVERY_READ_VAL(codec);

	switch (codec) {
	case MP4_AVC1:
		track->video.codec = MP4_VIDEO_CODEC_AVC;
		RECOVERY_READ_PTR(track->video.avc.sps,
				  track->video.avc.sps_size);
		RECOVERY_READ_PTR(track->video.avc.pps,
				  track->video.avc.pps_size);
		break;
	case MP4_HVC1:
		track->video.codec = MP4_VIDEO_CODEC_HEVC;
		RECOVERY_READ_PTR(track->video.hevc.sps,
				  track->video.hevc.sps_size);
		RECOVERY_READ_PTR(track->video.hevc.pps,
				  track->video.hevc.pps_size);
		RECOVERY_READ_PTR(track->video.hevc.vps,
				  track->video.hevc.vps_size);
		break;
	default:
		ULOGE("invalid video codec %d", codec);
		ret = -EINVAL;
		goto out;
	}
	RECOVERY_READ_VAL(track->video.width);
	RECOVERY_READ_VAL(track->video.height);

out:
	return ret;
}


static int mp4_mux_recovery_read_metadata_stsd(int file_fd,
					       struct mp4_mux *mux,
					       struct mp4_mux_track *track)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t encoding_len = 0;
	uint32_t mime_len = 0;
	char *content_encoding = NULL;
	char *mime_type = NULL;

	/* content encoding */
	RECOVERY_READ_STR(content_encoding, encoding_len);

	/* mime format */
	RECOVERY_READ_STR(mime_type, mime_len);

	ret = mp4_mux_track_set_metadata_mime_type(
		mux, track->handle, content_encoding, mime_type);
	if (ret < 0)
		ULOG_ERRNO("mp4_mux_track_set_metadata_mime_type", -ret);

out:
	free(content_encoding);
	free(mime_type);
	return ret;
}


static int mp4_mux_recovery_read_stsd(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	struct mp4_mux_track *track;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	switch (track->type) {
	case MP4_TRACK_TYPE_VIDEO:
		ret = mp4_mux_recovery_read_vdec(file_fd, mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_read_vdec", -ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_AUDIO:
		ret = mp4_mux_recovery_read_audio_specific_config(
			file_fd, mux, track);
		if (ret < 0) {
			ULOG_ERRNO(
				"mp4_mux_recovery_read_audio_specific_config",
				-ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_METADATA:
		ret = mp4_mux_recovery_read_metadata_stsd(file_fd, mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_recovery_read_metadata_stsd", -ret);
			goto out;
		}
		break;
	case MP4_TRACK_TYPE_CHAPTERS:
		break;
	default:
		return -EINVAL;
	}
out:
	return ret;
}


static int mp4_mux_recovery_read_meta(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;
	enum mp4_mux_meta_storage storage;
	char *key = NULL;
	char *value = NULL;

	/* storage */
	RECOVERY_READ_VAL(val32);
	storage = val32;

	/* key */
	RECOVERY_READ_STR(key, val32);

	/* value */
	RECOVERY_READ_STR(value, val32);

	if (item->track_handle == 0) {
		ret = mp4_mux_add_file_metadata(mux, key, value);
		if (ret < 0)
			ULOG_ERRNO("mp4_mux_add_file_metadata", -ret);
	} else {
		ret = mp4_mux_add_track_metadata(
			mux, item->track_handle, key, value);
		if (ret < 0)
			ULOG_ERRNO("mp4_mux_add_track_metadata", -ret);
	}

out:
	free(key);
	free(value);
	return ret;
}


static int mp4_mux_recovery_read_stss(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	struct mp4_mux_track *track;
	uint32_t sync;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	for (size_t i = 0; i < item->number; i++) {
		RECOVERY_READ_VAL(sync);

		if (track->sync.count + 1 > track->sync.capacity) {
			ret = mp4_mux_grow_sync(track, 1);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_grow_sync", -ret);
				goto out;
			}
		}

		track->sync.entries[track->sync.count] = sync;
		track->sync.count++;
	}
out:
	return ret;
}


static int mp4_mux_recovery_read_stts(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	struct mp4_mux_track *track;
	struct mp4_time_to_sample_entry entry;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	for (size_t i = 0; i < item->number; i++) {
		RECOVERY_READ_VAL(entry.sampleCount);
		RECOVERY_READ_VAL(entry.sampleDelta);

		if (track->time_to_sample.count + 1 >
		    track->time_to_sample.capacity) {
			ret = mp4_mux_grow_tts(track, 1);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_grow_tts", -ret);
				goto out;
			}
		}
		track->time_to_sample.entries[track->time_to_sample.count] =
			entry;
		track->time_to_sample.count++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_read_thumb(int file_fd,
				       struct mp4_mux *mux,
				       const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	uint32_t val32;

	/* cover type */
	RECOVERY_READ_VAL(val32);
	mux->file_metadata.cover_type = (enum mp4_metadata_cover_type)val32;

	/* cover */
	RECOVERY_READ_PTR(mux->file_metadata.cover,
			  mux->file_metadata.cover_size);

out:
	if (ret < 0)
		mux->file_metadata.cover_type = MP4_METADATA_COVER_TYPE_UNKNOWN;

	return ret;
}


static int mp4_mux_recovery_read_stco(int file_fd,
				      struct mp4_mux *mux,
				      const struct recovery_box_info *item)
{
	int ret = 0;
	ssize_t err = 0;
	struct mp4_mux_track *track;
	uint64_t offset;

	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		return ret;
	}

	for (size_t i = 0; i < item->number; i++) {
		/* 64 bits written whether it's co or co64 */
		RECOVERY_READ_VAL(offset);

		if (track->chunks.count + 1 > track->chunks.capacity) {
			ret = mp4_mux_grow_chunks(track, 1);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_grow_chunks", -ret);
				goto out;
			}
		}

		track->chunks.offsets[track->chunks.count] = offset;

		track->chunks.count++;
	}

out:
	return ret;
}


static int mp4_mux_recovery_read_track(int file_fd,
				       struct mp4_mux *mux,
				       const struct recovery_box_info *item)
{
	ssize_t err = 0;
	int ret = 0;
	struct mp4_mux_track_params params = {};
	uint32_t len_name;
	char *name = NULL;
	uint32_t flags;
	uint64_t val64;
	struct mp4_mux_track *track = NULL;

	RECOVERY_READ_VAL(params.type);
	RECOVERY_READ_STR(name, len_name);
	RECOVERY_READ_VAL(flags);

	params.name = name;
	params.enabled = !!(flags & TRACK_FLAG_ENABLED);
	params.in_movie = !!(flags & TRACK_FLAG_IN_MOVIE);
	params.in_preview = !!(flags & TRACK_FLAG_IN_PREVIEW);

	RECOVERY_READ_VAL(params.timescale);

	/* mp4_mux_add_track adds MP4_MAC_TO_UNIX_EPOCH_OFFSET */
	RECOVERY_READ_VAL(val64);
	if (val64 < MP4_MAC_TO_UNIX_EPOCH_OFFSET) {
		ret = -EPROTO;
		ULOG_ERRNO("creation time is invalid", -ret);
		goto out;
	}
	params.creation_time = val64 - MP4_MAC_TO_UNIX_EPOCH_OFFSET;

	/* mp4_mux_add_track adds MP4_MAC_TO_UNIX_EPOCH_OFFSET */
	RECOVERY_READ_VAL(val64);
	if (val64 < MP4_MAC_TO_UNIX_EPOCH_OFFSET) {
		ret = -EPROTO;
		ULOG_ERRNO("modification time is invalid", -ret);
		goto out;
	}
	params.modification_time = val64 - MP4_MAC_TO_UNIX_EPOCH_OFFSET;

	/* if track already present, only update references */
	track = mp4_mux_track_find_by_handle(mux, item->track_handle);
	if (track == NULL) {
		err = mp4_mux_add_track(mux, &params);
		if (err < 0)
			ULOG_ERRNO("mp4_mux_add_track", -err);

		track = mp4_mux_track_find_by_handle(mux, item->track_handle);
		if (track == NULL) {
			ret = -ENOENT;
			ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
			return ret;
		}
	}
	RECOVERY_READ_ARR(track->referenceTrackHandle,
			  track->referenceTrackHandleCount);

out:
	free(name);
	return ret;
}


static const struct {
	uint32_t type;
	int (*func)(int file_fd,
		    struct mp4_mux *mux,
		    const struct recovery_box_info *item);
	bool fatal;
} type_map[] = {
	{MP4_TRACK_BOX, &mp4_mux_recovery_read_track, true},
	{MP4_DECODING_TIME_TO_SAMPLE_BOX, &mp4_mux_recovery_read_stts, false},
	{MP4_SYNC_SAMPLE_BOX, &mp4_mux_recovery_read_stss, false},
	{MP4_SAMPLE_TO_CHUNK_BOX, &mp4_mux_recovery_read_stsc, false},
	{MP4_SAMPLE_SIZE_BOX, &mp4_mux_recovery_read_stsz, false},
	{MP4_CHUNK_OFFSET_BOX, &mp4_mux_recovery_read_stco, false},
	{MP4_CHUNK_OFFSET_64_BOX, &mp4_mux_recovery_read_stco, false},
	{MP4_SAMPLE_DESCRIPTION_BOX, &mp4_mux_recovery_read_stsd, true},
	{MP4_META_BOX, &mp4_mux_recovery_read_meta, false},
	{MP4_METADATA_TAG_TYPE_COVER, &mp4_mux_recovery_read_thumb, false},
};


static int mp4_mux_recovery_read_box_info(int file_fd,
					  struct recovery_box_info *item,
					  struct mp4_mux *mux,
					  bool *minor_fail)
{
	int ret = 0;
	ssize_t err = 0;

	RECOVERY_READ_VAL(item->track_handle);
	RECOVERY_READ_VAL(item->type);
	RECOVERY_READ_VAL(item->number);

	for (size_t i = 0; i < ARRAY_SIZE(type_map); i++) {
		if (item->type != type_map[i].type)
			continue;
		if (item->number > MAX_ITEM_NUMBER) {
			ULOGE("item count is too big");
			return -EPROTO;
		}
		ret = type_map[i].func(file_fd, mux, item);
		*minor_fail = !type_map[i].fatal && (ret < 0);
		return ret;
	}

	ULOGE("unknown box %d", item->type);
	return -EPROTO;

out:
	return ret;
}


int mp4_mux_fill_from_file(const char *file_path,
			   struct mp4_mux *mux,
			   char **error_msg)
{
	int ret = 0;
	int file_fd = open(file_path, O_RDONLY);
	ssize_t end_off;
	ssize_t curr_off;
	struct recovery_box_info item;
	struct mp4_mux_track *track;
	uint32_t resized_samples = 0;
	off_t end_of_file;
	off_t max_offset = 0;
	off_t tmp_offset;
	bool minor_fail = false;
	uint32_t min_count = 0;

	if (file_fd == -1) {
		ret = -errno;
		*error_msg = strdup("failed to open tables file");
		ULOG_ERRNO("%s (%s)", errno, *error_msg, file_path);
		goto out;
	}

	end_of_file = lseek(mux->fd, 0, SEEK_END);
	if (end_of_file < 0) {
		ret = -errno;
		*error_msg = strdup("failed to parse data file");
		ULOG_ERRNO("lseek: %s (%s)", errno, *error_msg, mux->filename);
		goto out;
	}

	end_off = lseek(file_fd, 0, SEEK_END);
	if (end_off <= 0) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		*error_msg = strdup("Failed to parse tables file");
		goto out;
	}

	curr_off = lseek(file_fd, 0, SEEK_SET);
	if (curr_off < 0) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto out;
	}

	while ((curr_off + 12) < end_off) {
		minor_fail = false;
		ret = mp4_mux_recovery_read_box_info(
			file_fd, &item, mux, &minor_fail);
		if (minor_fail) {
			/* crashed occurred during sync but mp4 is still
			 * recoverable */
			ULOGW_ERRNO(-ret, "mp4_mux_recovery_read_box_info");
			break;
		} else if (ret < 0) {
			/* mp4 will not be recoverable, quit with error */
			*error_msg = strdup("Failed to parse tables file");
			ULOG_ERRNO("mp4_mux_recovery_read_box_info: %s (%s)",
				   -ret,
				   *error_msg,
				   file_path);
			goto out;
		}
		curr_off = lseek(file_fd, 0, SEEK_CUR);
		if (curr_off < 0) {
			ret = -errno;
			ULOG_ERRNO("lseek", -ret);
			goto out;
		}
	}

	/* remove samples referencing unexisting data */
	list_walk_entry_forward(&mux->tracks, track, node)
	{
		min_count = MIN(track->chunks.count, track->samples.count);
		resized_samples = 0;
		for (size_t i = 0; i < min_count; i++) {
			tmp_offset = track->chunks.offsets[i] +
				     track->samples.sizes[i];
			if (tmp_offset > end_of_file)
				break;
			max_offset = MAX(max_offset, tmp_offset);
			resized_samples++;
		}
		track->samples.count = resized_samples;
		track->chunks.count = resized_samples;
	}

	/* remove unreferenced data */
	ret = ftruncate(mux->fd, max_offset);
	if (ret < 0) {
		ret = -errno;
		*error_msg = strdup("Failed to parse data file");
		ULOG_ERRNO(
			"ftruncate: %s (%s)", errno, *error_msg, mux->filename);
	}

out:
	if (file_fd != -1)
		close(file_fd);
	return ret;
}
