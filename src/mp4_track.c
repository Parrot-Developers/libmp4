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


int mp4_track_is_sync_sample(struct mp4_track *track,
			     unsigned int sampleIdx,
			     int *prevSyncSampleIdx)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (!track->syncSampleEntries)
		return 1;

	for (i = 0; i < track->syncSampleEntryCount; i++) {
		if (track->syncSampleEntries[i] - 1 == sampleIdx)
			return 1;
		else if (track->syncSampleEntries[i] - 1 > sampleIdx) {
			if ((prevSyncSampleIdx) && (i > 0)) {
				*prevSyncSampleIdx =
					track->syncSampleEntries[i - 1] - 1;
			}
			return 0;
		}
	}

	if ((prevSyncSampleIdx) && (i > 0))
		*prevSyncSampleIdx = track->syncSampleEntries[i - 1] - 1;
	return 0;
}


int mp4_track_find_sample_by_time(struct mp4_track *track,
				  uint64_t time,
				  enum mp4_time_cmp cmp,
				  int sync,
				  int start)
{
	int i, idx, is_sync, found = 0;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	switch (cmp) {
	case MP4_TIME_CMP_EXACT:
		if (start < 0)
			start = 0;
		if (start >= (int)track->sampleCount)
			start = (int)track->sampleCount - 1;
		for (i = start; i < (int)track->sampleCount; i++, is_sync = 0) {
			if (track->sampleDecodingTime[i] == time) {
				if (sync) {
					is_sync = mp4_track_is_sync_sample(
						track, i, NULL);
				}
				if ((!sync) || (is_sync)) {
					idx = i;
					found = 1;
					break;
				}
			} else if (track->sampleDecodingTime[i] > time) {
				break;
			}
		}
		break;
	case MP4_TIME_CMP_LT:
	case MP4_TIME_CMP_LT_EQ:
		if (start < 0)
			start = (int)track->sampleCount - 1;
		if (start >= (int)track->sampleCount)
			start = (int)track->sampleCount - 1;
		for (i = start; i >= 0; i--, is_sync = 0) {
			if (((cmp == MP4_TIME_CMP_LT) &&
			     (track->sampleDecodingTime[i] < time)) ||
			    ((cmp == MP4_TIME_CMP_LT_EQ) &&
			     (track->sampleDecodingTime[i] <= time))) {
				if (sync) {
					is_sync = mp4_track_is_sync_sample(
						track, i, NULL);
				}
				if ((!sync) || (is_sync)) {
					idx = i;
					found = 1;
					break;
				}
			}
		}
		break;
	case MP4_TIME_CMP_GT:
	case MP4_TIME_CMP_GT_EQ:
		if (start < 0)
			start = 0;
		if (start >= (int)track->sampleCount)
			start = (int)track->sampleCount - 1;
		for (i = start; i < (int)track->sampleCount; i++, is_sync = 0) {
			if (((cmp == MP4_TIME_CMP_GT) &&
			     (track->sampleDecodingTime[i] > time)) ||
			    ((cmp == MP4_TIME_CMP_GT_EQ) &&
			     (track->sampleDecodingTime[i] >= time))) {
				if (sync) {
					is_sync = mp4_track_is_sync_sample(
						track, i, NULL);
				}
				if ((!sync) || (is_sync)) {
					idx = i;
					found = 1;
					break;
				}
			}
		}
		break;
	default:
		ULOGE("unsupported comparison type: %d", cmp);
		return -EINVAL;
	}

	return (found) ? idx : -ENOENT;
}


static struct mp4_track *mp4_track_new(void)
{
	struct mp4_track *track = calloc(1, sizeof(*track));
	if (track == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return NULL;
	}
	list_node_unref(&track->node);

	return track;
}


static int mp4_track_destroy(struct mp4_track *track)
{
	if (track == NULL)
		return 0;

	mp4_video_decoder_config_destroy(&track->vdc);
	free(track->timeToSampleEntries);
	free(track->sampleDecodingTime);
	free(track->sampleSize);
	free(track->chunkOffset);
	free(track->sampleToChunkEntries);
	free(track->sampleOffset);
	free(track->syncSampleEntries);
	free(track->audioSpecificConfig);
	free(track->contentEncoding);
	free(track->mimeFormat);
	for (unsigned int i = 0; i < track->staticMetadataCount; i++) {
		free(track->staticMetadataKey[i]);
		free(track->staticMetadataValue[i]);
	}
	free(track->staticMetadataKey);
	free(track->staticMetadataValue);
	free(track->name);
	free(track);

	return 0;
}


struct mp4_track *mp4_track_add(struct mp4_file *mp4)
{
	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	struct mp4_track *track = mp4_track_new();
	if (track == NULL) {
		ULOG_ERRNO("mp4_track_new", ENOMEM);
		return NULL;
	}
	list_node_unref(&track->node); /* TODO: remove */

	/* Add to the list */
	list_add_after(list_last(&mp4->tracks), &track->node);
	mp4->trackCount++;

	return track;
}


int mp4_track_remove(struct mp4_file *mp4, struct mp4_track *track)
{
	struct mp4_track *_track = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(mp4 == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	_track = mp4_track_find(mp4, track);
	if (_track != track) {
		ULOG_ERRNO("mp4_track_find", ENOENT);
		return -ENOENT;
	}

	/* Remove from the list */
	list_del(&track->node);
	mp4->trackCount--;

	return mp4_track_destroy(track);
}


struct mp4_track *mp4_track_find(struct mp4_file *mp4, struct mp4_track *track)
{
	struct mp4_track *_track = NULL;
	int found = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(track == NULL, EINVAL, NULL);

	struct list_node *start = &mp4->tracks;
	custom_walk(start, _track, node, struct mp4_track)
	{
		if (_track == track) {
			found = 1;
			break;
		}
	}

	if (!found)
		return NULL;

	return _track;
}


struct mp4_track *mp4_track_find_by_idx(struct mp4_file *mp4,
					unsigned int track_idx)
{
	struct mp4_track *_track = NULL;
	int found = 0;
	unsigned int k = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	struct list_node *start = &mp4->tracks;
	custom_walk(start, _track, node, struct mp4_track)
	{
		if (k == track_idx) {
			found = 1;
			break;
		}
		k++;
	}

	if (!found)
		return NULL;

	return _track;
}


struct mp4_track *mp4_track_find_by_id(struct mp4_file *mp4,
				       unsigned int track_id)
{
	struct mp4_track *_track = NULL;
	int found = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	struct list_node *start = &mp4->tracks;
	custom_walk(start, _track, node, struct mp4_track)
	{
		if (_track->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return NULL;

	return _track;
}


void mp4_tracks_destroy(struct mp4_file *mp4)
{
	struct mp4_track *track = NULL, *tmp = NULL;

	ULOG_ERRNO_RETURN_IF(mp4 == NULL, EINVAL);

	struct list_node *start = &mp4->tracks;
	custom_safe_walk(start, track, tmp, node, struct mp4_track)
	{
		mp4_track_destroy(track);
	}
}


int mp4_tracks_build(struct mp4_file *mp4)
{
	struct mp4_track *tk = NULL, *videoTk = NULL;
	struct mp4_track *metaTk = NULL, *chapTk = NULL;
	int videoTrackCount = 0, audioTrackCount = 0, hintTrackCount = 0;
	int metadataTrackCount = 0, textTrackCount = 0;

	ULOG_ERRNO_RETURN_ERR_IF(mp4 == NULL, EINVAL);

	struct list_node *start = &mp4->tracks;
	custom_walk(start, tk, node, struct mp4_track)
	{
		unsigned int i, j, k, n;
		uint32_t lastFirstChunk = 1, lastSamplesPerChunk = 0;
		uint32_t chunkCount, sampleCount = 0, chunkIdx;
		uint64_t offsetInChunk;
		for (i = 0; i < tk->sampleToChunkEntryCount; i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				     lastFirstChunk;
			sampleCount += chunkCount * lastSamplesPerChunk;
			lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		sampleCount += chunkCount * lastSamplesPerChunk;

		if (sampleCount != tk->sampleCount) {
			ULOGE("sample count mismatch: %d, expected %d",
			      sampleCount,
			      tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleOffset = malloc(sampleCount * sizeof(uint64_t));
		if (tk->sampleOffset == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}

		lastFirstChunk = 1;
		lastSamplesPerChunk = 0;
		for (i = 0, n = 0, chunkIdx = 0;
		     i < tk->sampleToChunkEntryCount;
		     i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				     lastFirstChunk;
			for (j = 0; j < chunkCount; j++, chunkIdx++) {
				for (k = 0, offsetInChunk = 0;
				     k < lastSamplesPerChunk;
				     k++, n++) {
					tk->sampleOffset[n] =
						tk->chunkOffset[chunkIdx] +
						offsetInChunk;
					offsetInChunk += tk->sampleSize[n];
				}
			}
			lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		for (j = 0; j < chunkCount; j++, chunkIdx++) {
			for (k = 0, offsetInChunk = 0; k < lastSamplesPerChunk;
			     k++, n++) {
				tk->sampleOffset[n] =
					tk->chunkOffset[chunkIdx] +
					offsetInChunk;
				offsetInChunk += tk->sampleSize[n];
			}
		}

		for (i = 0, sampleCount = 0; i < tk->timeToSampleEntryCount;
		     i++)
			sampleCount += tk->timeToSampleEntries[i].sampleCount;

		if (sampleCount != tk->sampleCount) {
			ULOGE("sample count mismatch: %d, expected %d",
			      sampleCount,
			      tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleDecodingTime = malloc(sampleCount * sizeof(uint64_t));
		if (tk->sampleDecodingTime == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}

		uint64_t ts = 0;
		for (i = 0, k = 0; i < tk->timeToSampleEntryCount; i++) {
			for (j = 0; j < tk->timeToSampleEntries[i].sampleCount;
			     j++, k++) {
				tk->sampleDecodingTime[k] = ts;
				ts += tk->timeToSampleEntries[i].sampleDelta;
			}
		}

		switch (tk->type) {
		case MP4_TRACK_TYPE_VIDEO:
			videoTrackCount++;
			videoTk = tk;
			break;
		case MP4_TRACK_TYPE_AUDIO:
			audioTrackCount++;
			break;
		case MP4_TRACK_TYPE_HINT:
			hintTrackCount++;
			break;
		case MP4_TRACK_TYPE_METADATA:
			metadataTrackCount++;
			metaTk = tk;
			break;
		case MP4_TRACK_TYPE_TEXT:
			textTrackCount++;
			break;
		default:
			break;
		}

		/* Link tracks using track references */
		for (i = 0; i < tk->referenceTrackIdCount; i++) {
			struct mp4_track *tkRef;
			tkRef = mp4_track_find_by_id(mp4,
						     tk->referenceTrackId[i]);
			if (tkRef == NULL) {
				ULOGW("track reference: track ID %d not found",
				      tk->referenceTrackId[i]);
				continue;
			}

			if ((tk->referenceType ==
			     MP4_REFERENCE_TYPE_DESCRIPTION) &&
			    (tk->type == MP4_TRACK_TYPE_METADATA)) {
				tkRef->metadata = tk;
			} else if ((tk->referenceType ==
				    MP4_REFERENCE_TYPE_CHAPTERS) &&
				   (tkRef->type == MP4_TRACK_TYPE_TEXT)) {
				tk->chapters = tkRef;
				tkRef->type = MP4_TRACK_TYPE_CHAPTERS;
				chapTk = tkRef;
			}
		}
	}

	/* Workaround: if we have only 1 video track and 1 metadata
	 * track with no track reference, link them anyway */
	if ((videoTrackCount == 1) && (metadataTrackCount == 1) &&
	    (audioTrackCount == 0) && (hintTrackCount == 0) && (videoTk) &&
	    (metaTk) && (!videoTk->metadata))
		videoTk->metadata = metaTk;

	/* Build the chapter list */
	if (chapTk) {
		unsigned int i;
		for (i = 0; i < chapTk->sampleCount; i++) {
			unsigned int sampleSize, readBytes = 0;
			uint16_t sz;
			sampleSize = chapTk->sampleSize[i];
			int _ret = fseeko(
				mp4->file, chapTk->sampleOffset[i], SEEK_SET);
			if (_ret != 0) {
				ULOG_ERRNO("fseeko", errno);
				return -errno;
			}
			MP4_READ_16(mp4->file, sz, readBytes);
			sz = ntohs(sz);
			if (sz <= sampleSize - readBytes) {
				char *chapName = malloc(sz + 1);
				if (chapName == NULL)
					return -ENOMEM;
				mp4->chaptersName[mp4->chaptersCount] =
					chapName;
				size_t count =
					fread(chapName, sz, 1, mp4->file);
				if (count != 1) {
					ULOG_ERRNO("fread", EIO);
					return -EIO;
				}
				readBytes += sz;
				chapName[sz] = '\0';
				uint64_t chapTime = mp4_sample_time_to_usec(
					chapTk->sampleDecodingTime[i],
					chapTk->timescale);
				ULOGD("chapter #%d time=%" PRIu64 " '%s'",
				      mp4->chaptersCount + 1,
				      chapTime,
				      chapName);
				mp4->chaptersTime[mp4->chaptersCount] =
					chapTime;
				mp4->chaptersCount++;
			}
		}
	}

	return 0;
}
