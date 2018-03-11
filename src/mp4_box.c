/**
 * @file mp4_box.c
 * @brief MP4 file library - box read/write functions
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


struct mp4_box *mp4_box_new(
	struct mp4_box *parent)
{
	struct mp4_box *box = calloc(1, sizeof(*box));
	MP4_RETURN_VAL_IF_FAILED(box != NULL, -ENOMEM, NULL);
	list_node_unref(&box->node);
	list_init(&box->children);

	box->parent = parent;
	if (parent)
		list_add_after(list_last(&parent->children), &box->node);

	return box;
}


int mp4_box_destroy(
	struct mp4_box *box)
{
	MP4_RETURN_ERR_IF_FAILED(box != NULL, -EINVAL);

	struct mp4_box *child = NULL, *tmp = NULL;
	list_walk_entry_forward_safe(&box->children, child, tmp, node) {
		int ret = mp4_box_destroy(child);
		if (ret != 0)
			MP4_LOGE("mp4_box_destroy() failed: %d(%s)",
				ret, strerror(-ret));
	}

	if (list_node_is_ref(&box->node))
		list_del(&box->node);
	free(box);
	return 0;
}


void mp4_box_log(
	struct mp4_box *box,
	int indent,
	int level)
{
	char spaces[101];
	memset(spaces, ' ', sizeof(spaces));
	if (indent > 50)
		indent = 50;
	spaces[indent * 2] = '\0';
	char a = (char)((box->type >> 24) & 0xFF);
	char b = (char)((box->type >> 16) & 0xFF);
	char c = (char)((box->type >> 8) & 0xFF);
	char d = (char)(box->type & 0xFF);
	ULOG_PRI(level, "%s- %c%c%c%c size %" PRIu64 "\n", spaces,
		(a >= 32) ? a : '.', (b >= 32) ? b : '.',
		(c >= 32) ? c : '.', (d >= 32) ? d : '.',
		(box->size == 1) ?
		box->largesize : box->size);

	struct mp4_box *child = NULL;
	list_walk_entry_forward(&box->children, child, node)
		mp4_box_log(child, indent + 1, level);
}


static off_t mp4_box_ftyp_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* major_brand */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t majorBrand = ntohl(val32);
	MP4_LOGD("# ftyp: major_brand=%c%c%c%c",
		(char)((majorBrand >> 24) & 0xFF),
		(char)((majorBrand >> 16) & 0xFF),
		(char)((majorBrand >> 8) & 0xFF),
		(char)(majorBrand & 0xFF));

	/* minor_version */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t minorVersion = ntohl(val32);
	MP4_LOGD("# ftyp: minor_version=%" PRIu32, minorVersion);

	int k = 0;
	while (boxReadBytes + 4 <= maxBytes) {
		/* compatible_brands[] */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t compatibleBrands = ntohl(val32);
		MP4_LOGD("# ftyp: compatible_brands[%d]=%c%c%c%c", k,
			(char)((compatibleBrands >> 24) & 0xFF),
			(char)((compatibleBrands >> 16) & 0xFF),
			(char)((compatibleBrands >> 8) & 0xFF),
			(char)(compatibleBrands & 0xFF));
		k++;
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_mvhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 25 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 25 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# mvhd: version=%d", version);
	MP4_LOGD("# mvhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 28 * 4),
			-EINVAL, "invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes, 28 * 4);

		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->creationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mvhd: creation_time=%" PRIu64,
			mp4->creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->modificationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mvhd: modification_time=%" PRIu64,
			mp4->modificationTime);

		/* timescale */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->timescale = ntohl(val32);
		MP4_LOGD("# mvhd: timescale=%" PRIu32, mp4->timescale);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			mp4->duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->creationTime = ntohl(val32);
		MP4_LOGD("# mvhd: creation_time=%" PRIu64,
			mp4->creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->modificationTime = ntohl(val32);
		MP4_LOGD("# mvhd: modification_time=%" PRIu64,
			mp4->modificationTime);

		/* timescale */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->timescale = ntohl(val32);
		MP4_LOGD("# mvhd: timescale=%" PRIu32, mp4->timescale);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		mp4->duration = ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(mp4->duration + mp4->timescale / 2) /
			mp4->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			mp4->duration, hrs, min, sec);
	}

	/* rate */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float rate = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# mvhd: rate=%.4f", rate);

	/* volume & reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	MP4_LOGD("# mvhd: volume=%.2f", volume);

	/* reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* matrix */
	int k;
	for (k = 0; k < 9; k++)
		MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* pre_defined */
	for (k = 0; k < 6; k++)
		MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* next_track_ID */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t next_track_ID = ntohl(val32);
	MP4_LOGD("# mvhd: next_track_ID=%" PRIu32, next_track_ID);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_tkhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 21 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 21 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# tkhd: version=%d", version);
	MP4_LOGD("# tkhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24 * 4),
			-EINVAL, "invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes, 24 * 4);

		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# tkhd: creation_time=%" PRIu64,
			creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# tkhd: modification_time=%" PRIu64,
			modificationTime);

		/* track_ID */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->id = ntohl(val32);
		MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

		/* reserved */
		MP4_READ_32(mp4->file, val32, boxReadBytes);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint64_t duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# tkhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t creationTime = ntohl(val32);
		MP4_LOGD("# tkhd: creation_time=%" PRIu32,
			creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t modificationTime = ntohl(val32);
		MP4_LOGD("# tkhd: modification_time=%" PRIu32,
			modificationTime);

		/* track_ID */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->id = ntohl(val32);
		MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

		/* reserved */
		MP4_READ_32(mp4->file, val32, boxReadBytes);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t duration = ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(duration + mp4->timescale / 2) /
			mp4->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# tkhd: duration=%" PRIu32 " (%02d:%02d:%02d)",
			duration, hrs, min, sec);
	}

	/* reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* layer & alternate_group */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	int16_t layer = (int16_t)(ntohl(val32) >> 16);
	int16_t alternateGroup = (int16_t)(ntohl(val32) & 0xFFFF);
	MP4_LOGD("# tkhd: layer=%i", layer);
	MP4_LOGD("# tkhd: alternate_group=%i", alternateGroup);

	/* volume & reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	MP4_LOGD("# tkhd: volume=%.2f", volume);

	/* matrix */
	int k;
	for (k = 0; k < 9; k++)
		MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* width */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float width = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# tkhd: width=%.2f", width);

	/* height */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float height = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# tkhd: height=%.2f", height);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_tref_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 3 * 4);

	/* reference type size */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t referenceTypeSize = ntohl(val32);
	MP4_LOGD("# tref: reference_type_size=%" PRIu32, referenceTypeSize);

	/* reference type */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->referenceType = ntohl(val32);
	MP4_LOGD("# tref: reference_type=%c%c%c%c",
		(char)((track->referenceType >> 24) & 0xFF),
		(char)((track->referenceType >> 16) & 0xFF),
		(char)((track->referenceType >> 8) & 0xFF),
		(char)(track->referenceType & 0xFF));

	/* track IDs */
	/* NB: only read the first track ID, ignore multiple references */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->referenceTrackId = ntohl(val32);
	MP4_LOGD("# tref: track_id=%" PRIu32, track->referenceTrackId);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_mdhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 6 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# mdhd: version=%d", version);
	MP4_LOGD("# mdhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9 * 4),
			-EINVAL, "invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes, 9 * 4);

		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mdhd: creation_time=%" PRIu64,
			track->creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->modificationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mdhd: modification_time=%" PRIu64,
			track->modificationTime);

		/* timescale */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			track->duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->creationTime = ntohl(val32);
		MP4_LOGD("# mdhd: creation_time=%" PRIu64,
			track->creationTime);

		/* modification_time */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->modificationTime = ntohl(val32);
		MP4_LOGD("# mdhd: modification_time=%" PRIu64,
			track->modificationTime);

		/* timescale */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

		/* duration */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			track->duration, hrs, min, sec);
	}

	/* language & pre_defined */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint16_t language = (uint16_t)(ntohl(val32) >> 16) & 0x7FFF;
	MP4_LOGD("# mdhd: language=%" PRIu16, language);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_vmhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 3 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# vmhd: version=%d", version);
	MP4_LOGD("# vmhd: flags=%" PRIu32, flags);

	/* graphicsmode */
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	uint16_t graphicsmode = ntohs(val16);
	MP4_LOGD("# vmhd: graphicsmode=%" PRIu16, graphicsmode);

	/* opcolor */
	uint16_t opcolor[3];
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	opcolor[0] = ntohs(val16);
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	opcolor[1] = ntohs(val16);
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	opcolor[2] = ntohs(val16);
	MP4_LOGD("# vmhd: opcolor=(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
		opcolor[0], opcolor[1], opcolor[2]);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_smhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 2 * 4), -EINVAL,
			"invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes, 2 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# smhd: version=%d", version);
	MP4_LOGD("# smhd: flags=%" PRIu32, flags);

	/* balance & reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	float balance = (float)(
		(int16_t)((ntohl(val32) >> 16) & 0xFFFF)) / 256.;
	MP4_LOGD("# smhd: balance=%.2f", balance);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_hmhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 5 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 5 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# hmhd: version=%d", version);
	MP4_LOGD("# hmhd: flags=%" PRIu32, flags);

	/* maxPDUsize & avgPDUsize */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint16_t maxPDUsize = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	uint16_t avgPDUsize = (uint16_t)(ntohl(val32) & 0xFFFF);
	MP4_LOGD("# hmhd: maxPDUsize=%" PRIu16, maxPDUsize);
	MP4_LOGD("# hmhd: avgPDUsize=%" PRIu16, avgPDUsize);

	/* maxbitrate */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t maxbitrate = ntohl(val32);
	MP4_LOGD("# hmhd: maxbitrate=%" PRIu32, maxbitrate);

	/* avgbitrate */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t avgbitrate = ntohl(val32);
	MP4_LOGD("# hmhd: avgbitrate=%" PRIu32, avgbitrate);

	/* reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_nmhd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# nmhd: version=%d", version);
	MP4_LOGD("# nmhd: flags=%" PRIu32, flags);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_hdlr_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 6 * 4);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# hdlr: version=%d", version);
	MP4_LOGD("# hdlr: flags=%" PRIu32, flags);

	/* pre_defined */
	MP4_READ_32(mp4->file, val32, boxReadBytes);

	/* handler_type */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t handlerType = ntohl(val32);
	MP4_LOGD("# hdlr: handler_type=%c%c%c%c",
		(char)((handlerType >> 24) & 0xFF),
		(char)((handlerType >> 16) & 0xFF),
		(char)((handlerType >> 8) & 0xFF),
		(char)(handlerType & 0xFF));

	if ((track) && (box) && (box->parent) &&
		(box->parent->type == MP4_MEDIA_BOX)) {
		switch (handlerType) {
		case MP4_HANDLER_TYPE_VIDEO:
			track->type = MP4_TRACK_TYPE_VIDEO;
			break;
		case MP4_HANDLER_TYPE_AUDIO:
			track->type = MP4_TRACK_TYPE_AUDIO;
			break;
		case MP4_HANDLER_TYPE_HINT:
			track->type = MP4_TRACK_TYPE_HINT;
			break;
		case MP4_HANDLER_TYPE_METADATA:
			track->type = MP4_TRACK_TYPE_METADATA;
			break;
		case MP4_HANDLER_TYPE_TEXT:
			track->type = MP4_TRACK_TYPE_TEXT;
			break;
		default:
			track->type = MP4_TRACK_TYPE_UNKNOWN;
			break;
		}
	}

	/* reserved */
	unsigned int k;
	for (k = 0; k < 3; k++)
		MP4_READ_32(mp4->file, val32, boxReadBytes);

	char name[100];
	for (k = 0; (k < sizeof(name) - 1) && (boxReadBytes < maxBytes); k++) {
		MP4_READ_8(mp4->file, name[k], boxReadBytes);
		if (name[k] == '\0')
			break;
	}
	name[k + 1] = '\0';
	MP4_LOGD("# hdlr: name=%s", name);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_avcc_read(
	struct mp4_file *mp4,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0, minBytes = 6;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* version & profile & level */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	val32 = htonl(val32);
	uint8_t version = (val32 >> 24) & 0xFF;
	uint8_t profile = (val32 >> 16) & 0xFF;
	uint8_t profile_compat = (val32 >> 8) & 0xFF;
	uint8_t level = val32 & 0xFF;
	MP4_LOGD("# avcC: version=%d", version);
	MP4_LOGD("# avcC: profile=%d", profile);
	MP4_LOGD("# avcC: profile_compat=%d", profile_compat);
	MP4_LOGD("# avcC: level=%d", level);

	/* length_size & sps_count */
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	val16 = htons(val16);
	uint8_t lengthSize = ((val16 >> 8) & 0x3) + 1;
	uint8_t spsCount = val16 & 0x1F;
	MP4_LOGD("# avcC: length_size=%d", lengthSize);
	MP4_LOGD("# avcC: sps_count=%d", spsCount);

	minBytes += 2 * spsCount;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	int i;
	for (i = 0; i < spsCount; i++) {
		/* sps_length */
		MP4_READ_16(mp4->file, val16, boxReadBytes);
		uint16_t spsLength = htons(val16);
		MP4_LOGD("# avcC: sps_length=%" PRIu16, spsLength);

		minBytes += spsLength;
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes),
			-EINVAL, "invalid size: %" PRIi64 " expected %"
			PRIi64 " min", (int64_t)maxBytes, (int64_t)minBytes);

		if ((!track->videoSps) && (spsLength)) {
			/* first SPS found */
			track->videoSpsSize = spsLength;
			track->videoSps = malloc(spsLength);
			MP4_RETURN_ERR_IF_FAILED((track->videoSps != NULL),
				-ENOMEM);
			size_t count = fread(track->videoSps, spsLength,
				1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", spsLength);
		} else {
			/* ignore any other SPS */
			int ret = fseeko(mp4->file, spsLength, SEEK_CUR);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
				"failed to seek %u bytes forward in file",
				spsLength);
		}
		boxReadBytes += spsLength;
	}

	minBytes++;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* pps_count */
	uint8_t ppsCount;
	MP4_READ_8(mp4->file, ppsCount, boxReadBytes);
	MP4_LOGD("# avcC: pps_count=%d", ppsCount);

	minBytes += 2 * ppsCount;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	for (i = 0; i < ppsCount; i++) {
		/* pps_length */
		MP4_READ_16(mp4->file, val16, boxReadBytes);
		uint16_t ppsLength = htons(val16);
		MP4_LOGD("# avcC: pps_length=%" PRIu16, ppsLength);

		minBytes += ppsLength;
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes),
			-EINVAL, "invalid size: %" PRIi64 " expected %"
			PRIi64 " min", (int64_t)maxBytes, (int64_t)minBytes);

		if ((!track->videoPps) && (ppsLength)) {
			/* first PPS found */
			track->videoPpsSize = ppsLength;
			track->videoPps = malloc(ppsLength);
			MP4_RETURN_ERR_IF_FAILED((track->videoPps != NULL),
				-ENOMEM);
			size_t count = fread(track->videoPps, ppsLength,
				1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", ppsLength);
		} else {
			/* ignore any other PPS */
			int ret = fseeko(mp4->file, ppsLength, SEEK_CUR);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
				"failed to seek %u bytes forward in file",
				ppsLength);
		}
		boxReadBytes += ppsLength;
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}

static off_t mp4_box_esds_read(
	struct mp4_file *mp4,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0, minBytes = 9;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* version, always 0 */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	MP4_LOGD("# edsd: version=%" PRIu32, val32);

	/* ESDescriptor */
	uint8_t tag;
	MP4_READ_8(mp4->file, tag, boxReadBytes);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((tag == 3), -EPROTO,
		"invalid ESDescriptor tag: %" PRIu8 " expected %d",
		tag, 0x03);
	MP4_LOGD("# esds: ESDescriptor tag:0x%x", tag);

	off_t size = 0;
	int cnt = 0;
	do {
		MP4_READ_8(mp4->file, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(((val8 & 0x80) == 0), -EPROTO,
		"invalid ESDescriptor size, more than 4 bytes !");
	MP4_LOGD("# esds: ESDescriptor size:%zd (%d bytes)", size, cnt);

	minBytes = boxReadBytes + size;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* ES_ID */
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	val16 = ntohs(val16);
	MP4_LOGD("# esds: ESDescriptor ES_ID:%" PRIu16, val16);

	/* flags */
	uint8_t flags;
	MP4_READ_8(mp4->file, flags, boxReadBytes);
	MP4_LOGD("# esds: ESDecriptor flags:0x%02x", flags);

	if (flags & 0x80) {
		/* dependsOn_ES_ID */
		MP4_READ_16(mp4->file, val16, boxReadBytes);
		val16 = ntohs(val16);
		MP4_LOGD("# esds: ESDescriptor dependsOn_ES_ID:%" PRIu16,
			val16);
	}
	if (flags & 0x40) {
		/* URL_Flag : read url_len & url */
		MP4_READ_8(mp4->file, val8, boxReadBytes);
		MP4_LOGD("# esds: ESDescriptor url_len:%" PRIu8, val8);
		MP4_SKIP_BYTES(mp4->file, val8, boxReadBytes);
		MP4_LOGD("# esds: skipped %" PRIu8 " bytes", val8);
	}

	/* DecoderConfigDescriptor */
	MP4_READ_8(mp4->file, tag, boxReadBytes);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((tag == 4), -EPROTO,
		"invalid DecoderConfigDescriptor tag: %" PRIu8 " expected %d",
		tag, 0x04);
	MP4_LOGD("# esds: DecoderConfigDescriptor tag:0x%x", tag);
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(mp4->file, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(((val8 & 0x80) == 0), -EPROTO,
		"invalid DecoderConfigDescriptor size, more than 4 bytes !");
	MP4_LOGD("# esds: DCD size:%zd (%d bytes)", size, cnt);

	minBytes = boxReadBytes + size;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* Next 13 bytes unused */
	MP4_SKIP_BYTES(mp4->file, 13, boxReadBytes);
	MP4_LOGD("# esds: skipped 13 bytes");

	/* DecoderSpecificInfo */
	MP4_READ_8(mp4->file, tag, boxReadBytes);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((tag == 5), -EPROTO,
		"invalid DecoderSpecificInfo tag: %" PRIu8 " expected %d",
		tag, 0x05);
	MP4_LOGD("# esds: DecoderSpecificInfo tag:0x%x", tag);
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(mp4->file, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(((val8 & 0x80) == 0), -EPROTO,
		"invalid DecoderSpecificInfo size, more than 4 bytes !");
	MP4_LOGD("# esds: DSI size:%zd (%d bytes)", size, cnt);

	minBytes = boxReadBytes + size;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %" PRIi64 " expected %" PRIi64 " min",
		(int64_t)maxBytes, (int64_t)minBytes);

	/* Only read the first audioSpecificConfig */
	if ((!track->audioSpecificConfig) && (size > 0)) {
		track->audioSpecificConfigSize = size;
		track->audioSpecificConfig = malloc(size);
		MP4_RETURN_ERR_IF_FAILED((track->audioSpecificConfig != NULL),
			-ENOMEM);
		size_t count = fread(track->audioSpecificConfig, size,
			1, mp4->file);
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
			"failed to read %zd bytes from file", size);
		MP4_LOGD("# esds: read %zd bytes for audioSpecificConfig",
			size);
		boxReadBytes += size;
	}
	/* Skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}

static off_t mp4_box_stsd_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsd: version=%d", version);
	MP4_LOGD("# stsd: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t entryCount = ntohl(val32);
	MP4_LOGD("# stsd: entry_count=%" PRIu32, entryCount);

	unsigned int i;
	for (i = 0; i < entryCount; i++) {
		switch (track->type) {
		case MP4_TRACK_TYPE_VIDEO:
		{
			MP4_LOGD("# stsd: video handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 102),
				-EINVAL, "invalid size: %" PRIi64
				" expected %d min", (int64_t)maxBytes, 102);

			/* size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);

			/* reserved & data_reference_index */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: data_reference_index=%" PRIu16,
				dataReferenceIndex);

			int k;
			for (k = 0; k < 4; k++) {
				/* pre_defined & reserved */
				MP4_READ_32(mp4->file, val32, boxReadBytes);
			}

			/* width & height */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			track->videoWidth = ((ntohl(val32) >> 16) & 0xFFFF);
			track->videoHeight = (ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: width=%" PRIu32, track->videoWidth);
			MP4_LOGD("# stsd: height=%" PRIu32, track->videoHeight);

			/* horizresolution */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			float horizresolution = (float)(ntohl(val32)) / 65536.;
			MP4_LOGD("# stsd: horizresolution=%.2f",
				horizresolution);

			/* vertresolution */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			float vertresolution = (float)(ntohl(val32)) / 65536.;
			MP4_LOGD("# stsd: vertresolution=%.2f",
				vertresolution);

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);

			/* frame_count */
			MP4_READ_16(mp4->file, val16, boxReadBytes);
			uint16_t frameCount = ntohs(val16);
			MP4_LOGD("# stsd: frame_count=%" PRIu16, frameCount);

			/* compressorname */
			char compressorname[32];
			size_t count = fread(&compressorname,
				sizeof(compressorname), 1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %zu bytes from file",
				sizeof(compressorname));
			boxReadBytes += sizeof(compressorname);
			MP4_LOGD("# stsd: compressorname=%s", compressorname);

			/* depth & pre_defined */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint16_t depth =
				(uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
			MP4_LOGD("# stsd: depth=%" PRIu16, depth);

			/* codec specific size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t codecSize = ntohl(val32);
			MP4_LOGD("# stsd: codec_size=%" PRIu32, codecSize);

			/* codec specific */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t codec = ntohl(val32);
			MP4_LOGD("# stsd: codec=%c%c%c%c",
				(char)((codec >> 24) & 0xFF),
				(char)((codec >> 16) & 0xFF),
				(char)((codec >> 8) & 0xFF),
				(char)(codec & 0xFF));

			if (codec == MP4_AVC_DECODER_CONFIG_BOX) {
				track->videoCodec = MP4_VIDEO_CODEC_AVC;
				off_t ret = mp4_box_avcc_read(
					mp4, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					MP4_LOGE("mp4_box_avcc_read() failed"
						" (%" PRIi64 ")", (int64_t)ret);
					return ret;
				}
				boxReadBytes += ret;
			}
			break;
		}
		case MP4_TRACK_TYPE_AUDIO:
		{
			MP4_LOGD("# stsd: audio handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 44),
				-EINVAL, "invalid size: %" PRIi64
				" expected %d min", (int64_t)maxBytes, 44);

			/* size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);

			/* reserved & data_reference_index */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: data_reference_index=%" PRIu16,
				dataReferenceIndex);

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			MP4_READ_32(mp4->file, val32, boxReadBytes);

			/* channelcount & samplesize */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			track->audioChannelCount =
				((ntohl(val32) >> 16) & 0xFFFF);
			track->audioSampleSize = (ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: channelcount=%" PRIu32,
				track->audioChannelCount);
			MP4_LOGD("# stsd: samplesize=%" PRIu32,
				track->audioSampleSize);

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);

			/* samplerate */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			track->audioSampleRate = ntohl(val32);
			MP4_LOGD("# stsd: samplerate=%.2f",
				(float)track->audioSampleRate / 65536.);

			/* codec specific size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t codecSize = ntohl(val32);
			MP4_LOGD("# stsd: codec_size=%" PRIu32, codecSize);

			/* codec specific */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t codec = ntohl(val32);
			MP4_LOGD("# stsd: codec=%c%c%c%c",
				(char)((codec >> 24) & 0xFF),
				(char)((codec >> 16) & 0xFF),
				(char)((codec >> 8) & 0xFF),
				(char)(codec & 0xFF));

			if (codec == MP4_AUDIO_DECODER_CONFIG_BOX) {
				off_t ret = mp4_box_esds_read(
					mp4, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					MP4_LOGE("mp4_box_esds_read() failed"
						" (%" PRIi64 ")", (int64_t)ret);
					return ret;
				}
				boxReadBytes += ret;
			}

			break;
		}
		case MP4_TRACK_TYPE_HINT:
			MP4_LOGD("# stsd: hint handler type");
			break;
		case MP4_TRACK_TYPE_METADATA:
		{
			MP4_LOGD("# stsd: metadata handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24),
				-EINVAL, "invalid size: %" PRIi64
				" expected %d min", (int64_t)maxBytes, 24);

			/* size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			MP4_READ_16(mp4->file, val16, boxReadBytes);

			/* data_reference_index */
			MP4_READ_16(mp4->file, val16, boxReadBytes);
			uint16_t dataReferenceIndex = ntohl(val16);
			MP4_LOGD("# stsd: size=%d", dataReferenceIndex);

			char str[100];
			unsigned int k;
			for (k = 0; (k < sizeof(str) - 1) &&
				(boxReadBytes < maxBytes); k++) {
				MP4_READ_8(mp4->file, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			str[k + 1] = '\0';
			if (strlen(str) > 0)
				track->metadataContentEncoding = strdup(str);
			MP4_LOGD("# stsd: content_encoding=%s", str);

			for (k = 0; (k < sizeof(str) - 1) &&
				(boxReadBytes < maxBytes); k++) {
				MP4_READ_8(mp4->file, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			str[k + 1] = '\0';
			if (strlen(str) > 0)
				track->metadataMimeFormat = strdup(str);
			MP4_LOGD("# stsd: mime_format=%s", str);

			break;
		}
		case MP4_TRACK_TYPE_TEXT:
			MP4_LOGD("# stsd: text handler type");
			break;
		default:
			MP4_LOGD("# stsd: unknown handler type");
			break;
		}
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_stts_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->timeToSampleEntries == NULL), -EEXIST,
		"time to sample table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stts: version=%d", version);
	MP4_LOGD("# stts: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->timeToSampleEntryCount = ntohl(val32);
	MP4_LOGD("# stts: entry_count=%" PRIu32, track->timeToSampleEntryCount);

	track->timeToSampleEntries = malloc(track->timeToSampleEntryCount *
		sizeof(struct mp4_time_to_sample_entry));
	MP4_RETURN_ERR_IF_FAILED((track->timeToSampleEntries != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(8 + track->timeToSampleEntryCount * 8)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8 + track->timeToSampleEntryCount * 8);

	unsigned int i;
	for (i = 0; i < track->timeToSampleEntryCount; i++) {
		/* sample_count */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleCount = ntohl(val32);

		/* sample_delta */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleDelta = ntohl(val32);
		/* MP4_LOGD("# stts: sample_count=%" PRIu32
			" sample_delta=%" PRIu32,
			track->timeToSampleEntries[i].sampleCount,
			track->timeToSampleEntries[i].sampleDelta);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_stss_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->syncSampleEntries == NULL), -EEXIST,
		"sync sample table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stss: version=%d", version);
	MP4_LOGD("# stss: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->syncSampleEntryCount = ntohl(val32);
	MP4_LOGD("# stss: entry_count=%" PRIu32, track->syncSampleEntryCount);

	track->syncSampleEntries = malloc(
		track->syncSampleEntryCount * sizeof(uint32_t));
	MP4_RETURN_ERR_IF_FAILED((track->syncSampleEntries != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(8 + track->syncSampleEntryCount * 4)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8 + track->syncSampleEntryCount * 4);

	unsigned int i;
	for (i = 0; i < track->syncSampleEntryCount; i++) {
		/* sample_number */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->syncSampleEntries[i] = ntohl(val32);
		/*MP4_LOGD("# stss: sample_number=%" PRIu32,
			track->syncSampleEntries[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_stsz_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->sampleSize == NULL),
		-EEXIST, "sample size table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 12), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 12);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsz: version=%d", version);
	MP4_LOGD("# stsz: flags=%" PRIu32, flags);

	/* sample_size */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t sampleSize = ntohl(val32);
	MP4_LOGD("# stsz: sample_size=%" PRIu32, sampleSize);

	/* sample_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->sampleCount = ntohl(val32);
	MP4_LOGD("# stsz: sample_count=%" PRIu32, track->sampleCount);

	track->sampleSize = malloc(track->sampleCount * sizeof(uint32_t));
	MP4_RETURN_ERR_IF_FAILED((track->sampleSize != NULL), -ENOMEM);

	if (sampleSize == 0) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes >= (signed)(12 + track->sampleCount * 4)),
			-EINVAL, "invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes, 12 + track->sampleCount * 4);

		unsigned int i;
		for (i = 0; i < track->sampleCount; i++) {
			/* entry_size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			track->sampleSize[i] = ntohl(val32);
			/*MP4_LOGD("# stsz: entry_size=%" PRIu32,
				track->sampleSize[i]);*/
		}
	} else {
		unsigned int i;
		for (i = 0; i < track->sampleCount; i++)
			track->sampleSize[i] = sampleSize;
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_stsc_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->sampleToChunkEntries == NULL), -EEXIST,
		"sample to chunk table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsc: version=%d", version);
	MP4_LOGD("# stsc: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->sampleToChunkEntryCount = ntohl(val32);
	MP4_LOGD("# stsc: entry_count=%" PRIu32,
		track->sampleToChunkEntryCount);

	track->sampleToChunkEntries = malloc(
		track->sampleToChunkEntryCount *
		sizeof(struct mp4_sample_to_chunk_entry));
	MP4_RETURN_ERR_IF_FAILED((track->sampleToChunkEntries != NULL),
		-ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(8 + track->sampleToChunkEntryCount * 12)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8 + track->sampleToChunkEntryCount * 12);

	unsigned int i;
	for (i = 0; i < track->sampleToChunkEntryCount; i++) {
		/* first_chunk */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].firstChunk = ntohl(val32);
		/*MP4_LOGD("# stsc: first_chunk=%" PRIu32,
			track->sampleToChunkEntries[i].firstChunk);*/

		/* samples_per_chunk */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].samplesPerChunk = ntohl(val32);
		/*MP4_LOGD("# stsc: samples_per_chunk=%" PRIu32,
			track->sampleToChunkEntries[i].samplesPerChunk);*/

		/* sample_description_index */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].sampleDescriptionIndex =
			ntohl(val32);
		/*MP4_LOGD("# stsc: sample_description_index=%" PRIu32,
			track->sampleToChunkEntries[i].sampleDescriptionIndex);
			*/
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_stco_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL),
		-EEXIST, "chunk offset table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stco: version=%d", version);
	MP4_LOGD("# stco: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	MP4_LOGD("# stco: entry_count=%" PRIu32, track->chunkCount);

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(8 + track->chunkCount * 4)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8 + track->chunkCount * 4);

	unsigned int i;
	for (i = 0; i < track->chunkCount; i++) {
		/* chunk_offset */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32);
		/*MP4_LOGD("# stco: chunk_offset=%" PRIu64,
			track->chunkOffset[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_co64_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL),
		-EEXIST, "chunk offset table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# co64: version=%d", version);
	MP4_LOGD("# co64: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	MP4_LOGD("# co64: entry_count=%" PRIu32, track->chunkCount);

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(8 + track->chunkCount * 8)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8 + track->chunkCount * 8);

	unsigned int i;
	for (i = 0; i < track->chunkCount; i++) {
		/* chunk_offset */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		track->chunkOffset[i] |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		/*MP4_LOGD("# co64: chunk_offset=%" PRIu64,
			track->chunkOffset[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_xyz_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((box != NULL), -EINVAL,
		"invalid box");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 4);

	/* location_size */
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	uint16_t locationSize = ntohs(val16);
	MP4_LOGD("# xyz: location_size=%d", locationSize);

	/* language_code */
	MP4_READ_16(mp4->file, val16, boxReadBytes);
	uint16_t languageCode = ntohs(val16);
	MP4_LOGD("# xyz: language_code=%d", languageCode);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4 + locationSize),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 4 + locationSize);

	mp4->udtaLocationKey = malloc(5);
	MP4_RETURN_ERR_IF_FAILED((mp4->udtaLocationKey != NULL), -ENOMEM);
	mp4->udtaLocationKey[0] = ((box->type >> 24) & 0xFF);
	mp4->udtaLocationKey[1] = ((box->type >> 16) & 0xFF);
	mp4->udtaLocationKey[2] = ((box->type >> 8) & 0xFF);
	mp4->udtaLocationKey[3] = (box->type & 0xFF);
	mp4->udtaLocationKey[4] = '\0';

	mp4->udtaLocationValue = malloc(locationSize + 1);
	MP4_RETURN_ERR_IF_FAILED((mp4->udtaLocationValue != NULL), -ENOMEM);
	size_t count = fread(mp4->udtaLocationValue, locationSize,
		1, mp4->file);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
		"failed to read %u bytes from file", locationSize);
	boxReadBytes += locationSize;
	mp4->udtaLocationValue[locationSize] = '\0';
	MP4_LOGD("# xyz: location=%s", mp4->udtaLocationValue);

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static int mp4_ilst_sub_box_count(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t totalReadBytes = 0, boxReadBytes = 0;
	off_t originalOffset, realBoxSize;
	uint32_t val32;
	int lastBox = 0, count = 0;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	originalOffset = ftello(mp4->file);

	while ((totalReadBytes + 8 <= maxBytes) && (!lastBox)) {
		boxReadBytes = 0;

		/* box size */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t size = ntohl(val32);

		/* box type */
		MP4_READ_32(mp4->file, val32, boxReadBytes);

		if (size == 0) {
			/* box extends to end of file */
			/*TODO*/
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0, -ENOSYS,
				"size == 0 for list element"
				" is not implemented");
		} else if (size == 1) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				(maxBytes >= boxReadBytes + 16), -EINVAL,
				"invalid size: %" PRIi64 " expected %"
				PRIi64 " min", (int64_t)maxBytes,
				(int64_t)boxReadBytes + 16);

			/* large size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			realBoxSize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			realBoxSize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		} else
			realBoxSize = size;

		count++;

		/* skip the rest of the box */
		MP4_SKIP(mp4->file, boxReadBytes, realBoxSize);
		totalReadBytes += realBoxSize;
	}

	int ret = fseeko(mp4->file, -totalReadBytes, SEEK_CUR);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
		"failed to seek %" PRIi64 " bytes forward in file",
		(int64_t)-totalReadBytes);

	return count;
}


static off_t mp4_box_meta_keys_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32, i;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 8);

	/* version & flags */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# keys: version=%d", version);
	MP4_LOGD("# keys: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	mp4->metaMetadataCount = ntohl(val32);
	MP4_LOGD("# keys: entry_count=%" PRIu32, mp4->metaMetadataCount);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= (signed)(4 + mp4->metaMetadataCount * 8)),
		-EINVAL, "invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 4 + mp4->metaMetadataCount * 8);

	mp4->metaMetadataKey = calloc(
		mp4->metaMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((mp4->metaMetadataKey != NULL), -ENOMEM);

	mp4->metaMetadataValue = calloc(
		mp4->metaMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((mp4->metaMetadataValue != NULL), -ENOMEM);

	for (i = 0; i < mp4->metaMetadataCount; i++) {
		/* key_size */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t keySize = ntohl(val32);
		MP4_LOGD("# keys: key_size=%" PRIu32, keySize);

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((keySize >= 8), -EINVAL,
			"invalid key size: %" PRIu32 " expected %d min",
			keySize, 8);
		keySize -= 8;

		/* key_namespace */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		uint32_t keyNamespace = ntohl(val32);
		MP4_LOGD("# keys: key_namespace=%c%c%c%c",
			(char)((keyNamespace >> 24) & 0xFF),
			(char)((keyNamespace >> 16) & 0xFF),
			(char)((keyNamespace >> 8) & 0xFF),
			(char)(keyNamespace & 0xFF));

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes - boxReadBytes >= (signed)keySize),
			-EINVAL, "invalid size: %" PRIi64 " expected %d min",
			(int64_t)maxBytes - boxReadBytes, keySize);

		mp4->metaMetadataKey[i] = malloc(keySize + 1);
		MP4_RETURN_ERR_IF_FAILED((mp4->metaMetadataKey[i] != NULL),
			-ENOMEM);
		size_t count = fread(mp4->metaMetadataKey[i], keySize,
			1, mp4->file);
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
			"failed to read %u bytes from file", keySize);
		boxReadBytes += keySize;
		mp4->metaMetadataKey[i][keySize] = '\0';
		MP4_LOGD("# keys: key_value[%i]=%s",
			i, mp4->metaMetadataKey[i]);
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_box_meta_data_read(
	struct mp4_file *mp4,
	struct mp4_box *box,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((box->parent != NULL), -EINVAL,
		"invalid parent");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9), -EINVAL,
		"invalid size: %" PRIi64 " expected %d min",
		(int64_t)maxBytes, 9);

	/* version & class */
	MP4_READ_32(mp4->file, val32, boxReadBytes);
	uint32_t clazz = ntohl(val32);
	uint8_t version = (clazz >> 24) & 0xFF;
	clazz &= 0xFF;
	MP4_LOGD("# data: version=%d", version);
	MP4_LOGD("# data: class=%" PRIu32, clazz);

	/* reserved */
	MP4_READ_32(mp4->file, val32, boxReadBytes);

	unsigned int valueLen = maxBytes - boxReadBytes;

	if (clazz == MP4_METADATA_CLASS_UTF8) {
		switch (box->parent->type & 0xFFFFFF) {
		case MP4_METADATA_TAG_TYPE_ARTIST:
		case MP4_METADATA_TAG_TYPE_TITLE:
		case MP4_METADATA_TAG_TYPE_DATE:
		case MP4_METADATA_TAG_TYPE_COMMENT:
		case MP4_METADATA_TAG_TYPE_COPYRIGHT:
		case MP4_METADATA_TAG_TYPE_MAKER:
		case MP4_METADATA_TAG_TYPE_MODEL:
		case MP4_METADATA_TAG_TYPE_VERSION:
		case MP4_METADATA_TAG_TYPE_ENCODER:
		{
			uint32_t idx = mp4->udtaMetadataParseIdx++;
			mp4->udtaMetadataKey[idx] = malloc(5);
			MP4_RETURN_ERR_IF_FAILED(
				(mp4->udtaMetadataKey[idx] != NULL), -ENOMEM);
			mp4->udtaMetadataKey[idx][0] =
				((box->parent->type >> 24) & 0xFF);
			mp4->udtaMetadataKey[idx][1] =
				((box->parent->type >> 16) & 0xFF);
			mp4->udtaMetadataKey[idx][2] =
				((box->parent->type >> 8) & 0xFF);
			mp4->udtaMetadataKey[idx][3] =
				(box->parent->type & 0xFF);
			mp4->udtaMetadataKey[idx][4] = '\0';
			mp4->udtaMetadataValue[idx] = malloc(valueLen + 1);
			MP4_RETURN_ERR_IF_FAILED(
				(mp4->udtaMetadataValue[idx] != NULL),
				-ENOMEM);
			size_t count = fread(mp4->udtaMetadataValue[idx],
				valueLen, 1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", valueLen);
			boxReadBytes += valueLen;
			mp4->udtaMetadataValue[idx][valueLen] = '\0';
			MP4_LOGD("# data: value[%s]=%s",
				mp4->udtaMetadataKey[idx],
				mp4->udtaMetadataValue[idx]);
			break;
		}
		default:
		{
			if ((box->parent->type > 0) &&
				(box->parent->type <=
				mp4->metaMetadataCount)) {
				uint32_t idx = box->parent->type - 1;
				mp4->metaMetadataValue[idx] =
					malloc(valueLen + 1);
				MP4_RETURN_ERR_IF_FAILED(
					(mp4->metaMetadataValue[idx] != NULL),
					-ENOMEM);
				size_t count = fread(
					mp4->metaMetadataValue[idx], valueLen,
					1, mp4->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %u bytes from file",
					valueLen);
				boxReadBytes += valueLen;
				mp4->metaMetadataValue[idx][valueLen] = '\0';
				MP4_LOGD("# data: value[%s]=%s",
					mp4->metaMetadataKey[idx],
					mp4->metaMetadataValue[idx]);
			}
			break;
		}
		}
	} else if ((clazz == MP4_METADATA_CLASS_JPEG) ||
		(clazz == MP4_METADATA_CLASS_PNG) ||
		(clazz == MP4_METADATA_CLASS_BMP)) {
		uint32_t type = box->parent->type;
		if (type == MP4_METADATA_TAG_TYPE_COVER) {
			mp4->udtaCoverOffset = ftello(mp4->file);
			mp4->udtaCoverSize = valueLen;
			switch (clazz) {
			default:
			case MP4_METADATA_CLASS_JPEG:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			}
			MP4_LOGD("# data: udta cover size=%d type=%d",
				mp4->udtaCoverSize, mp4->udtaCoverType);
		} else if ((type > 0) && (type <= mp4->metaMetadataCount) &&
			(!strcmp(mp4->metaMetadataKey[type - 1],
				MP4_METADATA_KEY_COVER))) {
			mp4->metaCoverOffset = ftello(mp4->file);
			mp4->metaCoverSize = valueLen;
			switch (clazz) {
			default:
			case MP4_METADATA_CLASS_JPEG:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			}
			MP4_LOGD("# data: meta cover size=%d type=%d",
				mp4->metaCoverSize, mp4->metaCoverType);
		}
	}

	/* skip the rest of the box */
	MP4_SKIP(mp4->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


off_t mp4_box_children_read(
	struct mp4_file *mp4,
	struct mp4_box *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t parentReadBytes = 0;
	int ret = 0, lastBox = 0;

	while ((!feof(mp4->file)) && (!lastBox) &&
		(parentReadBytes + 8 < maxBytes)) {
		off_t boxReadBytes = 0, realBoxSize;
		uint32_t val32;

		/* keep the box in the tree */
		struct mp4_box *box = mp4_box_new(parent);
		MP4_RETURN_ERR_IF_FAILED((box != NULL), -ENOMEM);

		/* box size */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		box->size = ntohl(val32);

		/* box type */
		MP4_READ_32(mp4->file, val32, boxReadBytes);
		box->type = ntohl(val32);
		if ((parent) && (parent->type == MP4_ILST_BOX) &&
			(box->type <= mp4->metaMetadataCount))
			MP4_LOGD("offset 0x%" PRIx64
				" metadata box size %" PRIu32,
				(int64_t)ftello(mp4->file), box->size);
		else
			MP4_LOGD("offset 0x%" PRIx64
				" box '%c%c%c%c' size %" PRIu32,
				(int64_t)ftello(mp4->file),
				(box->type >> 24) & 0xFF,
				(box->type >> 16) & 0xFF,
				(box->type >> 8) & 0xFF,
				box->type & 0xFF, box->size);

		if (box->size == 0) {
			/* box extends to end of file */
			lastBox = 1;
			realBoxSize = mp4->fileSize - mp4->readBytes;
		} else if (box->size == 1) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				(maxBytes >= parentReadBytes + 16), -EINVAL,
				"invalid size: %" PRIi64 " expected %"
				PRIi64 " min", (int64_t)maxBytes,
				(int64_t)parentReadBytes + 16);

			/* large size */
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			box->largesize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(mp4->file, val32, boxReadBytes);
			box->largesize |=
				(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
			realBoxSize = box->largesize;
		} else
			realBoxSize = box->size;

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes >= parentReadBytes + realBoxSize), -EINVAL,
			"invalid size: %" PRIi64 " expected %" PRIi64 " min",
			(int64_t)maxBytes,
			(int64_t)parentReadBytes + realBoxSize);

		switch (box->type) {
		case MP4_UUID:
		{
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				((unsigned)(realBoxSize - boxReadBytes) >=
				sizeof(box->uuid)), -EINVAL,
				"invalid size: %" PRIi64 " expected %zu min",
				(int64_t)realBoxSize - boxReadBytes,
				sizeof(box->uuid));

			/* box extended type */
			size_t count = fread(box->uuid, sizeof(box->uuid),
				1, mp4->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %zu bytes from file",
				sizeof(box->uuid));
			boxReadBytes += sizeof(box->uuid);
			break;
		}
		case MP4_MOVIE_BOX:
		case MP4_USER_DATA_BOX:
		case MP4_MEDIA_BOX:
		case MP4_MEDIA_INFORMATION_BOX:
		case MP4_DATA_INFORMATION_BOX:
		case MP4_SAMPLE_TABLE_BOX:
		{
			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_FILE_TYPE_BOX:
		{
			off_t _ret = mp4_box_ftyp_read(
				mp4, box, realBoxSize - boxReadBytes);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MOVIE_HEADER_BOX:
		{
			off_t _ret = mp4_box_mvhd_read(
				mp4, box, realBoxSize - boxReadBytes);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_BOX:
		{
			/* keep the track in the list */
			struct mp4_track *tk = mp4_track_add(mp4);
			MP4_RETURN_ERR_IF_FAILED((tk != NULL), -ENOMEM);

			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, tk);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_HEADER_BOX:
		{
			off_t _ret = mp4_box_tkhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_REFERENCE_BOX:
		{
			off_t _ret = mp4_box_tref_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HANDLER_REFERENCE_BOX:
		{
			off_t _ret = mp4_box_hdlr_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_box_mdhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_VIDEO_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_box_vmhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SOUND_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_box_smhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HINT_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_box_hmhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_NULL_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_box_nmhd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_DESCRIPTION_BOX:
		{
			off_t _ret = mp4_box_stsd_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DECODING_TIME_TO_SAMPLE_BOX:
		{
			off_t _ret = mp4_box_stts_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SYNC_SAMPLE_BOX:
		{
			off_t _ret = mp4_box_stss_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_SIZE_BOX:
		{
			off_t _ret = mp4_box_stsz_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_TO_CHUNK_BOX:
		{
			off_t _ret = mp4_box_stsc_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_BOX:
		{
			off_t _ret = mp4_box_stco_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_64_BOX:
		{
			off_t _ret = mp4_box_co64_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_META_BOX:
		{
			if ((parent) &&
				(parent->type == MP4_USER_DATA_BOX)) {
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(realBoxSize - boxReadBytes >= 4),
					-EINVAL, "invalid size: %" PRIi64
					" expected %d min",
					(int64_t)realBoxSize - boxReadBytes, 4);

				/* version & flags */
				MP4_READ_32(mp4->file, val32, boxReadBytes);
				uint32_t flags = ntohl(val32);
				uint8_t version = (flags >> 24) & 0xFF;
				flags &= ((1 << 24) - 1);
				MP4_LOGD("# meta: version=%d", version);
				MP4_LOGD("# meta: flags=%" PRIu32, flags);

				off_t _ret = mp4_box_children_read(mp4, box,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			} else if ((parent) &&
				(parent->type == MP4_MOVIE_BOX)) {
				off_t _ret = mp4_box_children_read(mp4, box,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_ILST_BOX:
		{
			if ((parent) && (parent->parent) &&
				(parent->parent->type ==
				MP4_USER_DATA_BOX)) {
				mp4->udtaMetadataCount =
					mp4_ilst_sub_box_count(mp4, box,
					realBoxSize - boxReadBytes, track);
				if (mp4->udtaMetadataCount > 0) {
					char **key =
						calloc(mp4->udtaMetadataCount,
						sizeof(char *));
					MP4_RETURN_ERR_IF_FAILED(
						(key != NULL), -ENOMEM);
					mp4->udtaMetadataKey = key;

					char **value =
						calloc(mp4->udtaMetadataCount,
						sizeof(char *));
					MP4_RETURN_ERR_IF_FAILED(
						(value != NULL), -ENOMEM);
					mp4->udtaMetadataValue = value;

					mp4->udtaMetadataParseIdx = 0;
				}
			}
			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DATA_BOX:
		{
			off_t _ret = mp4_box_meta_data_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_LOCATION_BOX:
		{
			if ((parent) &&
				(parent->type == MP4_USER_DATA_BOX)) {
				off_t _ret = mp4_box_xyz_read(mp4, box,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_KEYS_BOX:
		{
			if ((parent) && (parent->type == MP4_META_BOX)) {
				off_t _ret = mp4_box_meta_keys_read(mp4, box,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		default:
		{
			if ((parent) && (parent->type == MP4_ILST_BOX)) {
				off_t _ret = mp4_box_children_read(mp4, box,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		}

		/* skip the rest of the box */
		if (realBoxSize < boxReadBytes) {
			MP4_LOGE("invalid box size %" PRIi64
				" (read bytes: %" PRIi64 ")",
				(int64_t)realBoxSize, (int64_t)boxReadBytes);
			ret = -EIO;
			break;
		}
		int _ret = fseeko(mp4->file,
			realBoxSize - boxReadBytes, SEEK_CUR);
		if (_ret != 0) {
			MP4_LOGE("failed to seek %" PRIi64
				" bytes forward in file",
				(int64_t)realBoxSize - boxReadBytes);
			ret = -EIO;
			break;
		}

		parentReadBytes += realBoxSize;
	}

	return (ret < 0) ? ret : parentReadBytes;
}


int mp4_generate_avc_decoder_config(
	uint8_t *sps,
	unsigned int sps_size,
	uint8_t *pps,
	unsigned int pps_size,
	uint8_t *avcc,
	unsigned int *avcc_size)
{
	off_t off = 0;

	MP4_RETURN_ERR_IF_FAILED(sps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(avcc != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps_size >= 4, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(*avcc_size >= (sps_size + pps_size + 11),
				 -ENOMEM);

	/* configurationVersion = 1, AVCProfileIndication,
	 * profile_compatibility, AVCLevelIndication */
	avcc[off++] = 0x01;
	avcc[off++] = sps[1];
	avcc[off++] = sps[2];
	avcc[off++] = sps[3];
	/* reserved (6 bits), lengthSizeMinusOne = 3 (2 bits),
	 * reserved (3bits), numOfSequenceParameterSets (5 bits) */
	avcc[off++] = 0xFF;
	avcc[off++] = 0xE1;
	/* sequenceParameterSetLength */
	avcc[off++] = sps_size >> 8;
	avcc[off++] = sps_size & 0xFF;
	/* sequenceParameterSetNALUnit */
	memcpy(&avcc[off], sps, sps_size);
	off += sps_size;
	/* numOfPictureParameterSets */
	avcc[off++] = 0x01;
	/* pictureParameterSetLength */
	avcc[off++] = pps_size >> 8;
	avcc[off++] = pps_size & 0xFF;
	/* pictureParameterSetNALUnit */
	memcpy(&avcc[off], pps, pps_size);
	off += pps_size;

	*avcc_size = off;
	return 0;
}
