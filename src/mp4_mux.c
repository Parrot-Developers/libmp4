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
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
/* For fsync() */
#	include <unistd.h>
/* For iov/writev */
#	include <sys/uio.h>
#endif
#define MP4_MUX_TABLES_GROW_SIZE 128

#ifdef _WIN32

struct iovec {
	void *iov_base;
	size_t iov_len;
};

static ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t total = 0, ret;
	for (int i = 0; i < iovcnt; i++) {
		ret = write(fd, iov[i].iov_base, iov[i].iov_len);
		if (ret < 0)
			return total == 0 ? ret : total;
		total += ret;
		if ((size_t)ret != iov[i].iov_len)
			return total;
	}
	return total;
}

#endif


struct mp4_mux_track *mp4_mux_track_find_by_handle(struct mp4_mux *mux,
						   uint32_t track_handle)
{
	struct mp4_mux_track *_track = NULL;

	if (track_handle > mux->track_count)
		return NULL;

	list_walk_entry_forward(&mux->tracks, _track, node)
	{
		if (_track->handle == track_handle)
			return _track;
	}
	return NULL;
}


int mp4_mux_grow_samples(struct mp4_mux_track *track, int new_samples)
{
	uint32_t *tmp;
	uint64_t *tmp64;

	uint32_t nextcap = track->samples.capacity;

	while (nextcap < track->samples.count + new_samples)
		nextcap += MP4_MUX_TABLES_GROW_SIZE;

	if (nextcap == track->samples.capacity)
		return 0;

	tmp = realloc(track->samples.sizes, nextcap * sizeof(*tmp));
	if (tmp == NULL)
		return -ENOMEM;
	track->samples.sizes = tmp;

	tmp64 = realloc(track->samples.decoding_times,
			nextcap * sizeof(*tmp64));
	if (tmp64 == NULL)
		return -ENOMEM;
	track->samples.decoding_times = tmp64;

	tmp64 = realloc(track->samples.offsets, nextcap * sizeof(*tmp64));
	if (tmp64 == NULL)
		return -ENOMEM;
	track->samples.offsets = tmp64;

	track->samples.capacity = nextcap;
	return 0;
}


int mp4_mux_grow_chunks(struct mp4_mux_track *track, int new_chunks)
{
	uint64_t *tmp64;

	uint32_t nextcap = track->chunks.capacity;

	while (nextcap < track->chunks.count + new_chunks)
		nextcap += MP4_MUX_TABLES_GROW_SIZE;

	if (nextcap == track->chunks.capacity)
		return 0;

	tmp64 = realloc(track->chunks.offsets, nextcap * sizeof(*tmp64));
	if (tmp64 == NULL)
		return -ENOMEM;
	track->chunks.offsets = tmp64;

	track->chunks.capacity = nextcap;
	return 0;
}


int mp4_mux_grow_tts(struct mp4_mux_track *track, int new_tts)
{
	struct mp4_time_to_sample_entry *tmp;

	uint32_t nextcap = track->time_to_sample.capacity;

	while (nextcap < track->time_to_sample.count + new_tts)
		nextcap += MP4_MUX_TABLES_GROW_SIZE;

	if (nextcap == track->time_to_sample.capacity)
		return 0;

	tmp = realloc(track->time_to_sample.entries, nextcap * sizeof(*tmp));
	if (tmp == NULL)
		return -ENOMEM;
	track->time_to_sample.entries = tmp;

	track->time_to_sample.capacity = nextcap;
	return 0;
}


int mp4_mux_grow_stc(struct mp4_mux_track *track, int new_stc)
{
	struct mp4_sample_to_chunk_entry *tmp;

	uint32_t nextcap = track->sample_to_chunk.capacity;

	while (nextcap < track->sample_to_chunk.count + new_stc)
		nextcap += MP4_MUX_TABLES_GROW_SIZE;

	if (nextcap == track->sample_to_chunk.capacity)
		return 0;

	tmp = realloc(track->sample_to_chunk.entries, nextcap * sizeof(*tmp));
	if (tmp == NULL)
		return -ENOMEM;
	track->sample_to_chunk.entries = tmp;

	track->sample_to_chunk.capacity = nextcap;
	return 0;
}


int mp4_mux_grow_sync(struct mp4_mux_track *track, int new_sync)
{
	uint32_t *tmp;

	uint32_t nextcap = track->sync.capacity;

	while (nextcap < track->sync.count + new_sync)
		nextcap += MP4_MUX_TABLES_GROW_SIZE;

	if (nextcap == track->sync.capacity)
		return 0;

	tmp = realloc(track->sync.entries, nextcap * sizeof(*tmp));
	if (tmp == NULL)
		return -ENOMEM;
	track->sync.entries = tmp;

	track->sync.capacity = nextcap;
	return 0;
}


int mp4_mux_track_compute_tts(struct mp4_mux *mux, struct mp4_mux_track *track)
{
	int ret;
	uint32_t i, nsamples;
	uint64_t prev_dts, next_dts;
	uint32_t diff, prev_diff;

	if (track == NULL)
		return 0;

	track->time_to_sample.count = 0;
	track->duration = 0;
	track->duration_moov = 0;

	nsamples = track->samples.count;
	/* Trivial case : zero samples, nothing to do */
	if (nsamples == 0)
		return 0;

	prev_diff = UINT32_MAX;
	prev_dts = track->samples.decoding_times[0];
	for (i = 1; i < nsamples; i++) {
		next_dts = track->samples.decoding_times[i];
		diff = next_dts - prev_dts;
		/* Convert to timescale */
		track->duration_moov += mp4_convert_timescale(
			diff, track->timescale, mux->timescale);
		track->duration += diff;
		if (diff != prev_diff) {
			ret = mp4_mux_grow_tts(track, 1);
			if (ret != 0)
				return ret;
			track->time_to_sample
				.entries[track->time_to_sample.count]
				.sampleCount = 1;
			track->time_to_sample
				.entries[track->time_to_sample.count]
				.sampleDelta = diff;
			track->time_to_sample.count++;
		} else if (track->time_to_sample.count > 0) {
			track->time_to_sample
				.entries[track->time_to_sample.count - 1]
				.sampleCount++;
		}
		prev_diff = diff;
		prev_dts = next_dts;
	}
	/* Add a final zero-length entry */
	ret = mp4_mux_grow_tts(track, 1);
	if (ret != 0)
		return ret;
	track->time_to_sample.entries[track->time_to_sample.count].sampleCount =
		1;
	track->time_to_sample.entries[track->time_to_sample.count].sampleDelta =
		0;
	track->time_to_sample.count++;

	return 0;
}


static void mp4_mux_track_destroy(struct mp4_mux_track *track)
{
	struct mp4_mux_metadata *meta, *tmp;

	if (track == NULL)
		return;

	list_del(&track->node);
	/* Samples */
	free(track->samples.sizes);
	free(track->samples.decoding_times);
	free(track->samples.offsets);
	/* Chunks */
	free(track->chunks.offsets);
	/* 'time_to_sample' */
	free(track->time_to_sample.entries);
	/* 'sample_to_chunk' */
	free(track->sample_to_chunk.entries);
	/* 'sync' */
	free(track->sync.entries);
	/* cover of the track*/
	free(track->track_metadata.cover);

	list_walk_entry_forward_safe(&track->metadatas, meta, tmp, node)
	{
		free(meta->key);
		free(meta->value);
		list_del(&meta->node);
		free(meta);
	}

	/* Type-specific configs */
	switch (track->type) {
	case MP4_TRACK_TYPE_VIDEO:
		mp4_video_decoder_config_destroy(&track->video);
		break;
	case MP4_TRACK_TYPE_AUDIO:
		free(track->audio.specific_config);
		break;
	case MP4_TRACK_TYPE_METADATA:
		free(track->metadata.content_encoding);
		free(track->metadata.mime_type);
		break;
	default:
		break;
	}
	free(track->name);
	free(track);
}


static void mp4_mux_free(struct mp4_mux *mux)
{
	struct mp4_mux_track *track, *ttmp;
	struct mp4_mux_metadata *meta, *mtmp;
	int len_recovery_path = 0;

	if (mux == NULL)
		return;

	if (mux->fd != -1)
		close(mux->fd);

	free(mux->file_metadata.cover);

	list_walk_entry_forward_safe(&mux->tracks, track, ttmp, node)
	{
		mp4_mux_track_destroy(track);
	}

	list_walk_entry_forward_safe(&mux->metadatas, meta, mtmp, node)
	{
		free(meta->key);
		free(meta->value);
		list_del(&meta->node);
		free(meta);
	}

	if (mux->recovery.tables_file != NULL &&
	    mux->recovery.link_file != NULL &&
	    mux->recovery.tmp_tables_file != NULL &&
	    !mux->recovery.failed_in_close) {
		if (mux->recovery.fd_link != -1)
			close(mux->recovery.fd_link);
		if (mux->recovery.fd_tables != -1)
			close(mux->recovery.fd_tables);

		free(mux->recovery.link_file);
		len_recovery_path = strlen(mux->recovery.tables_file);
		/* tmp_tables_file file is supposed to be deleted when writing
		 * tables but if something wrong happenened we try to delete it
		 * here also
		 */
		if (remove(mux->recovery.tmp_tables_file) < 0 &&
		    errno != ENOENT)
			ULOG_ERRNO("remove %s",
				   errno,
				   mux->recovery.tmp_tables_file);
	}
	free(mux->tables.buf);
	free(mux->recovery.tmp_tables_file);
	free(mux->recovery.tables_file);
	free(mux->filename);
	free(mux);
}


static int
mp4_mux_track_set_avc_decoder_config(struct mp4_mux_track *track,
				     struct mp4_video_decoder_config *vdc)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->codec != MP4_VIDEO_CODEC_AVC, EPROTO);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.c_sps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.sps_size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.c_pps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.pps_size == 0, EINVAL);

	track->video.avc.sps_size = vdc->avc.sps_size;
	free(track->video.avc.sps);
	track->video.avc.sps = malloc(vdc->avc.sps_size);
	if (track->video.avc.sps == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	memcpy(track->video.avc.sps, vdc->avc.c_sps, vdc->avc.sps_size);
	track->video.avc.pps_size = vdc->avc.pps_size;
	free(track->video.avc.pps);
	track->video.avc.pps = malloc(vdc->avc.pps_size);
	if (track->video.avc.pps == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	memcpy(track->video.avc.pps, vdc->avc.c_pps, vdc->avc.pps_size);

	return 0;
error:
	mp4_video_decoder_config_destroy(&track->video);
	return ret;
}


static int
mp4_mux_track_set_hevc_decoder_config(struct mp4_mux_track *track,
				      struct mp4_video_decoder_config *vdc)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->codec != MP4_VIDEO_CODEC_HEVC, EPROTO);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.c_vps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.vps_size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.c_sps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.sps_size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.c_pps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->hevc.pps_size == 0, EINVAL);

	track->video.hevc.vps_size = vdc->hevc.vps_size;
	free(track->video.hevc.vps);
	track->video.hevc.vps = malloc(vdc->hevc.vps_size);
	if (track->video.hevc.vps == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	memcpy(track->video.hevc.vps, vdc->hevc.c_vps, vdc->hevc.vps_size);

	track->video.hevc.sps_size = vdc->hevc.sps_size;
	free(track->video.hevc.sps);
	track->video.hevc.sps = malloc(vdc->hevc.sps_size);
	if (track->video.hevc.sps == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	memcpy(track->video.hevc.sps, vdc->hevc.c_sps, vdc->hevc.sps_size);

	track->video.hevc.pps_size = vdc->hevc.pps_size;
	free(track->video.hevc.pps);
	track->video.hevc.pps = malloc(vdc->hevc.pps_size);
	if (track->video.hevc.pps == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	memcpy(track->video.hevc.pps, vdc->hevc.c_pps, vdc->hevc.pps_size);

	track->video.hevc.hvcc_info = vdc->hevc.hvcc_info;

	return 0;

error:
	mp4_video_decoder_config_destroy(&track->video);
	return ret;
}


MP4_API int mp4_mux_open(struct mp4_mux_config *config,
			 struct mp4_mux **ret_obj)
{
	struct mp4_mux *mux = NULL;
	off_t len;
	off_t err;
	int ret = 0;
	bool recovery_enabled = false;
	mode_t mode;

	ULOG_ERRNO_RETURN_ERR_IF(config == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(config->filename == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(config->filename) == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(config->tables_size_mbytes == 0, EINVAL);

	if (config->recovery.link_file != NULL ||
	    config->recovery.tables_file != NULL) {
		ULOG_ERRNO_RETURN_ERR_IF(config->recovery.link_file == NULL,
					 EINVAL);
		ULOG_ERRNO_RETURN_ERR_IF(config->recovery.tables_file == NULL,
					 EINVAL);
		recovery_enabled = true;
	}

	mux = calloc(1, sizeof(*mux));
	if (mux == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}

	list_init(&mux->tracks);
	list_init(&mux->metadatas);

	mux->filename = strdup(config->filename);

	/* Default file mode: 0600 */
	mode = config->filemode ? config->filemode : S_IRUSR | S_IWUSR;

	mux->fd = open(mux->filename, O_WRONLY | O_CREAT, mode);
	if (mux->fd == -1) {
		ret = -errno;
		ULOG_ERRNO("open:'%s'", -ret, mux->filename);
		goto error;
	}

	mux->creation_time =
		config->creation_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	mux->modification_time =
		config->modification_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	mux->timescale = config->timescale;

	mux->data_offset = config->tables_size_mbytes * 1024 * 1024;

	mux->tables.buf_size = mux->data_offset;
	mux->tables.buf = calloc(1, mux->tables.buf_size);
	if (mux->tables.buf == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}
	mux->tables.offset = 0;

	mux->file_metadata.metadatas = &mux->metadatas;
	mux->recovery.enabled = recovery_enabled;

	/* Write ftyp */
	len = mp4_box_ftyp_write(mux);
	if (len < 0) {
		ret = OFF_T_TO_ERRNO(len, EPROTO);
		ULOG_ERRNO("mp4_box_ftyp_write", -ret);
		goto error;
	}
	mux->boxes_offset = len;

	/* Write initial free (for mov table) */
	len = mp4_box_free_write(mux, mux->data_offset - mux->boxes_offset);
	if (len < 0) {
		ret = OFF_T_TO_ERRNO(len, EPROTO);
		ULOG_ERRNO("mp4_box_free_write", -ret);
		goto error;
	}

	/* Seek to dataOffset */
	err = lseek(mux->fd, mux->data_offset, SEEK_SET);
	if (err == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto error;
	}
	/* Write mdat with size zero */
	/* mp4_box_mdat_write writes directly to file */
	len = mp4_box_mdat_write(mux, 0);
	if (len < 0) {
		ret = OFF_T_TO_ERRNO(len, EPROTO);
		ULOG_ERRNO("mp4_box_mdat_write", -ret);
		goto error;
	}

	*ret_obj = mux;
	if (recovery_enabled) {
		/* link file to link tables, data and uuid */
		mux->recovery.link_file = strdup(config->recovery.link_file);
		mux->recovery.fd_link =
			open(mux->recovery.link_file, O_WRONLY | O_CREAT, 0600);
		if (mux->recovery.fd_link == -1) {
			ret = -errno;
			ULOG_ERRNO("open:'%s'", -ret, mux->recovery.link_file);
			goto error;
		}

		/* tables file to record tables */
		mux->recovery.tables_file =
			strdup(config->recovery.tables_file);
		mux->recovery.fd_tables = open(
			mux->recovery.tables_file, O_WRONLY | O_CREAT, 0600);
		if (mux->recovery.fd_tables == -1) {
			ret = -errno;
			ULOG_ERRNO(
				"open:'%s'", -ret, mux->recovery.tables_file);
			goto error;
		}

		ret = asprintf(&mux->recovery.tmp_tables_file,
			       "%s.TMP",
			       mux->recovery.tables_file);
		if (ret < 0) {
			ret = -ENOMEM;
			ULOG_ERRNO("asprintf", -ret);
			goto error;
		}

		ret = mp4_prepare_link_file(
			mux->recovery.fd_link,
			mux->recovery.tables_file,
			mux->filename,
			mux->data_offset,
			config->recovery.check_storage_uuid);
		if (ret < 0) {
			ULOG_ERRNO("mp4_prepare_link_file", -ret);
			goto error;
		}
	} else {
		mux->recovery.link_file = NULL;
		mux->recovery.fd_link = -1;
		mux->recovery.tables_file = NULL;
		mux->recovery.fd_tables = -1;
	}
#ifndef _WIN32
	ret = fsync(mux->fd);
	if (ret < 0) {
		ret = -errno;
		ULOG_ERRNO("fsync", -ret);
		return ret;
	}
#else
	ULOGW("fsync not available, mp4 file not sync'ed on disk");
#endif
	return ret;
error:
	if (mux == NULL)
		return ret;
	if (recovery_enabled) {
		if (mux->recovery.fd_link > 0) {
			err = remove(mux->recovery.link_file);
			if (err < 0) {
				ULOG_ERRNO("remove (%s)",
					   errno,
					   mux->recovery.link_file);
			}
		}
		if (mux->recovery.fd_tables > 0) {
			err = remove(mux->recovery.tables_file);
			if (err < 0) {
				ULOG_ERRNO("remove (%s)",
					   errno,
					   mux->recovery.tables_file);
			}
		}
	}
	mp4_mux_free(mux);
	return ret;
}


static struct {
	/* 0 = highest priority */
	unsigned int priority;
	enum mp4_track_type type;
} track_type_priority_map[] = {
#define TRACK_TYPE_MAX_PRIORITY 0
	{TRACK_TYPE_MAX_PRIORITY, MP4_TRACK_TYPE_VIDEO},
	{1, MP4_TRACK_TYPE_AUDIO},
	{2, MP4_TRACK_TYPE_HINT},
	{3, MP4_TRACK_TYPE_METADATA},
	{4, MP4_TRACK_TYPE_TEXT},
#define TRACK_TYPE_MIN_PRIORITY 5
	{TRACK_TYPE_MIN_PRIORITY, MP4_TRACK_TYPE_CHAPTERS},
};


static enum mp4_track_type
mp4_mux_track_type_from_priority(unsigned int priority)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(track_type_priority_map); i++) {
		if (priority == track_type_priority_map[i].priority)
			return track_type_priority_map[i].type;
	}
	return MP4_TRACK_TYPE_UNKNOWN;
}


int mp4_mux_sort_tracks(struct mp4_mux *mux)
{
	uint32_t track_id;
	struct mp4_mux_track *track = NULL, *tmp = NULL;
	struct list_node new_list;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);

	list_init(&new_list);

	track_id = 1;
	for (size_t i = TRACK_TYPE_MAX_PRIORITY; i <= TRACK_TYPE_MIN_PRIORITY;
	     i++) {
		enum mp4_track_type type = mp4_mux_track_type_from_priority(i);
		if (type == MP4_TRACK_TYPE_UNKNOWN) {
			ULOGE("invalid track priority: %zu", i);
			continue;
		}
		for (size_t j = 0; j < 2; j++) {
			list_walk_entry_forward_safe(
				&mux->tracks, track, tmp, node)
			{
				bool enabled =
					(track->flags & TRACK_FLAG_ENABLED);
				/* Skip tracks that don't match type */
				if (track->type != type)
					continue;
				/* First pass: only enabled tracks */
				if (j == 0 && !enabled)
					continue;
				/* Second pass: only disabled tracks */
				if (j == 1 && enabled)
					continue;

				track->id = track_id++;

				/* Reorder tracks in a new list */
				list_del(&track->node);
				list_add_before(&new_list, &track->node);
			}
		}
	}

	/* Replace current list with the new one */
	list_replace(&new_list, &mux->tracks);

	return 0;
}


static int mp4_mux_sync_internal(struct mp4_mux *mux, bool allow_boxes_after)
{
	struct mp4_mux_track *track;
	struct mp4_mux_metadata *meta;
	struct mp4_box *moov;
	int ret;
	uint32_t duration = 0;
	off_t end;
	off_t err;
	off_t written;

	int has_meta_meta = 0;
	int has_meta_udta = 0;
	int has_meta_udta_root = 0;

	if (mux == NULL)
		return 0;

	if (mux->max_tables_size_reached && !allow_boxes_after)
		return 0;

	/* Fix moov size */
	end = lseek(mux->fd, 0, SEEK_END);
	if (end == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto out;
	}
	written = end - mux->data_offset - 8;
	mux->tables.offset = mux->data_offset;

	err = lseek(mux->fd, mux->data_offset, SEEK_SET);
	if (err == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto out;
	}
	/* mp4_box_mdat_write writes directly to file */
	end = mp4_box_mdat_write(mux, written);
	if (end < 0) {
		ret = OFF_T_TO_ERRNO(end, EPROTO);
		ULOG_ERRNO("mp4_box_mdat_write", -ret);
		goto out;
	}

	/* Sort tracks */
	ret = mp4_mux_sort_tracks(mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_sort_tracks", -ret);
		goto out;
	}

	list_walk_entry_forward(&mux->tracks, track, node)
	{
		ret = mp4_mux_track_compute_tts(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_track_compute_tts(%d)",
				   -ret,
				   track->handle);
			goto out;
		}
		if (track->duration_moov > duration)
			duration = track->duration_moov;
	}
	mux->duration = duration;

	moov = mp4_box_new_container(NULL, MP4_MOVIE_BOX);
	if (moov == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	/* Fill the box */
	mp4_box_new_mvhd(moov, mux);
	list_walk_entry_forward(&mux->tracks, track, node)
	{
		/* Skip empty tracks */
		if (track->samples.count == 0)
			continue;
		struct mp4_box *trak, *mdia, *minf, *dinf, *stbl;
		trak = mp4_box_new_container(moov, MP4_TRACK_BOX);
		if (trak == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_tkhd(trak, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (track->referenceTrackHandleCount > 0) {
			struct mp4_box *tref, *content;
			tref = mp4_box_new_container(trak,
						     MP4_TRACK_REFERENCE_BOX);
			if (tref == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			for (size_t i = 0; i < track->referenceTrackHandleCount;
			     i++) {
				struct mp4_mux_track *ref_track = NULL;
				ref_track = mp4_mux_track_find_by_handle(
					mux, track->referenceTrackHandle[i]);
				if (ref_track == NULL) {
					ret = -ENOENT;
					goto out;
				}
				switch (track->type) {
				case MP4_TRACK_TYPE_METADATA:
					content = mp4_box_new_cdsc(tref, track);
					break;
				default:
					switch (ref_track->type) {
					case MP4_TRACK_TYPE_CHAPTERS:
						content = mp4_box_new_chap(
							tref, track);
						break;
					default:
						/* Ref is not handled for
						 * non-metadata tracks */
						ret = -EINVAL;
						goto out;
					}
				}
				if (content == NULL) {
					ret = -ENOMEM;
					goto out;
				}
			}
		}
		mdia = mp4_box_new_container(trak, MP4_MEDIA_BOX);
		if (mdia == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_mdhd(mdia, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_hdlr(mdia, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		minf = mp4_box_new_container(mdia, MP4_MEDIA_INFORMATION_BOX);
		if (minf == NULL) {
			ret = -ENOMEM;
			goto out;
		}

		has_meta_meta = 0;
		has_meta_udta = 0;
		has_meta_udta_root = 0;

		/* Metadata */
		list_walk_entry_forward(&track->metadatas, meta, node)
		{
			switch (meta->storage) {
			case MP4_MUX_META_META:
				has_meta_meta = 1;
				break;
			case MP4_MUX_META_UDTA:
				has_meta_udta = 1;
				break;
			case MP4_MUX_META_UDTA_ROOT:
				has_meta_udta_root = 1;
				break;
			}
		}

		/* Write META metadata */
		if (has_meta_meta) {
			if (mp4_box_new_meta(trak, &track->track_metadata) ==
			    NULL) {
				ret = -ENOMEM;
				goto out;
			}
		}

		/* Write UDTA metadata */
		if (has_meta_udta || has_meta_udta_root) {
			struct mp4_box *udta =
				mp4_box_new_container(trak, MP4_USER_DATA_BOX);
			if (udta == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			/* Write UDTA metadata */
			if (has_meta_udta) {
				if (mp4_box_new_meta_udta(
					    udta, &track->track_metadata) ==
				    NULL) {
					ret = -ENOMEM;
					goto out;
				}
			}
			/* Directly write UDTA_ROOT metadata */
			list_walk_entry_forward(&track->metadatas, meta, node)
			{
				if (meta->storage != MP4_MUX_META_UDTA_ROOT)
					continue;
				if (mp4_box_new_udta_entry(udta, meta) ==
				    NULL) {
					ret = -ENOMEM;
					goto out;
				}
			}
		}

		switch (track->type) {
		case MP4_TRACK_TYPE_VIDEO:
			if (mp4_box_new_vmhd(minf, track) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			break;
		case MP4_TRACK_TYPE_AUDIO:
			if (mp4_box_new_smhd(minf, track) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			break;
		case MP4_TRACK_TYPE_METADATA:
			if (mp4_box_new_nmhd(minf, track) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			break;
		case MP4_TRACK_TYPE_CHAPTERS:
			if (mp4_box_new_gmhd(minf, track) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			break;
		default:
			break;
		}
		dinf = mp4_box_new_container(minf, MP4_DATA_INFORMATION_BOX);
		if (dinf == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		mp4_box_new_dref(dinf, track);
		stbl = mp4_box_new_container(minf, MP4_SAMPLE_TABLE_BOX);
		if (stbl == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stsd(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stts(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stss(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stsc(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stsz(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stco(stbl, track) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
	}

	has_meta_meta = 0;
	has_meta_udta = 0;
	has_meta_udta_root = 0;

	/* Metadata */
	list_walk_entry_forward(&mux->metadatas, meta, node)
	{
		switch (meta->storage) {
		case MP4_MUX_META_META:
			has_meta_meta = 1;
			break;
		case MP4_MUX_META_UDTA:
			has_meta_udta = 1;
			break;
		case MP4_MUX_META_UDTA_ROOT:
			has_meta_udta_root = 1;
			break;
		}
	}
	/* Write META metadata */
	if (has_meta_meta) {
		if (mp4_box_new_meta(moov, &mux->file_metadata) == NULL) {
			ret = -ENOMEM;
			goto out;
		}
	}
	/* Write UDTA metadata */
	if (has_meta_udta || has_meta_udta_root) {
		struct mp4_box *udta =
			mp4_box_new_container(moov, MP4_USER_DATA_BOX);
		if (udta == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		/* Write UDTA metadata */
		if (has_meta_udta) {
			if (mp4_box_new_meta_udta(udta, &mux->file_metadata) ==
			    NULL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		/* Directly write UDTA_ROOT metadata */
		list_walk_entry_forward(&mux->metadatas, meta, node)
		{
			if (meta->storage != MP4_MUX_META_UDTA_ROOT)
				continue;
			if (mp4_box_new_udta_entry(udta, meta) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	mux->tables.offset = 0;
	written = moov->writer.func(
		mux, moov, mux->data_offset - mux->boxes_offset);
	if (written >= 0) {
		err = lseek(mux->fd, mux->boxes_offset, SEEK_SET);
		if (err == -1) {
			ret = -errno;
			ULOG_ERRNO("lseek", -ret);
			goto out;
		}
		err = write(mux->fd, mux->tables.buf, mux->tables.offset);
		if (err != mux->tables.offset) {
			if (err < 0) {
				ret = -errno;
				ULOG_ERRNO("write", -ret);
			} else {
				ULOG_ERRNO(
					"only %zu bytes written instead of %zu",
					EIO,
					(size_t)(err),
					(size_t)(mux->tables.offset));
				ret = -EPROTO;
			}
			goto out;
		}
		/* Written, pad with a free */
		end = mp4_box_free_write(
			mux,
			mux->data_offset -
				(mux->tables.offset + mux->boxes_offset));
		if (end == -1) {
			ret = -end;
			ULOG_ERRNO("mp4_box_free_write", -ret);
			goto out;
		}
	} else if (written == -ENOSPC && allow_boxes_after) {
		/* Not enough space, rewrite free, then put boxes at the end */
		unsigned int tables_factor = 2;
		while (written == -ENOSPC) {
			/* Raise tables size until large enough */
			mux->tables.buf_size = mux->data_offset * tables_factor;
			free(mux->tables.buf);
			mux->tables.buf = calloc(1, mux->tables.buf_size);
			if (mux->tables.buf == NULL) {
				ret = -ENOMEM;
				ULOG_ERRNO("calloc", -ret);
				mp4_box_destroy(moov);
				goto out;
			}

			mux->tables.offset = 0;
			written = moov->writer.func(
				mux, moov, mux->tables.buf_size);
			if (written < 0 && written != -ENOSPC) {
				ret = OFF_T_TO_ERRNO(written, EPROTO);
				mp4_box_destroy(moov);
				ULOG_ERRNO("mp4_box_write", -ret);
				goto out;
			}
			tables_factor++;
		}
		if (mux->data_offset * tables_factor >= INT_MAX) {
			ULOGE("tables size too big, abandon sync");
			ret = -ENOSPC;
			mp4_box_destroy(moov);
			goto out;
		}

		err = lseek(mux->fd, mux->boxes_offset, SEEK_SET);
		if (err == -1) {
			ret = -errno;
			ULOG_ERRNO("lseek", -ret);
			goto out;
		}

		uint32_t val32 = htonl(mux->data_offset - mux->boxes_offset);
		err = write(mux->fd, &val32, sizeof(uint32_t));
		if (err != sizeof(uint32_t)) {
			if (err < 0) {
				ret = -errno;
				ULOG_ERRNO("write", -ret);
			} else {
				ULOG_ERRNO(
					"only %zu bytes written instead of %zu",
					EIO,
					(size_t)(err),
					sizeof(uint32_t));
				ret = -EPROTO;
			}
			goto out;
		}

		/* Box name */
		val32 = htonl(MP4_FREE_BOX);
		err = write(mux->fd, &val32, sizeof(uint32_t));
		if (err != sizeof(uint32_t)) {
			if (err < 0) {
				ret = -errno;
				ULOG_ERRNO("write", -ret);
			} else {
				ULOG_ERRNO(
					"only %zu bytes written instead of %zu",
					EIO,
					(size_t)(err),
					sizeof(uint32_t));
				ret = -EPROTO;
			}
			goto out;
		}

		err = lseek(mux->fd, 0, SEEK_END);
		if (err == -1) {
			ret = -errno;
			ULOG_ERRNO("lseek", -ret);
			goto out;
		}

		err = write(mux->fd, mux->tables.buf, mux->tables.offset);
		if (err != mux->tables.offset) {
			if (err < 0) {
				ret = -errno;
				ULOG_ERRNO("write", -ret);
			} else {
				ULOG_ERRNO(
					"only %zu bytes written instead of %zu",
					EIO,
					(size_t)(err),
					(size_t)(mux->tables.offset));
				ret = -EPROTO;
			}
			goto out;
		}
	} else if (written == -ENOSPC) {
		ULOGW("max_tables_size reached, mp4 file not sync'ed on disk");
		mux->max_tables_size_reached = true;
		ret = -ENOBUFS;
		mp4_box_destroy(moov);
		goto out;
	} else {
		ret = OFF_T_TO_ERRNO(written, EPROTO);
		ULOG_ERRNO("mp4_box_write", -ret);
	}

	mp4_box_destroy(moov);

#ifndef _WIN32
	ret = fsync(mux->fd);
	if (ret != 0)
		return -errno;
#else
	ULOGW("fsync not available, mp4 file not sync'ed on disk");
#endif

out:
	/* Seek back to end */
	err = lseek(mux->fd, 0, SEEK_END);
	if (err == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
	}
	return ret;
}


MP4_API int mp4_mux_sync(struct mp4_mux *mux, bool write_tables)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);

	if (mux->recovery.tables_file != NULL) {
		ret = mp4_mux_incremental_sync(mux);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_incremental_sync", -ret);
			return ret;
		}
	}

	if (write_tables) {
		ret = mp4_mux_sync_internal(mux, false);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_sync_internal", -ret);
			return ret;
		}
	}

	return 0;
}


MP4_API int mp4_mux_close(struct mp4_mux *mux)
{
	int ret = 0;
	if (mux == NULL)
		return 0;

	ret = mp4_mux_sync_internal(mux, true);
	if (ret < 0) {
		mux->recovery.failed_in_close = true;
		ULOG_ERRNO("mp4_mux_sync_internal", -ret);
	}

	mp4_mux_free(mux);
	return ret;
}


MP4_API int mp4_mux_add_track(struct mp4_mux *mux,
			      const struct mp4_mux_track_params *params)
{
	int ret;
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(params == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(params->timescale == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		params->type != MP4_TRACK_TYPE_VIDEO &&
			params->type != MP4_TRACK_TYPE_AUDIO &&
			params->type != MP4_TRACK_TYPE_METADATA &&
			params->type != MP4_TRACK_TYPE_CHAPTERS,
		EINVAL);

	track = calloc(1, sizeof(*track));
	if (!track)
		return -ENOMEM;

	track->type = params->type;
	track->name = xstrdup(params->name);
	track->flags = 0;
	if (params->enabled)
		track->flags |= TRACK_FLAG_ENABLED;
	if (params->in_movie)
		track->flags |= TRACK_FLAG_IN_MOVIE;
	if (params->in_preview)
		track->flags |= TRACK_FLAG_IN_PREVIEW;

	ret = mp4_mux_grow_stc(track, 1);
	if (ret != 0)
		goto error;
	track->sample_to_chunk.entries[0].firstChunk = 1;
	track->sample_to_chunk.entries[0].samplesPerChunk = 1;
	track->sample_to_chunk.entries[0].sampleDescriptionIndex = 1;
	track->sample_to_chunk.count = 1;

	track->timescale = params->timescale;
	track->creation_time =
		params->creation_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	track->modification_time =
		params->modification_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;

	list_add_before(&mux->tracks, &track->node);
	mux->track_count++;
	track->handle = mux->track_count;

	list_init(&track->metadatas);

	track->track_metadata.metadatas = &track->metadatas;
	track->track_metadata.cover = NULL;
	track->track_metadata.cover_type = MP4_METADATA_COVER_TYPE_UNKNOWN;
	track->track_metadata.cover_size = 0;

	return mux->track_count;

error:
	mp4_mux_track_destroy(track);
	return -ENOMEM;
}


MP4_API int mp4_mux_add_ref_to_track(struct mp4_mux *mux,
				     unsigned int track_handle,
				     unsigned int ref_track_handle)
{
	unsigned int i;
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ref_track_handle == 0, EINVAL);

	/* Get track */
	track = mp4_mux_track_find_by_handle(mux, track_handle);
	if (!track) {
		ULOGD("%s: no track found with handle = %" PRIu32,
		      __func__,
		      track_handle);
		return -ENOENT;
	}

	if (track->referenceTrackHandleCount >= MP4_TRACK_REF_MAX) {
		ULOGD("%s: track handle %" PRIu32
		      " reference track list is full",
		      __func__,
		      track_handle);
		return -ENOBUFS;
	}

	/* Check ref to add isn't already stored */
	for (i = 0; i < track->referenceTrackHandleCount; i++) {
		if (track->referenceTrackHandle[i] == ref_track_handle) {
			ULOGD("%s: reference to track handle %" PRIu32
			      " exist within track handle %" PRIu32,
			      __func__,
			      ref_track_handle,
			      track_handle);
			return 0;
		}
	}

	/* Store the ref track */
	track->referenceTrackHandle[track->referenceTrackHandleCount++] =
		ref_track_handle;

	/* If reference count changed, track needs to be written again */
	track->track_info_written = false;

	return 0;
}


MP4_API int
mp4_mux_track_set_video_decoder_config(struct mp4_mux *mux,
				       int track_handle,
				       struct mp4_video_decoder_config *vdc)
{
	struct mp4_mux_track *track;
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF((vdc->codec != MP4_VIDEO_CODEC_HEVC) &&
					 (vdc->codec != MP4_VIDEO_CODEC_AVC),
				 EINVAL);

	track = mp4_mux_track_find_by_handle(mux, track_handle);
	if (!track)
		return -ENOENT;
	if (track->type != MP4_TRACK_TYPE_VIDEO)
		return -EINVAL;

	track->video.codec = vdc->codec;

	track->video.width = vdc->width;
	track->video.height = vdc->height;

	switch (vdc->codec) {
	case MP4_VIDEO_CODEC_AVC:
		ret = mp4_mux_track_set_avc_decoder_config(track, vdc);
		break;
	case MP4_VIDEO_CODEC_HEVC:
		ret = mp4_mux_track_set_hevc_decoder_config(track, vdc);
		break;
	default:
		ULOGE("unsupported codec");
		ret = -ENOSYS;
		break;
	}

	return ret;
}


MP4_API int mp4_mux_track_set_audio_specific_config(struct mp4_mux *mux,
						    int track_handle,
						    const uint8_t *asc,
						    size_t asc_size,
						    uint32_t channel_count,
						    uint32_t sample_size,
						    float sample_rate)
{
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(asc == NULL || asc_size == 0, EINVAL);

	track = mp4_mux_track_find_by_handle(mux, track_handle);
	if (!track)
		return -ENOENT;
	if (track->type != MP4_TRACK_TYPE_AUDIO)
		return -EINVAL;

	track->audio.codec = MP4_AUDIO_CODEC_AAC_LC;
	track->audio.specific_config_size = asc_size;
	free(track->audio.specific_config);
	track->audio.specific_config = malloc(asc_size);
	if (track->audio.specific_config == NULL)
		return -ENOMEM;
	memcpy(track->audio.specific_config, asc, asc_size);

	track->audio.channel_count = channel_count;
	track->audio.sample_size = sample_size;
	track->audio.sample_rate = (uint32_t)(sample_rate * 65536);

	return 0;
}


MP4_API int mp4_mux_track_set_metadata_mime_type(struct mp4_mux *mux,
						 int track_handle,
						 const char *content_encoding,
						 const char *mime_type)
{
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);

	track = mp4_mux_track_find_by_handle(mux, track_handle);
	if (!track)
		return -ENOENT;
	if (track->type != MP4_TRACK_TYPE_METADATA &&
	    track->type != MP4_TRACK_TYPE_CHAPTERS)
		return -EINVAL;

	track->metadata.content_encoding = xstrdup(content_encoding);
	track->metadata.mime_type = xstrdup(mime_type);

	return 0;
}


static const char *mp4_mux_get_alternate_metadata_key(const char *key)
{
	static const struct {
		const char *base;
		const char *alt;
	} keys[] = {
		{MP4_META_KEY_FRIENDLY_NAME, MP4_UDTA_KEY_FRIENDLY_NAME},
		{MP4_META_KEY_TITLE, MP4_UDTA_KEY_TITLE},
		{MP4_META_KEY_COMMENT, MP4_UDTA_KEY_COMMENT},
		{MP4_META_KEY_COPYRIGHT, MP4_UDTA_KEY_COPYRIGHT},
		{MP4_META_KEY_MEDIA_DATE, MP4_UDTA_KEY_MEDIA_DATE},
		{MP4_META_KEY_LOCATION, MP4_UDTA_KEY_LOCATION},
		{MP4_META_KEY_MAKER, MP4_UDTA_KEY_MAKER},
		{MP4_META_KEY_MODEL, MP4_UDTA_KEY_MODEL},
		{MP4_META_KEY_SOFTWARE_VERSION, MP4_UDTA_KEY_SOFTWARE_VERSION},
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(keys); i++) {
		if (strcmp(key, keys[i].base) == 0)
			return keys[i].alt;
		if (strcmp(key, keys[i].alt) == 0)
			return keys[i].base;
	}
	return NULL;
}


static int mp4_mux_add_metadata_internal(struct mp4_mux *mux,
					 const char *key,
					 const char *value,
					 int user,
					 uint32_t track_handle)
{
	int ret = 0;
	int found = 0;
	struct mp4_mux_metadata *meta;
	struct mp4_mux_track *track = NULL;
	struct list_node *local_meta = NULL;

	enum mp4_mux_meta_storage storage = MP4_MUX_META_META;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(key == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(value == NULL, EINVAL);

	if (strncmp(key, "com.", 4) == 0) {
		/* _META_ key, store in moov/meta */
		storage = MP4_MUX_META_META;
	} else if (strlen(key) == 4) {
		/* _UDTA_ key, store in moov/udta/meta, except for location,
		 * stored in moov/udta */
		if (strcmp(key, MP4_UDTA_KEY_LOCATION) == 0)
			storage = MP4_MUX_META_UDTA_ROOT;
		else
			storage = MP4_MUX_META_UDTA;
	} else {
		return -EINVAL;
	}

	/* If a track id is given, we process track metadata */
	if (track_handle > 0) {
		/* get the mux_track with matching id */
		track = mp4_mux_track_find_by_handle(mux, track_handle);
		if (!track)
			return -ENOENT;

		local_meta = &track->metadatas;
	} else {
		/* If no track id is given, we process file metadata */
		local_meta = &mux->metadatas;
	}

	/* Search for a metadata with the same key */
	list_walk_entry_forward(local_meta, meta, node)
	{
		if (strcmp(key, meta->key) != 0)
			continue;
		/* We have a match. For user key, continue & override,
		 * otherwise, return success as we don't want to override the
		 * previous value */
		if (!user) {
			ULOGD("Metadata key %s was already set, skip", key);
			return 0;
		}
		if (track != NULL) {
			/* reset the counter to write this meta again */
			track->meta_write_count = 0;
		}
		ULOGD("Metadata key %s was already set, override", key);
		found = 1;
		free(meta->value);
		break;
	}

	/* If no matching entry, allocate a new one and add it to the list */
	if (!found) {
		meta = calloc(1, sizeof(*meta));
		if (!meta)
			return -ENOMEM;
		meta->key = strdup(key);
		meta->storage = storage;
		list_add_before(local_meta, &meta->node);
	}
	/* Update the value */
	meta->value = strdup(value);

	/* Finally, if the key is a user-given key, fill an alternate key/value
	 * pair if known, and not already set */
	if (user) {
		const char *alt = mp4_mux_get_alternate_metadata_key(key);
		if (alt != NULL) {
			ret = mp4_mux_add_metadata_internal(
				mux, alt, value, 0, track_handle);
		}
	}

	return ret;
}


MP4_API int mp4_mux_add_file_metadata(struct mp4_mux *mux,
				      const char *key,
				      const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(key == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(value == NULL, EINVAL);

	return mp4_mux_add_metadata_internal(mux, key, value, 1, 0);
}

MP4_API int mp4_mux_add_track_metadata(struct mp4_mux *mux,
				       uint32_t track_handle,
				       const char *key,
				       const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle > mux->track_count, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(key == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(value == NULL, EINVAL);

	return mp4_mux_add_metadata_internal(mux, key, value, 1, track_handle);
}

MP4_API int mp4_mux_set_file_cover(struct mp4_mux *mux,
				   enum mp4_metadata_cover_type cover_type,
				   const uint8_t *cover,
				   size_t cover_size)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover_type == MP4_METADATA_COVER_TYPE_UNKNOWN,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover_size == 0, EINVAL);

	free(mux->file_metadata.cover);
	mux->file_metadata.cover = calloc(1, cover_size);
	if (mux->file_metadata.cover == NULL)
		return -ENOMEM;

	memcpy(mux->file_metadata.cover, cover, cover_size);
	mux->file_metadata.cover_size = cover_size;
	mux->file_metadata.cover_type = cover_type;

	mux->recovery.thumb_written = false;

	return 0;
}

MP4_API int mp4_mux_track_add_sample(struct mp4_mux *mux,
				     int track_handle,
				     const struct mp4_mux_sample *sample)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample == NULL, EINVAL);

	const struct mp4_mux_scattered_sample sample_ = {
		.buffers = &sample->buffer,
		.len = &sample->len,
		.nbuffers = 1,
		.dts = sample->dts,
		.sync = sample->sync,
	};

	return mp4_mux_track_add_scattered_sample(mux, track_handle, &sample_);
}


MP4_API int mp4_mux_track_add_scattered_sample(
	struct mp4_mux *mux,
	int track_handle,
	const struct mp4_mux_scattered_sample *sample)
{
	int ret = 0;
	struct mp4_mux_track *track;
	struct iovec *iov;
	ssize_t written;
	ssize_t total_size = 0;
	off_t offset = 0;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_handle == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample == NULL, EINVAL);

	iov = calloc(sample->nbuffers, sizeof(*iov));
	if (!iov) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	track = mp4_mux_track_find_by_handle(mux, track_handle);
	if (!track) {
		ret = -ENOENT;
		ULOG_ERRNO("mp4_mux_track_find_by_handle", -ret);
		goto out;
	}

	for (int i = 0; i < sample->nbuffers; i++) {
		iov[i].iov_base = (void *)sample->buffers[i];
		iov[i].iov_len = sample->len[i];
		total_size += sample->len[i];
	}

	ULOGD("adding a %ssample of size %zu at dts %" PRIi64
	      " to track %d(type %d)",
	      sample->sync ? "sync " : "",
	      total_size,
	      sample->dts,
	      track_handle,
	      track->type);

	/* Grow arrays if needed */
	ret = mp4_mux_grow_samples(track, 1);
	if (ret != 0) {
		ULOG_ERRNO("mp4_mux_grow_samples", -ret);
		goto out;
	}
	ret = mp4_mux_grow_chunks(track, 1);
	if (ret != 0) {
		ULOG_ERRNO("mp4_mux_grow_chunks", -ret);
		goto out;
	}

	offset = lseek(mux->fd, 0, SEEK_CUR);
	if (offset == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto out;
	}

	track->samples.sizes[track->samples.count] = total_size;
	track->samples.decoding_times[track->samples.count] = sample->dts;
	track->samples.offsets[track->samples.count] = offset;

	track->chunks.offsets[track->chunks.count] = offset;

	if (sample->sync && track->type == MP4_TRACK_TYPE_VIDEO) {
		ret = mp4_mux_grow_sync(track, 1);
		if (ret != 0) {
			ULOG_ERRNO("mp4_mux_grow_sync", -ret);
			goto out;
		}
		track->sync.entries[track->sync.count] =
			track->samples.count + 1;
	}

	written = writev(mux->fd, iov, sample->nbuffers);
	if (written == -1 || written < total_size) {
		ret = -errno;
		if (written == -1) {
			ULOG_ERRNO("writev", -ret);
		} else {
			ret = -ENOSPC;
			ULOGE("writev: only %zu bytes written instead of %zu",
			      (size_t)written,
			      (size_t)total_size);
		}
		offset = lseek(mux->fd, offset, SEEK_SET);
		if (offset == -1)
			ULOG_ERRNO("lseek", errno);
		goto out;
	}


	track->samples.count++;
	track->chunks.count++;
	if (sample->sync && track->type == MP4_TRACK_TYPE_VIDEO)
		track->sync.count++;

out:
	if (iov)
		free(iov);
	return ret;
}


MP4_API void mp4_mux_dump(struct mp4_mux *mux)
{
	struct mp4_mux_track *track;
	unsigned int i;

	ULOG_ERRNO_RETURN_IF(mux == NULL, EINVAL);

	ULOGI("object MUX dump:");
	if (!mux) {
		ULOGI("NULL");
		return;
	}

	ULOGI("- %d tracks: {", mux->track_count);

	list_walk_entry_forward(&mux->tracks, track, node)
	{
		(void)mp4_mux_track_compute_tts(mux, track);
		(void)mp4_mux_sort_tracks(mux);
		ULOGI("  - track %" PRIu32 " (ID=%" PRIu32 ") of type %d: {",
		      track->handle,
		      track->id,
		      track->type);
		for (i = 0; i < track->referenceTrackHandleCount; i++) {
			struct mp4_mux_track *ref_track =
				mp4_mux_track_find_by_handle(
					mux, track->referenceTrackHandle[i]);
			if (ref_track == NULL) {
				ULOGE("mp4_mux_track_find_by_handle(%u)",
				      track->referenceTrackHandle[i]);
			} else {
				ULOGI("    - reference to track %" PRIu32
				      " (ID=%" PRIu32 ")",
				      ref_track->handle,
				      ref_track->id);
			}
		}
		ULOGI("    - samples[%d/%d]: {",
		      track->samples.count,
		      track->samples.capacity);
		for (uint32_t i = 0; i < track->samples.count; i++) {
			ULOGI("      - size:%10" PRIu32 ", offset:%10" PRIu64
			      ", dts:%10" PRIu64,
			      track->samples.sizes[i],
			      track->samples.offsets[i],
			      track->samples.decoding_times[i]);
		}
		ULOGI("    }");
		ULOGI("    - chunks[%d/%d]: {",
		      track->chunks.count,
		      track->chunks.capacity);
		for (uint32_t i = 0; i < track->chunks.count; i++) {
			ULOGI("      - offset:%" PRIu64,
			      track->chunks.offsets[i]);
		}
		ULOGI("    }");
		ULOGI("    - time_to_sample[%d/%d]: {",
		      track->time_to_sample.count,
		      track->time_to_sample.capacity);
		for (uint32_t i = 0; i < track->time_to_sample.count; i++) {
			ULOGI("      - count:%" PRIu32 ", delta:%" PRIu32,
			      track->time_to_sample.entries[i].sampleCount,
			      track->time_to_sample.entries[i].sampleDelta);
		}
		ULOGI("    }");
		ULOGI("    - sample_to_chunk[%d/%d]: {",
		      track->sample_to_chunk.count,
		      track->sample_to_chunk.capacity);
		for (uint32_t i = 0; i < track->sample_to_chunk.count; i++) {
			ULOGI("      - firstChunk:%" PRIu32 ", count:%" PRIu32
			      ", desc:%" PRIu32,
			      track->sample_to_chunk.entries[i].firstChunk,
			      track->sample_to_chunk.entries[i].samplesPerChunk,
			      track->sample_to_chunk.entries[i]
				      .sampleDescriptionIndex);
		}
		ULOGI("    }");
		ULOGI("    - sync[%d/%d]: {",
		      track->sync.count,
		      track->sync.capacity);
		for (uint32_t i = 0; i < track->sync.count; i++) {
			ULOGI("      - sample:%" PRIu32,
			      track->sync.entries[i]);
		}
		ULOGI("    }");
		ULOGI("  }");
	}
	ULOGI("}");

	ULOGI("- metadatas: {");

	struct mp4_mux_metadata *meta;
	list_walk_entry_forward(&mux->metadatas, meta, node)
	{
		ULOGI("  - %s :: %s [ type %d ]",
		      meta->key,
		      meta->value,
		      meta->storage);
	}
	ULOGI("}");
}
