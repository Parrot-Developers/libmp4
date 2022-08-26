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
#include "list.h"

#include <math.h>

#ifndef _WIN32
/* For fsync() */
#	include <unistd.h>
#endif

#define MP4_MUX_TABLES_GROW_SIZE 128
#define MP4_MUX_DEFAULT_TABLE_SIZE_MB 2


static struct mp4_mux_track *mp4_mux_get_track(struct mp4_mux *mux,
					       uint32_t track_id)
{
	struct mp4_mux_track *_track = NULL;

	if (track_id > mux->track_count)
		return NULL;

	struct list_node *start = &mux->tracks;
	custom_walk(start, _track, node, struct mp4_mux_track)
	{
		if (_track->id == track_id)
			return _track;
	}
	return NULL;
}


static int mp4_mux_grow_samples(struct mp4_mux_track *track, int new_samples)
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


static int mp4_mux_grow_chunks(struct mp4_mux_track *track, int new_chunks)
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


static int mp4_mux_grow_tts(struct mp4_mux_track *track, int new_tts)
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


static int mp4_mux_grow_stc(struct mp4_mux_track *track, int new_stc)
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


static int mp4_mux_grow_sync(struct mp4_mux_track *track, int new_sync)
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


static int mp4_mux_track_compute_tts(struct mp4_mux *mux,
				     struct mp4_mux_track *track)
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
		} else {
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

	struct list_node *start = &track->metadatas;
	custom_safe_walk(start, meta, tmp, node, struct mp4_mux_metadata)
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
	if (mux == NULL)
		return;

	if (mux->file)
		fclose(mux->file);

	free(mux->file_metadata.cover);

	struct list_node *start = &mux->tracks;
	custom_safe_walk(start, track, ttmp, node, struct mp4_mux_track)
	{
		mp4_mux_track_destroy(track);
	}
	
	start = &mux->metadatas;
	custom_safe_walk(start, meta, mtmp, node, struct mp4_mux_metadata)
	{
		free(meta->key);
		free(meta->value);
		list_del(&meta->node);
		free(meta);
	}

	free(mux);
}

static int 
mp4_mux_track_set_mp4v_decoder_config(struct mp4_mux_track *track,
						struct mp4_video_decoder_config *vdc)
{
	return 0;

	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc->codec != MP4_VIDEO_CODEC_AVC, EPROTO);
	// ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.c_sps == NULL, EINVAL);
	// ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.sps_size == 0, EINVAL);
	// ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.c_pps == NULL, EINVAL);
	// ULOG_ERRNO_RETURN_ERR_IF(vdc->avc.pps_size == 0, EINVAL);

	// track->video.avc.sps_size = vdc->avc.sps_size;
	// free(track->video.avc.sps);
	// track->video.avc.sps = malloc(vdc->avc.sps_size);
	// if (track->video.avc.sps == NULL) {
	// 	ret = -ENOMEM;
	// 	goto error;
	// }
	// memcpy(track->video.avc.sps, vdc->avc.c_sps, vdc->avc.sps_size);
	// track->video.avc.pps_size = vdc->avc.pps_size;
	// free(track->video.avc.pps);
	// track->video.avc.pps = malloc(vdc->avc.pps_size);
	// if (track->video.avc.pps == NULL) {
	// 	ret = -ENOMEM;
	// 	goto error;
	// }
	// memcpy(track->video.avc.pps, vdc->avc.c_pps, vdc->avc.pps_size);
error:
	mp4_video_decoder_config_destroy(&track->video);
	return ret;
	
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


MP4_API int mp4_mux_open(const char *filename,
			 uint32_t timescale,
			 uint64_t creation_time,
			 uint64_t modification_time,
			 struct mp4_mux **ret_obj)
{
	return mp4_mux_open2(filename,
			     timescale,
			     creation_time,
			     modification_time,
			     MP4_MUX_DEFAULT_TABLE_SIZE_MB,
			     ret_obj);
}


MP4_API int mp4_mux_open2(const char *filename,
			  uint32_t timescale,
			  uint64_t creation_time,
			  uint64_t modification_time,
			  uint32_t table_size_mbytes,
			  struct mp4_mux **ret_obj)
{
	struct mp4_mux *mux;
	off_t len;
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(filename == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(filename) == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(table_size_mbytes == 0, EINVAL);

	mux = calloc(1, sizeof(*mux));
	if (mux == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}

	list_init(&mux->tracks);
	list_init(&mux->metadatas);

	mux->file = fopen(filename, "wb");
	if (mux->file == NULL) {
		ret = -errno;
		ULOG_ERRNO("fopen:'%s'", -ret, filename);
		goto error;
	}
	mux->creation_time = creation_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	mux->modification_time =
		modification_time + MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	mux->timescale = timescale;

	mux->data_offset = table_size_mbytes * 1024 * 1024;

	mux->file_metadata.metadatas = &mux->metadatas;

	/* Write ftyp */
	len = mp4_box_ftyp_write(mux);
	if (len < 0) {
		ret = len;
		ULOG_ERRNO("mp4_box_ftyp_write", -ret);
		goto error;
	}
	mux->boxes_offset = len;

	/* Write initial free (for mov table) */
	len = mp4_box_free_write(mux, mux->data_offset - mux->boxes_offset);
	if (len < 0) {
		ret = len;
		ULOG_ERRNO("mp4_box_free_write", -ret);
		goto error;
	}

	/* Seek to dataOffset */
	ret = fseeko(mux->file, mux->data_offset, SEEK_SET);
	if (ret == -1) {
		ret = -errno;
		ULOG_ERRNO("fseeko", -ret);
		goto error;
	}
	/* Write mdat with size zero */
	len = mp4_box_mdat_write(mux, 0);
	if (len < 0) {
		ret = len;
		ULOG_ERRNO("mp4_box_mdat_write", -ret);
		goto error;
	}

	*ret_obj = mux;
	return 0;

error:
	mp4_mux_free(mux);
	return ret;
}


static int mp4_mux_sync_internal(struct mp4_mux *mux, bool allow_boxes_after)
{
	struct mp4_mux_track *track;
	struct mp4_mux_metadata *meta;
	struct mp4_box *moov;
	int ret;
	uint32_t duration = 0;
	ptrdiff_t end;
	uint64_t len;

	int has_meta_meta = 0;
	int has_meta_udta = 0;
	int has_meta_udta_root = 0;

	if (mux == NULL)
		return 0;

	/* Fix moov size */
	end = ftello(mux->file);
	if (end == -1) {
		ret = -errno;
		ULOG_ERRNO("ftello", -ret);
		goto out;
	}
	len = end - mux->data_offset - 8;
	ret = fseeko(mux->file, mux->data_offset, SEEK_SET);
	if (ret == -1) {
		ret = -errno;
		ULOG_ERRNO("fseeko", -ret);
		goto out;
	}
	end = mp4_box_mdat_write(mux, len);
	if (end < 0) {
		ret = end;
		ULOG_ERRNO("mp4_box_mdat_write", -ret);
		goto out;
	}
	
	struct list_node *start = &mux->tracks;
	custom_walk(start, track, node, struct mp4_mux_track)
	{
		ret = mp4_mux_track_compute_tts(mux, track);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_track_compute_tts(%d)",
				   -ret,
				   track->id);
			goto out;
		}
		if (track->duration_moov > duration)
			duration = track->duration_moov;
	}
	mux->duration = duration;

	moov = mp4_box_new_container(NULL, MP4_MOVIE_BOX); // level 1 box - moov
	if (moov == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	/* Fill the box */
	mp4_box_new_mvhd(moov, mux); // allocates memory for mp4_box which is child of moov box - type is mvhd

	start = &mux->tracks;
	custom_walk(start, track, node, struct mp4_mux_track)
	{
		/* Skip empty tracks */
		if (track->samples.count == 0)
			continue;
		struct mp4_box *trak, *mdia, *minf, *dinf, *stbl;
		trak = mp4_box_new_container(moov, MP4_TRACK_BOX); // moov.trak 
		if (trak == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_tkhd(trak, track) == NULL) { // moov.track.tkhd 
			ret = -ENOMEM;
			goto out;
		}
		if (track->referenceTrackIdCount > 0) {
			struct mp4_box *tref, *content;
			tref = mp4_box_new_container(trak,
						     MP4_TRACK_REFERENCE_BOX); // moov.trak.tref
			if (tref == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			switch (track->type) {
			case MP4_TRACK_TYPE_METADATA:
				content = mp4_box_new_cdsc(tref, track);
				break;
			default:
				/* Ref is not handled for non-metadata tracks */
				ret = -EINVAL;
				goto out;
			}
			if (content == NULL) {
				ret = -ENOMEM;
				goto out;
			}
		}
		mdia = mp4_box_new_container(trak, MP4_MEDIA_BOX); // moov.trak.mdia
		if (mdia == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_mdhd(mdia, track) == NULL) { // moov.trak.mdia.mdhd
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_hdlr(mdia, track) == NULL) { // moov.trak.mdia.hdlr
			ret = -ENOMEM;
			goto out;
		}
		minf = mp4_box_new_container(mdia, MP4_MEDIA_INFORMATION_BOX); // moov.track.mdia.minf
		if (minf == NULL) {
			ret = -ENOMEM;
			goto out;
		}

		has_meta_meta = 0;
		has_meta_udta = 0;
		has_meta_udta_root = 0;

		/* Metadata */
		struct list_node *start = &track->metadatas;
		custom_walk(start, meta, node, struct mp4_mux_metadata)
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
			if (mp4_box_new_meta(trak, &track->track_metadata) == // the metadata track is stored in the same track structure
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
			struct list_node *start = &track->metadatas;
			custom_walk(start, meta, node, struct mp4_mux_metadata)
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
			if (mp4_box_new_vmhd(minf, track) == NULL) { // moov.trak.mdia.minf.vmhd
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
		default:
			break;
		}
		dinf = mp4_box_new_container(minf, MP4_DATA_INFORMATION_BOX); // moov.trak.mdia.minf.dinf
		if (dinf == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		mp4_box_new_dref(dinf, track);
		stbl = mp4_box_new_container(minf, MP4_SAMPLE_TABLE_BOX); // moov.trak.mdia.minf.stbl
		if (stbl == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		if (mp4_box_new_stsd(stbl, track) == NULL) { // moov.trak.mdia.minf.stbl.stsd // this needs to be a container
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
	start = &mux->metadatas;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
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
		start = &mux->metadatas;
		custom_walk(start, meta, node, struct mp4_mux_metadata)
		{
			if (meta->storage != MP4_MUX_META_UDTA_ROOT)
				continue;
			if (mp4_box_new_udta_entry(udta, meta) == NULL) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	/* Write box at start */
	ret = fseeko(mux->file, mux->boxes_offset, SEEK_SET); // seek to start - we did while mp4_mux_open
	if (ret == -1) {
		ret = -errno;
		ULOG_ERRNO("fseeko", -ret);
		goto out;
	}
	ret = moov->writer.func(
		mux, moov, mux->data_offset - mux->boxes_offset); // whole header is written here - starting from moov box and all its children
	if (ret >= 0) {
		/* Written, pad with a free */
		end = mp4_box_free_write(
			mux, mux->data_offset - mux->boxes_offset - ret);
		if (end == -1) {
			ret = -end;
			ULOG_ERRNO("mp4_box_free_write", -ret);
			goto out;
		}
	} else if (ret == -ENOSPC && allow_boxes_after) {
		/* Not enough space, rewrite free, then put boxes at the end */
		ret = fseeko(mux->file, mux->boxes_offset, SEEK_SET);
		if (ret == -1) {
			ret = -errno;
			ULOG_ERRNO("fseeko", -ret);
			goto out;
		}
		end = mp4_box_free_write(mux,
					 mux->data_offset - mux->boxes_offset);
		if (end == -1) {
			ret = -end;
			ULOG_ERRNO("mp4_box_free_write", -ret);
			goto out;
		}
		ret = fseeko(mux->file, 0, SEEK_END);
		if (ret == -1) {
			ret = -errno;
			ULOG_ERRNO("fseeko", -ret);
			goto out;
		}
		ret = moov->writer.func(mux, moov, UINT32_MAX);
	}
	mp4_box_destroy(moov);

	if (ret < 0)
		ULOG_ERRNO("mp4_box_write", -ret);

out:
	/* Seek back to end */
	fseeko(mux->file, 0, SEEK_END);
	return ret;
}


MP4_API int mp4_mux_sync(struct mp4_mux *mux)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);

	ret = mp4_mux_sync_internal(mux, false);
	if (ret != 0)
		return ret;

	ret = fflush(mux->file);
	if (ret != 0)
		return -errno;
#ifndef _WIN32
	ret = fsync(fileno(mux->file));
	if (ret != 0)
		return -errno;
#else
	ULOGW("fsync not available, mp4 file not sync'ed on disk");
#endif

	return 0;
}


MP4_API int mp4_mux_close(struct mp4_mux *mux)
{
	int ret;

	if (mux == NULL)
		return 0;

	ret = mp4_mux_sync_internal(mux, true);

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
	ULOG_ERRNO_RETURN_ERR_IF(params->type != MP4_TRACK_TYPE_VIDEO &&
					 params->type != MP4_TRACK_TYPE_AUDIO &&
					 params->type !=
						 MP4_TRACK_TYPE_METADATA,
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
	track->id = mux->track_count;

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
				     unsigned int track_id,
				     unsigned int ref_track_id)
{
	unsigned int i;
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_id == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ref_track_id == 0, EINVAL);

	/* Get track */
	track = mp4_mux_get_track(mux, track_id);
	if (!track) {
		ULOGD("%s: no track found with id = %d", __func__, track_id);
		return -ENOENT;
	}


	if (track->referenceTrackIdCount >= MP4_TRACK_REF_MAX) {
		ULOGD("%s: track %d reference track list is full",
		      __func__,
		      track_id);
		return -ENOBUFS;
	}

	/* Check ref to add isn't already stored */
	for (i = 0; i < track->referenceTrackIdCount; i++) {
		if (track->referenceTrackId[i] == ref_track_id) {
			ULOGD("%s: reference to track %d exist within track %d",
			      __func__,
			      ref_track_id,
			      track_id);
			return 0;
		}
	}

	/* Store the ref track */
	track->referenceTrackId[track->referenceTrackIdCount++] = ref_track_id;

	return 0;
}


MP4_API int
mp4_mux_track_set_video_decoder_config(struct mp4_mux *mux,
				       int track_id,
				       struct mp4_video_decoder_config *vdc)
{
	struct mp4_mux_track *track;
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF((vdc->codec != MP4_VIDEO_CODEC_HEVC) &&
					 (vdc->codec != MP4_VIDEO_CODEC_AVC) && 
					 (vdc->codec != MP4_VIDEO_CODEC_MP4V),
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);

	track = mp4_mux_get_track(mux, track_id);
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
	case MP4_VIDEO_CODEC_MP4V:
		ret = mp4_mux_track_set_mp4v_decoder_config(track, vdc);
	default:
		ULOGE("unsupported codec");
		ret = -ENOSYS;
		break;
	}

	return ret;
}


MP4_API int mp4_mux_track_set_audio_specific_config(struct mp4_mux *mux,
						    int track_id,
						    const uint8_t *asc,
						    size_t asc_size,
						    uint32_t channel_count,
						    uint32_t sample_size,
						    float sample_rate)
{
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(asc == NULL || asc_size == 0, EINVAL);

	track = mp4_mux_get_track(mux, track_id);
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
						 int track_id,
						 const char *content_encoding,
						 const char *mime_type)
{
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);

	track = mp4_mux_get_track(mux, track_id);
	if (!track)
		return -ENOENT;
	if (track->type != MP4_TRACK_TYPE_METADATA)
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
					 uint32_t track_id)
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
	if (track_id > 0) {
		/* get the mux_track with matching id */
		track = mp4_mux_get_track(mux, track_id);
		if (!track)
			return -ENOENT;

		local_meta = &track->metadatas;
	} else {
		/* If no track id is given, we process file metadata */
		local_meta = &mux->metadatas;
	}

	/* Search for a metadata with the same key */
	struct list_node *start = local_meta;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
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
				mux, alt, value, 0, track_id);
		}
	}

	return ret;
}


MP4_API int mp4_mux_add_file_metadata(struct mp4_mux *mux,
				      const char *key,
				      const char *value)
{
	return mp4_mux_add_metadata_internal(mux, key, value, 1, 0);
}

MP4_API int mp4_mux_add_track_metadata(struct mp4_mux *mux,
				       uint32_t track_id,
				       const char *key,
				       const char *value)
{
	ULOG_ERRNO_RETURN_ERR_IF(track_id == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_id > mux->track_count, EINVAL);

	return mp4_mux_add_metadata_internal(mux, key, value, 1, track_id);
}

MP4_API int mp4_mux_set_file_cover(struct mp4_mux *mux,
				   enum mp4_metadata_cover_type cover_type,
				   const uint8_t *cover,
				   size_t cover_size)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover_type == MP4_METADATA_COVER_TYPE_UNKNOWN,
				 EINVAL);

	free(mux->file_metadata.cover);
	mux->file_metadata.cover = malloc(cover_size);
	if (mux->file_metadata.cover == NULL)
		return -ENOMEM;

	memcpy(mux->file_metadata.cover, cover, cover_size);
	mux->file_metadata.cover_size = cover_size;
	mux->file_metadata.cover_type = cover_type;

	return 0;
}

MP4_API int mp4_mux_track_add_sample(struct mp4_mux *mux,
				     int track_id,
				     const struct mp4_mux_sample *sample)
{
	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample == NULL, EINVAL);

	const struct mp4_mux_scattered_sample sample_ = {
		.buffers = &sample->buffer,
		.len = &sample->len,
		.nbuffers = 1,
		.dts = sample->dts,
		.sync = sample->sync,
	};

	return mp4_mux_track_add_scattered_sample(mux, track_id, &sample_);
}


MP4_API int mp4_mux_track_add_scattered_sample(
	struct mp4_mux *mux,
	int track_id,
	const struct mp4_mux_scattered_sample *sample)
{
	int ret = 0;
	struct mp4_mux_track *track;

	ULOG_ERRNO_RETURN_ERR_IF(mux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample == NULL, EINVAL);

	track = mp4_mux_get_track(mux, track_id);
	if (!track)
		return -ENOENT;

	size_t total_size = 0;
	for (int i = 0; i < sample->nbuffers; i++) // nbuffers is always 1 - whole frame
		total_size += sample->len[i]; // frame size 

	ULOGD("adding a %ssample of size %zu at dts %" PRIi64
	      " to track %d(type %d)",
	      sample->sync ? "sync " : "",
	      total_size,
	      sample->dts,
	      track_id,
	      track->type);

	/* Grow arrays if needed */
	ret = mp4_mux_grow_samples(track, 1);
	if (ret != 0)
		return ret;
	ret = mp4_mux_grow_chunks(track, 1);
	if (ret != 0)
		return ret;

	ptrdiff_t offset = ftello(mux->file);
	if (offset == -1) {
		ret = -errno;
		return ret;
	}

	track->samples.sizes[track->samples.count] = total_size;
	track->samples.decoding_times[track->samples.count] = sample->dts;
	track->samples.offsets[track->samples.count] = offset;

	track->chunks.offsets[track->chunks.count] = offset;

	if (sample->sync && track->type == MP4_TRACK_TYPE_VIDEO) { // if keyframe
		ret = mp4_mux_grow_sync(track, 1); // increase the sync array, where we track the keyframes
		if (ret != 0)
			return ret;
		track->sync.entries[track->sync.count] =
			track->samples.count + 1;
	}

	for (int i = 0; i < sample->nbuffers; i++) {
		ret = fwrite(sample->buffers[i], sample->len[i], 1, mux->file); // write the frame with size len[0] to the file.
		if (ret != 1) {
			offset = fseeko(mux->file, offset, SEEK_SET);
			if (offset == -1)
				ULOG_ERRNO("fseeko", errno);
			return -EIO;
		}
	}


	track->samples.count++;
	track->chunks.count++;
	if (sample->sync && track->type == MP4_TRACK_TYPE_VIDEO)
		track->sync.count++;

	return 0;
}


MP4_API void mp4_mux_dump(struct mp4_mux *mux)
{
	struct mp4_mux_track *track;
	unsigned int i;

	ULOGI("object MUX dump:");
	if (!mux) {
		ULOGI("NULL");
		return;
	}

	ULOGI("- %d tracks: {", mux->track_count);

	struct list_node *start = &mux->tracks;
	custom_walk(start, track, node, struct mp4_mux_track)
	{
		mp4_mux_track_compute_tts(mux, track);
		ULOGI("  - track %d of type %d: {", track->id, track->type);
		for (i = 0; i < track->referenceTrackIdCount; i++) {
			ULOGI("    - reference to track %" PRIu32,
			      track->referenceTrackId[i]);
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
	start = &mux->metadatas;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
	{
		ULOGI("  - %s :: %s [ type %d ]",
		      meta->key,
		      meta->value,
		      meta->storage);
	}
	ULOGI("}");
}
