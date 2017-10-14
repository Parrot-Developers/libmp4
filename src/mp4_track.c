/**
 * @file mp4_track.c
 * @brief MP4 file library - track related functions
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


void mp4_demux_free_tracks(
	struct mp4_demux *demux)
{
	struct mp4_track *tk = NULL, *next = NULL;

	for (tk = demux->track; tk; tk = next) {
		next = tk->next;
		free(tk->timeToSampleEntries);
		free(tk->sampleDecodingTime);
		free(tk->sampleSize);
		free(tk->chunkOffset);
		free(tk->sampleToChunkEntries);
		free(tk->sampleOffset);
		free(tk->videoSps);
		free(tk->videoPps);
		free(tk->metadataContentEncoding);
		free(tk->metadataMimeFormat);
		free(tk);
	}
}


int mp4_demux_is_sync_sample(
	struct mp4_demux *demux,
	struct mp4_track *track,
	unsigned int sampleIdx,
	int *prevSyncSampleIdx)
{
	unsigned int i;

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


int mp4_demux_build_tracks(
	struct mp4_demux *demux)
{
	struct mp4_track *tk = NULL, *videoTk = NULL;
	struct mp4_track *metaTk = NULL, *chapTk = NULL;
	int videoTrackCount = 0, audioTrackCount = 0, hintTrackCount = 0;
	int metadataTrackCount = 0, textTrackCount = 0;

	for (tk = demux->track; tk; tk = tk->next) {
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
			MP4_LOGE("sample count mismatch: %d vs. %d",
				sampleCount, tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleOffset = malloc(sampleCount * sizeof(uint64_t));
		MP4_RETURN_ERR_IF_FAILED((tk->sampleOffset != NULL), -ENOMEM);

		lastFirstChunk = 1;
		lastSamplesPerChunk = 0;
		for (i = 0, n = 0, chunkIdx = 0;
			i < tk->sampleToChunkEntryCount; i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				lastFirstChunk;
			for (j = 0; j < chunkCount; j++, chunkIdx++) {
				for (k = 0, offsetInChunk = 0;
					k < lastSamplesPerChunk; k++, n++) {
					tk->sampleOffset[n] =
						tk->chunkOffset[chunkIdx] +
						offsetInChunk;
					offsetInChunk += tk->sampleSize[n];
				}
			}
			lastFirstChunk =
				tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		for (j = 0; j < chunkCount; j++, chunkIdx++) {
			for (k = 0, offsetInChunk = 0;
				k < lastSamplesPerChunk; k++, n++) {
				tk->sampleOffset[n] =
					tk->chunkOffset[chunkIdx] +
					offsetInChunk;
				offsetInChunk += tk->sampleSize[n];
			}
		}

		for (i = 0, sampleCount = 0;
			i < tk->timeToSampleEntryCount; i++)
			sampleCount += tk->timeToSampleEntries[i].sampleCount;

		if (sampleCount != tk->sampleCount) {
			MP4_LOGE("sample count mismatch: %d vs. %d",
				sampleCount, tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleDecodingTime = malloc(sampleCount * sizeof(uint64_t));
		MP4_RETURN_ERR_IF_FAILED(
			(tk->sampleDecodingTime != NULL), -ENOMEM);

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

		/* link tracks using track references */
		if ((tk->referenceType != 0) && (tk->referenceTrackId)) {
			struct mp4_track *tkRef;
			int found = 0;
			for (tkRef = demux->track; tkRef; tkRef = tkRef->next) {
				if (tkRef->id == tk->referenceTrackId) {
					found = 1;
					break;
				}
			}
			if (found) {
				if ((tk->referenceType ==
					MP4_REFERENCE_TYPE_DESCRIPTION)
					&& (tk->type ==
					MP4_TRACK_TYPE_METADATA)) {
					tkRef->metadata = tk;
					tk->ref = tkRef;
				} else if ((tk->referenceType ==
					MP4_REFERENCE_TYPE_CHAPTERS) &&
					(tkRef->type == MP4_TRACK_TYPE_TEXT)) {
					tk->chapters = tkRef;
					tkRef->ref = tk;
					tkRef->type = MP4_TRACK_TYPE_CHAPTERS;
					chapTk = tkRef;
				}
			}
		}
	}

	/* workaround: if we have only 1 video track and 1 metadata
	 * track with no track reference, link them anyway */
	if ((videoTrackCount == 1) && (metadataTrackCount == 1)
			&& (audioTrackCount == 0) && (hintTrackCount == 0)
			&& (videoTk) && (metaTk) && (!videoTk->metadata)) {
		videoTk->metadata = metaTk;
		metaTk->ref = videoTk;
	}

	/* build the chapter list */
	if (chapTk) {
		unsigned int i;
		for (i = 0; i < chapTk->sampleCount; i++) {
			unsigned int sampleSize, readBytes = 0;
			uint16_t sz;
			sampleSize = chapTk->sampleSize[i];
			int _ret = fseeko(demux->file,
				chapTk->sampleOffset[i], SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIu64
				" bytes forward in file",
				chapTk->sampleOffset[i]);
			MP4_READ_16(demux->file, sz, readBytes);
			sz = ntohs(sz);
			if (sz <= sampleSize - readBytes) {
				char *chapName = malloc(sz + 1);
				MP4_RETURN_ERR_IF_FAILED(
					(chapName != NULL), -ENOMEM);
				demux->chaptersName[demux->chaptersCount] =
					chapName;
				size_t count = fread(
					chapName, sz, 1, demux->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %u bytes from file",
					sz);
				readBytes += sz;
				chapName[sz] = '\0';
				uint64_t chapTime =
					(chapTk->sampleDecodingTime[i] *
					1000000 + chapTk->timescale / 2) /
					chapTk->timescale;
				MP4_LOGD("chapter #%d time=%" PRIu64 " '%s'",
					demux->chaptersCount + 1,
					chapTime, chapName);
				demux->chaptersTime[demux->chaptersCount] =
					chapTime;
				demux->chaptersCount++;
			}
		}
	}

	return 0;
}
