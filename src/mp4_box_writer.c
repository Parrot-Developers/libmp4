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


static off_t
mp4_box_empty_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	uint32_t val32;
	off_t size = 8;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	char name[5];
	uint32_t *x = (uint32_t *)name;
	*x = htonl(box->type);
	name[4] = '\0';
	ULOGE("box %s write function not implemented", name);

	/* Box size */
	val32 = htonl(size);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, size, bytesWritten);

	return bytesWritten;
}


static off_t mp4_box_container_write(struct mp4_mux *mux,
				     struct mp4_box *box,
				     size_t maxBytes)
{
	off_t bytesWritten = 0;
	uint32_t val32;
	struct mp4_box *child;
	off_t ret;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Write all childrens */

	struct list_node *start = &box->children;
	custom_walk(start, child, node, struct mp4_box)
	{
		if (child->writer.func == NULL)
			continue;
		ret = child->writer.func(mux, child, maxBytes - bytesWritten);
		if (ret < 0)
			return ret;
		bytesWritten += ret;
	}

	MP4_WRITE_CHECK_SIZE(mux->file, 0, bytesWritten);

	return bytesWritten;
}


/**
 *  ISO/IEC 14496-12 8.2.2
 */
static off_t
mp4_box_mvhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux *args;
	off_t bytesWritten = 0;
	off_t boxSize = 120;
	uint32_t val32;
	uint16_t val16;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	args = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Version & Flags */
	val32 = htonl(0x01000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'creation_time' */
	val32 = htonl(args->creation_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(args->creation_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'modification_time' */
	val32 = htonl(args->modification_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(args->modification_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'timescale' */
	val32 = htonl(args->timescale);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'duration' */
	val32 = htonl(args->duration >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(args->duration & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'preferred_rate' */
	val32 = htonl(0x00010000); /* Q16.16 */
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'preferred_volume' */
	val16 = htons(0x0100); /* Q8.8 */
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 10 bytes Reserved */
	skip = 10;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Matrix */
	val32 = htonl(0x00010000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	skip = 12;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);
	val32 = htonl(0x40000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Pre defined */
	skip = 24;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Next track id */
	val32 = htonl(args->track_count + 1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.3.2
 */
static off_t
mp4_box_tkhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 104;
	uint32_t val32;
	uint16_t val16;
	size_t skip;
	uint32_t width, height;
	uint16_t volume;
	uint32_t version_flags;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	version_flags = track->flags & 0x7;
	version_flags |= 0x01000000;
	val32 = htonl(version_flags);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'creation_time' */
	val32 = htonl(track->creation_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->creation_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'modification_time' */
	val32 = htonl(track->modification_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->modification_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'track_ID' */
	val32 = htonl(track->id);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 4;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'duration' */
	val32 = htonl(track->duration_moov >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->duration_moov & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 8;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'layer' & 'alternate_group' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'volume' */
	volume = (track->type == MP4_TRACK_TYPE_AUDIO) ? 0x0100 : 0;
	val16 = htons(volume);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Reserved */
	skip = 2;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Matrix */
	val32 = htonl(0x00010000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	skip = 12;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);
	val32 = htonl(0x40000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'width' & 'height' */
	if (track->type == MP4_TRACK_TYPE_VIDEO) {
		width = track->video.width << 16;
		height = track->video.height << 16;
	} else {
		width = height = 0;
	}
	val32 = htonl(width);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(height);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.3.3
 */
static off_t mp4_box_tref_content_write(struct mp4_mux *mux,
					struct mp4_box *box,
					size_t maxBytes)
{
	struct mp4_mux_track *track;
	size_t i;
	off_t bytesWritten = 0;
	off_t boxSize = 0;
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	boxSize = 8 + 4 * track->referenceTrackIdCount;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'track_reference_id' */
	for (i = 0; i < track->referenceTrackIdCount; i++) {
		val32 = htonl(track->referenceTrackId[i]);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.4.2
 */
static off_t
mp4_box_mdhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 44;
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0x01000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'creation_time' */
	val32 = htonl(track->creation_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->creation_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'modification_time' */
	val32 = htonl(track->modification_time >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->modification_time & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'timescale' */
	val32 = htonl(track->timescale);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'duration' */
	val32 = htonl(track->duration >> 32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(track->duration & 0xffffffff);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'language' & 'quality' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.4.5.2
 */
static off_t
mp4_box_vmhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 20;
	uint32_t val32;
	size_t skip;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'graphicsmode' & 'opcolor' */
	skip = 8;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.4.5.3
 */
static off_t
mp4_box_smhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 16;
	uint32_t val32;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'balance' (0 = center) */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.4.5.5
 */
static off_t
mp4_box_nmhd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 12;
	uint32_t val32;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.4.3
 */
static off_t
mp4_box_hdlr_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 32; /* Box size excluding name length */
	uint32_t val32;
	uint8_t val8;
	size_t skip;
	uint32_t handler_type;
	const char *name;
	size_t namelen;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	switch (track->type) {
	case MP4_TRACK_TYPE_VIDEO:
		handler_type = MP4_HANDLER_TYPE_VIDEO;
		name = "VideoHandler";
		break;
	case MP4_TRACK_TYPE_AUDIO:
		handler_type = MP4_HANDLER_TYPE_AUDIO;
		name = "SoundHandler";
		break;
	case MP4_TRACK_TYPE_METADATA:
		handler_type = MP4_HANDLER_TYPE_METADATA;
		name = "TimedMetadata";
		break;
	default:
		return -EINVAL;
	}

	if (track->name)
		name = track->name;

	namelen = strlen(name) + 1;
	boxSize += namelen;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Pre defined */
	skip = 4;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'handler_type' */
	val32 = htonl(handler_type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 12;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'name' (including terminating NULL) */
	for (size_t i = 0; i < namelen; i++) {
		val8 = name[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.7.2
 */
static off_t
mp4_box_dref_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 28;
	uint32_t val32;

	if (mux == NULL || box == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flag' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* entry_count' */
	val32 = htonl(1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Dref 'size' */
	val32 = htonl(12);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Dref 'type' */
	val32 = htonl(MP4_DATA_REFERENCE_TYPE_URL);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Dref 'version' & 'flags' */
	val32 = htonl(1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}

static off_t 
mp4_box_mp4v_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	/* This part is already written
	00 00 00 a6 6d 70 34 76 00 00 00 00 00 00 00 01
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	03 c0 01 e0 00 48 00 00 00 48 00 00 00 00 00 00
	00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	00 00 00 18 ff ff 
	*/

	/* We have to write the following bytes:
	00 00 00 2c 65 73 64 73 00 00
	00 00 03 80 80 80 1b 00 01 00 04 80 80 80 0d 6c
	11 00 00 00 00 36 e8 54 00 36 e8 54 06 80 80 80
	01 02 00 00 00 10 70 61 73 70 00 00 00 01 00 00
	00 01 00 00 00 14 62 74 72 74 00 00 00 00 00 36
	e8 54 00 36 e8 54
	*/

	uint32_t val32;
	uint16_t val16;
	off_t bytesWritten = 0;
	/*
	val32 = htonl(0x000000a6);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x6d703476);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000001);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x03c001e0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00480000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00480000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00010000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000018);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0xffff0000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	*/
	val16 = htons(0x0000);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);
	val32 = htonl(0x002c6573);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x64730000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000380);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x80801b00);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x01000480);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x80800d6c);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x11000000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x0036e854);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x0036e854);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x06808080);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x01020000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00107061);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x73700000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00010000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00010000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00146274);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x72740000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0x00000036);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(0xe8540036);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val16 = htons(0xe854);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);
	return bytesWritten;
}

/**
 * ISO/IEC 14496-15 5.3.4 + 5.2.4.1
 * Called from mp4_video_decoder_config_write, box->type cannot be used
 */
static off_t
mp4_box_avcc_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 19; /* Does not include sps/pps size */
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	if (track->video.avc.sps_size < 4)
		return -EINVAL;

	boxSize += track->video.avc.sps_size;
	boxSize += track->video.avc.pps_size;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_AVC_DECODER_CONFIG_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' */
	val8 = 1;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'AVCProfileIndication;' */
	val8 = track->video.avc.sps[1];
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'profile_compatibility;' */
	val8 = track->video.avc.sps[2];
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'AVCLevelIndication;' */
	val8 = track->video.avc.sps[3];
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* Reserved | 'LengthSizeMinusOne' */
	val8 = 0xfc | 0x03;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* Reserved | 'numOfSequenceParameterSets' */
	val8 = 0xe0 | 1;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'sequenceParameterSetLength' */
	val16 = htons(track->video.avc.sps_size);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'sequenceParameterSetNALUnit' */
	for (size_t i = 0; i < track->video.avc.sps_size; i++) {
		val8 = track->video.avc.sps[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}

	/* 'numOfPictureParameterSets' */
	val8 = 1;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'pictureParameterSetLength' */
	val16 = htons(track->video.avc.pps_size);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'pictureParameterSetNALUnit' */
	for (size_t i = 0; i < track->video.avc.pps_size; i++) {
		val8 = track->video.avc.pps[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-15 - chap. 8.3.3.1.2 - HVCC decoder configuration record
 * Called from mp4_video_decoder_config_write, box->type cannot be used
 */
static off_t
mp4_box_hvcc_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	struct mp4_hvcc_info *hvcc;
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* not known yet */
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;
	/* We have 3 'arrays': vps, sps, pps */
	uint8_t nb_arrays = 3;
	uint8_t array_completeness = 1;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	hvcc = &track->video.hevc.hvcc_info;

	if (track->video.hevc.sps_size < 4)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_HEVC_DECODER_CONFIG_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' */
	val8 = 1;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'general_profile_space', 'general_tier_flag', 'general_profile_idc'
	 */
	val8 = (hvcc->general_profile_space << 6) |
	       (hvcc->general_tier_flag << 5) | hvcc->general_profile_idc;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'general_profile_compatibility_flags' */
	val32 = htonl(hvcc->general_profile_compatibility_flags);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'general_constraints_indicator_flags' */
	val32 = (uint32_t)((hvcc->general_constraints_indicator_flags >> 16) &
			   0x00000000FFFFFFFF);
	val32 = htonl(val32);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val16 = (uint32_t)(hvcc->general_constraints_indicator_flags &
			   0x0000000000FFFF);
	val16 = htons(val16);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'general_level_idc' */
	MP4_WRITE_8(mux->file, hvcc->general_level_idc, bytesWritten, maxBytes);

	/* Reserved | 'min_spatial_segmentation_idc' */
	val16 = htons(hvcc->min_spatial_segmentation_idc | 0xF000);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Reserved | 'parallelismType' */
	val8 = hvcc->parallelism_type | 0xFC;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* Reserved | 'chromaFormat' */
	val8 = hvcc->chroma_format | 0xFC;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* Reserved | 'bitDepthLumaMinus8' */
	val8 = (hvcc->bit_depth_luma - 8) | 0xF8;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* Reserved | 'bitDepthChromaMinus8' */
	val8 = (hvcc->bit_depth_chroma - 8) | 0xF8;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'avgFrameRate' */
	val16 = htons(hvcc->avg_framerate);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'constantFrameRate', 'numTemporalLayers', 'temporalIdNested'
	   'lengthSize'	*/
	val8 = hvcc->constant_framerate << 6 | hvcc->num_temporal_layers << 5 |
	       hvcc->temporal_id_nested << 2 | (hvcc->length_size - 1);
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'numOfArrays' */
	MP4_WRITE_8(mux->file, nb_arrays, bytesWritten, maxBytes);

	/* Write VPS */
	/* 'array_completeness' and 'NAL_unit_type' */
	val8 = array_completeness << 7 | MP4_H265_NALU_TYPE_VPS;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'numNalus' */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'nalUnitLength' */
	val16 = htons(track->video.hevc.vps_size);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* write nalu data */
	for (size_t k = 0; k < track->video.hevc.vps_size; k++) {
		MP4_WRITE_8(mux->file,
			    track->video.hevc.vps[k],
			    bytesWritten,
			    maxBytes);
	}

	/* Write SPS */
	/* 'array_completeness' and 'NAL_unit_type' */
	val8 = array_completeness << 7 | MP4_H265_NALU_TYPE_SPS;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'numNalus' */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'nalUnitLength' */
	val16 = htons(track->video.hevc.sps_size);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* write nalu data */
	for (size_t k = 0; k < track->video.hevc.sps_size; k++) {
		MP4_WRITE_8(mux->file,
			    track->video.hevc.sps[k],
			    bytesWritten,
			    maxBytes);
	}

	/* Write PPS */
	/* 'array_completeness' and 'NAL_unit_type' */
	val8 = array_completeness << 7 | MP4_H265_NALU_TYPE_PPS;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'numNalus' */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'nalUnitLength' */
	val16 = htons(track->video.hevc.pps_size);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* write nalu data */
	for (size_t k = 0; k < track->video.hevc.pps_size; k++) {
		MP4_WRITE_8(mux->file,
			    track->video.hevc.pps[k],
			    bytesWritten,
			    maxBytes);
	}

	/* update the box size with real size */
	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


static uint8_t mp4_box_esds_descriptor_size_length(uint32_t desc_size)
{
	uint8_t bytes = 0;

	while (desc_size > 0) {
		bytes++;
		desc_size >>= 7;
	}

	return bytes;
}


/**
 * ISO/IEC 14496-14 5.6.1 + 14496-1 7.2.6.5
 * Called from mp4_box_mp4a_write, box->type cannot be used
 */
static off_t
mp4_box_esds_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* ES descriptor length & contents not included */
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;
	size_t skip;


	uint32_t esd_size, esd_size_len;
	uint32_t dcd_size, dcd_size_len;
	uint32_t dsi_size, dsi_size_len;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	dsi_size = track->audio.specific_config_size;
	if (dsi_size == 0)
		return -EINVAL;
	dsi_size_len = mp4_box_esds_descriptor_size_length(dsi_size);
	dcd_size = dsi_size + dsi_size_len + 14;
	dcd_size_len = mp4_box_esds_descriptor_size_length(dcd_size);
	esd_size = dcd_size + dcd_size_len + 4;
	esd_size_len = mp4_box_esds_descriptor_size_length(esd_size);

	/* esd_size is the biggest. If esd_size_len fits into 4 bytes,
	 * all other will fit */
	if (esd_size_len > 4)
		return -EINVAL;

	boxSize += esd_size + esd_size_len;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_AUDIO_DECODER_CONFIG_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' */
	val32 = 0;
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* ES Decriptor 'tag' */
	val8 = 0x03;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* ES Descriptor 'size' */
	switch (esd_size_len) {
	case 4:
		val8 = (esd_size >> 21 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 3:
		val8 = (esd_size >> 14 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 2:
		val8 = (esd_size >> 7 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 1:
		val8 = esd_size & 0x7f;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		break;
	default:
		return -EINVAL;
	}

	/* ES 'ID' */
	val16 = htons(track->id);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Flags:
	 * (7)    streamDependenceFlag
	 * (6)    URL_Flag
	 * (5)    OCRStreamFlag
	 * (4..0) streamPriority */
	val8 = 0;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* As flags are forced to 0, we don't need to handle 'dependsOn_ES_ID',
	 * 'URLlength', 'URLstring' or 'OCR_ES_Id' fields */

	/* Decoder Config Descriptor (DCD) 'tag' */
	val8 = 0x04;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* DCD 'size' */
	switch (dcd_size_len) {
	case 4:
		val8 = (dcd_size >> 21 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 3:
		val8 = (dcd_size >> 14 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 2:
		val8 = (dcd_size >> 7 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 1:
		val8 = dcd_size & 0x7f;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		break;
	default:
		return -EINVAL;
	}

	/* DCD 'ObjecttypeIndication' */
	val8 = 0x40; /* Audio ISO/IEC 14496-3 */
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'StreamType' (6bits) + 'upStream' (1bit) + Reserved (1bit) */
	val8 = 0x15; /* 0x05 [Audio] << 2 | 0 << 1 | 0 << 0 */
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'bufferSizeDB', 'maxBitrate' & 'avgBitrate' can be set as zero */
	skip = 11;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Decoder Specific Info (DSI) 'tag' */
	val8 = 0x05;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* DSI size */
	switch (dsi_size_len) {
	case 4:
		val8 = (dsi_size >> 21 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 3:
		val8 = (dsi_size >> 14 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 2:
		val8 = (dsi_size >> 7 & 0x7f) | 0x80;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		/* FALLTHROUGH */
	case 1:
		val8 = dsi_size & 0x7f;
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		break;
	default:
		return -EINVAL;
	}

	/* DSI data */
	for (uint32_t i = 0; i < track->audio.specific_config_size; i++) {
		val8 = track->audio.specific_config[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}

	/* SL Packet header 'tag' */
	val8 = 0x06;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* SL Packet header 'size' */
	val8 = 1;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* SL Packet 'header': predefined */
	val8 = 2; /* Reserved for use in MP4 files */
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO 14496-12 8.5.2.2
 * ISO/IEC 14496-15 5.3.4
 * ISO/IEC 14496-15 8.4.1.1.2
 * Called from mp4_box_stsd_write, box->type cannot be used
 * write both avc1 and hvc1 depending on input codec
 */
static off_t mp4_video_decoder_config_write(struct mp4_mux *mux,
					    struct mp4_box *box,
					    size_t maxBytes,
					    enum mp4_video_codec codec)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Box size can't be determined here */
	off_t res = 0;
	uint32_t val32;
	uint16_t val16;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	switch (codec) {
	case MP4_VIDEO_CODEC_AVC:
		val32 = htonl(MP4_AVC1);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
		break;
	case MP4_VIDEO_CODEC_HEVC:
		val32 = htonl(MP4_HVC1);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
		break;
	case MP4_VIDEO_CODEC_MP4V:
		val32 = htonl(MP4_MP4V);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
		break;
	default:
		ULOGE("unexpected video codec");
		return -ENOSYS;
	}

	/* Reserved */
	skip = 6;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Data reference index */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Pre defined & reserved */
	skip = 16;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'width' & 'height' */
	val16 = htons(track->video.width);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);
	val16 = htons(track->video.height);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'horizresolution' & 'vertresolution' */
	val32 = htonl(0x00480000);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 4;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'frame_count' */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'Compressorname' */
	skip = 32;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'depth' */
	val16 = htons(0x0018);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Pre defined */
	val16 = htons(0xffff);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Box type */
	if (codec == MP4_VIDEO_CODEC_AVC) {
		/* avcC */
		res = mp4_box_avcc_write(mux, box, maxBytes - bytesWritten);
		if (res < 0)
			return res;
	} else if (codec == MP4_VIDEO_CODEC_HEVC) {
		/* hvcC */
		res = mp4_box_hvcc_write(mux, box, maxBytes - bytesWritten);
		if (res < 0)
			return res;
	}
	else if (codec == MP4_VIDEO_CODEC_MP4V) {
		/* mp4v */
		res = mp4_box_mp4v_write(mux, box, maxBytes - bytesWritten); // TODO: hardcoded for now
		if (res < 0)
			return res;
	}

	bytesWritten += res;

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-14 5.6
 * Called from mp4_box_stsd_write, box->type cannot be used
 */
static off_t
mp4_box_mp4a_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Box size can't be determined here */
	off_t res;
	uint32_t val32;
	uint16_t val16;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_MP4A);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 6;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Data reference index */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Reserved */
	skip = 8;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'channelcount' */
	val16 = htons(2);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'samplesize' */
	val16 = htons(16);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* Pre-defined & reserved */
	skip = 4;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* 'samplerate' */
	val32 = htonl(track->audio.sample_rate);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* esds */
	res = mp4_box_esds_write(mux, box, maxBytes - bytesWritten);
	if (res < 0)
		return res;
	bytesWritten += res;

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.5.2.2
 * Called from mp4_box_stsd_write, box->type cannot be used
 */
static off_t
mp4_box_mett_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{

	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 18; /* Does not include encoding/mime len */
	size_t encoding_len = 0;
	size_t mime_len = 0;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	if (track->metadata.content_encoding != NULL)
		encoding_len = strlen(track->metadata.content_encoding);
	if (track->metadata.mime_type != NULL)
		mime_len = strlen(track->metadata.mime_type);

	boxSize += encoding_len + mime_len;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_TEXT_METADATA_SAMPLE_ENTRY);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Reserved */
	skip = 6;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Data reference index */
	val16 = htons(1);
	MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

	/* 'content_encoding' */
	for (size_t i = 0; i < encoding_len; i++) {
		val8 = track->metadata.content_encoding[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}
	val8 = 0;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	/* 'mime_format' */
	for (size_t i = 0; i < mime_len; i++) {
		val8 = track->metadata.mime_type[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}
	val8 = 0;
	MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.5.2
 */
static off_t
mp4_box_stsd_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Box size can't be determined here */
	off_t res;
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	switch (track->type) {
	case MP4_TRACK_TYPE_VIDEO: 					// this is what we care about - sample description table (16 bytes) + video sample description entry (70 bytes)
		res = mp4_video_decoder_config_write(
			mux, box, maxBytes - bytesWritten, track->video.codec);
		if (res < 0)
			return res;
		bytesWritten += res;
		break;
	case MP4_TRACK_TYPE_AUDIO:
		res = mp4_box_mp4a_write(mux, box, maxBytes - bytesWritten);
		if (res < 0)
			return res;
		bytesWritten += res;
		break;
	case MP4_TRACK_TYPE_METADATA:
		res = mp4_box_mett_write(mux, box, maxBytes - bytesWritten);
		if (res < 0)
			return res;
		bytesWritten += res;
		break;
	default:
		return -EINVAL;
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}

/**
 * ISO/IEC 14496-12 8.6.1.2
 */
static off_t
mp4_box_stts_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->time_to_sample.count == 0)
		return 0;

	boxSize += 8 * track->time_to_sample.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(track->time_to_sample.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->time_to_sample.count; i++) {
		struct mp4_time_to_sample_entry *entry;
		entry = &track->time_to_sample.entries[i];

		/* 'sample_count' */
		val32 = htonl(entry->sampleCount);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 'sample_delta' */
		val32 = htonl(entry->sampleDelta);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.6.2
 */
static off_t
mp4_box_stss_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->sync.count == 0)
		return 0;

	boxSize += 4 * track->sync.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(track->sync.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->sync.count; i++) {
		/* 'sample_number' */
		val32 = htonl(track->sync.entries[i]);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.7.3.2
 */
static off_t
mp4_box_stsz_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 20; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->samples.count == 0)
		return 0;

	boxSize += 4 * track->samples.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'sample_size' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'sample_count */
	val32 = htonl(track->samples.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->samples.count; i++) {
		/* 'entry_size' */
		val32 = htonl(track->samples.sizes[i]);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.7.4
 */
static off_t
mp4_box_stsc_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->sample_to_chunk.count == 0)
		return 0;

	boxSize += 12 * track->sample_to_chunk.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(track->sample_to_chunk.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->sample_to_chunk.count; i++) {
		struct mp4_sample_to_chunk_entry *entry;
		entry = &track->sample_to_chunk.entries[i];

		/* 'first_chunk' */
		val32 = htonl(entry->firstChunk);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 'samples_per_chunk' */
		val32 = htonl(entry->samplesPerChunk);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 'sample_description_id' */
		val32 = htonl(entry->sampleDescriptionIndex);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.7.5
 */
static off_t
mp4_box_stco_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->chunks.count == 0)
		return 0;

	boxSize += 4 * track->chunks.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(track->chunks.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->chunks.count; i++) {
		/* 'chunk_offset' (32bits) */
		val32 = htonl(track->chunks.offsets[i]);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.7.5
 */
static off_t
mp4_box_co64_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_track *track;
	off_t bytesWritten = 0;
	off_t boxSize = 16; /* Box size without table length */
	uint32_t val32;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	track = box->writer.args;

	/* Do not write the atom if no data is present */
	if (track->chunks.count == 0)
		return 0;

	boxSize += 8 * track->chunks.count;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	val32 = htonl(track->chunks.count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	for (uint32_t i = 0; i < track->chunks.count; i++) {
		uint64_t offset = track->chunks.offsets[i];
		/* 'chunk_offset' */
		val32 = htonl(offset >> 32);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
		val32 = htonl(offset & 0xffffffff);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * Apple Quicktime File Format Specification
 * Called from mp4_box_meta_write, box->type cannot be used
 * https://developer.apple.com/
 * library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
 */
static off_t
mp4_box_keys_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	struct mp4_mux_metadata *meta;
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Can't be determine here */
	uint32_t val32, count = 0, index = 0;
	struct mp4_mux_metadata_info *meta_info;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	meta_info = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_KEYS_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entry_count' */
	struct list_node *start = meta_info->metadatas;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
	{
		if (meta->storage != MP4_MUX_META_META)
			continue;
		count++;
	}
	val32 = htonl(count);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'entries' */
	start = meta_info->metadatas;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
	{
		size_t len;
		if (meta->storage != MP4_MUX_META_META)
			continue;
		index++;
		/* 'Key_size' */
		len = strlen(meta->key);
		val32 = htonl(len + 8);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 'Key_namespace' */
		val32 = htonl(MP4_METADATA_NAMESPACE_MDTA);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 'Key_value' */
		for (size_t i = 0; i < len; i++) {
			uint8_t val8 = meta->key[i];
			MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
		}
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


static off_t mp4_box_write_meta_raw_entry(struct mp4_mux *mux,
					  const char *key,
					  int type,
					  uint8_t *data,
					  size_t len,
					  enum mp4_mux_meta_storage storage,
					  uint32_t index,
					  size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 24 + len;
	uint32_t val32;
	uint16_t val16;

	/* For udta root meta boxes, do not include data sub-box */
	if (storage == MP4_MUX_META_UDTA_ROOT)
		boxSize = 12 + len;

	/* Entry box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Entry box key:
	 * index for MP4_MUX_META_META, key for others */
	if (storage == MP4_MUX_META_META)
		val32 = htonl(index);
	else
		memcpy(&val32, key, sizeof(uint32_t));
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	if (storage != MP4_MUX_META_UDTA_ROOT) {
		/* Entry data sub-box 'size' */
		val32 = htonl(boxSize - bytesWritten);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* Entry data sub-box 'type' */
		val32 = htonl(MP4_DATA_BOX);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* Entry data sub-box data type */
		val32 = htonl(type);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* Entry data sub-box locale */
		val32 = htonl(0);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	} else {
		/* meta->value length */
		val16 = htons(len);
		MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);

		/* language code */
		val16 = htons(0x55c4);
		MP4_WRITE_16(mux->file, val16, bytesWritten, maxBytes);
	}

	/* Entry data sub-box contents */
	for (size_t i = 0; i < len; i++) {
		uint8_t val8 = data[i];
		MP4_WRITE_8(mux->file, val8, bytesWritten, maxBytes);
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


static off_t mp4_box_write_meta_entry(struct mp4_mux *mux,
				      struct mp4_mux_metadata *meta,
				      enum mp4_mux_meta_storage storage,
				      uint32_t index,
				      size_t maxBytes)
{
	return mp4_box_write_meta_raw_entry(mux,
					    meta->key,
					    MP4_METADATA_CLASS_UTF8,
					    (uint8_t *)meta->value,
					    strlen(meta->value),
					    storage,
					    index,
					    maxBytes);
}


static off_t mp4_box_udta_entry_write(struct mp4_mux *mux,
				      struct mp4_box *box,
				      size_t maxBytes)
{
	struct mp4_mux_metadata *meta;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	meta = box->writer.args;

	return mp4_box_write_meta_entry(
		mux, meta, MP4_MUX_META_UDTA_ROOT, 0, maxBytes);
}


/**
 * Apple Quicktime File Format Specification
 * Called from mp4_box_meta_write or mp4_box_meta_udta_write,
 * box->type cannot be used
 */
static off_t mp4_box_ilst_write(struct mp4_mux *mux,
				struct mp4_box *box,
				size_t maxBytes,
				enum mp4_mux_meta_storage storage)
{
	struct mp4_mux_metadata *meta;
	struct mp4_mux_metadata_info *meta_info;

	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Box size can't be determined here */
	uint32_t val32, index = 0;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	meta_info = box->writer.args;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(MP4_ILST_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	struct list_node *start = meta_info->metadatas;
	custom_walk(start, meta, node, struct mp4_mux_metadata)
	{
		off_t res;

		/* Filter metadatas according to storage */
		if (meta->storage != storage)
			continue;
		index++;

		res = mp4_box_write_meta_entry(
			mux, meta, storage, index, maxBytes - bytesWritten);
		if (res < 0)
			return res;
		bytesWritten += res;
	}

	/* Write cover if needed */
	if (meta_info->cover_type != MP4_METADATA_COVER_TYPE_UNKNOWN &&
	    storage == MP4_MUX_META_UDTA) {
		off_t res;
		int type = 0;
		switch (meta_info->cover_type) {
		case MP4_METADATA_COVER_TYPE_JPEG:
			type = MP4_METADATA_CLASS_JPEG;
			break;
		case MP4_METADATA_COVER_TYPE_PNG:
			type = MP4_METADATA_CLASS_PNG;
			break;
		case MP4_METADATA_COVER_TYPE_BMP:
			type = MP4_METADATA_CLASS_BMP;
			break;
		default:
			break;
		}
		res = mp4_box_write_meta_raw_entry(mux,
						   "covr",
						   type,
						   meta_info->cover,
						   meta_info->cover_size,
						   MP4_MUX_META_UDTA,
						   index,
						   maxBytes - bytesWritten);
		if (res < 0)
			return res;
		bytesWritten += res;
	}

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.11.1
 */
static off_t mp4_box_meta_udta_write(struct mp4_mux *mux,
				     struct mp4_box *box,
				     size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* Box size can't be determined here */
	off_t res;
	uint32_t val32;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	/* Box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'size', always 33 bytes */
	val32 = htonl(33);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'type' */
	val32 = htonl(MP4_HANDLER_REFERENCE_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'predefined' */
	val32 = htonl(MP4_MHLR);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box handler 'type' */
	val32 = htonl(MP4_METADATA_HANDLER_TYPE_MDIR);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box reserved */
	val32 = htonl(MP4_METADATA_HANDLER_TYPE_APPL);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box Reserved & 'name' */
	skip = 9;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Ilst sub-box */
	res = mp4_box_ilst_write(
		mux, box, maxBytes - bytesWritten, MP4_MUX_META_UDTA);
	if (res < 0)
		return res;
	bytesWritten += res;

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * Apple Quicktime File Format Specification
 * https://developer.apple.com/
 * library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
 */
static off_t
mp4_box_meta_write(struct mp4_mux *mux, struct mp4_box *box, size_t maxBytes)
{
	off_t bytesWritten = 0;
	off_t boxSize = 0; /* can't determine here */
	off_t res;
	uint32_t val32;
	size_t skip;

	if (mux == NULL || box == NULL || box->writer.args == NULL)
		return -EINVAL;

	/* box size */
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* box type */
	val32 = htonl(box->type);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box size */
	val32 = htonl(33); /* Always 33 bytes */
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'type' */
	val32 = htonl(MP4_HANDLER_REFERENCE_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box 'version' & 'flags' */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box predefined */
	val32 = htonl(0);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box handler 'type' */
	val32 = htonl(MP4_METADATA_NAMESPACE_MDTA);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Handler sub-box Reserved &'name' */
	skip = 13;
	MP4_WRITE_SKIP(mux->file, skip, bytesWritten, maxBytes);

	/* Keys sub-box */
	res = mp4_box_keys_write(mux, box, maxBytes - bytesWritten);
	if (res < 0)
		return res;
	bytesWritten += res;

	/* Ilst sub-box */
	res = mp4_box_ilst_write(
		mux, box, maxBytes - bytesWritten, MP4_MUX_META_META);
	if (res < 0)
		return res;
	bytesWritten += res;

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


struct mp4_box *mp4_box_new_container(struct mp4_box *parent, uint32_t type)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = type;
	box->writer.func = mp4_box_container_write; // writes this box and calls children boxes' write functions also
	box->writer.args = NULL;
	return box;
}


struct mp4_box *mp4_box_new_mvhd(struct mp4_box *parent, struct mp4_mux *mux)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_MOVIE_HEADER_BOX;
	box->writer.func = mp4_box_mvhd_write;
	box->writer.args = mux;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_tkhd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_TRACK_HEADER_BOX;
	box->writer.func = mp4_box_tkhd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_cdsc(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_REFERENCE_TYPE_DESCRIPTION;
	box->writer.func = mp4_box_tref_content_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_mdhd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_MEDIA_HEADER_BOX;
	box->writer.func = mp4_box_mdhd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_hdlr(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_HANDLER_REFERENCE_BOX;
	box->writer.func = mp4_box_hdlr_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_vmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_VIDEO_MEDIA_HEADER_BOX;
	box->writer.func = mp4_box_vmhd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_smhd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_SOUND_MEDIA_HEADER_BOX;
	box->writer.func = mp4_box_smhd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_nmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_NULL_MEDIA_HEADER_BOX;
	box->writer.func = mp4_box_nmhd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_dref(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_DATA_REFERENCE_BOX;
	box->writer.func = mp4_box_dref_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stsd(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_SAMPLE_DESCRIPTION_BOX;
	box->writer.func = mp4_box_stsd_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stts(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_DECODING_TIME_TO_SAMPLE_BOX;
	box->writer.func = mp4_box_stts_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stss(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_SYNC_SAMPLE_BOX;
	box->writer.func = mp4_box_stss_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stsc(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_SAMPLE_TO_CHUNK_BOX;
	box->writer.func = mp4_box_stsc_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stsz(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_SAMPLE_SIZE_BOX;
	box->writer.func = mp4_box_stsz_write;
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_stco(struct mp4_box *parent,
				 struct mp4_mux_track *track)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	box->type = MP4_CHUNK_OFFSET_BOX;
	box->writer.func = mp4_box_stco_write;
	for (size_t i = 0; i < track->chunks.count; i++) {
		if (track->chunks.offsets[i] > UINT32_MAX) {
			box->type = MP4_CHUNK_OFFSET_64_BOX;
			box->writer.func = mp4_box_co64_write;
			break;
		}
	}
	box->writer.args = track;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_meta(struct mp4_box *parent,
				 struct mp4_mux_metadata_info *meta_info)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;

	box->type = MP4_META_BOX;
	box->writer.func = mp4_box_meta_write;
	box->writer.args = meta_info;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_meta_udta(struct mp4_box *parent,
				      struct mp4_mux_metadata_info *meta_info)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;

	box->type = MP4_META_BOX;
	box->writer.func = mp4_box_meta_udta_write;
	box->writer.args = meta_info;
	box->writer.need_free = 0;
	return box;
}


struct mp4_box *mp4_box_new_udta_entry(struct mp4_box *parent,
				       struct mp4_mux_metadata *meta)
{
	struct mp4_box *box = mp4_box_new(parent);
	if (box == NULL)
		return box;
	/* Unused by udta_entry box */
	box->type = 0;
	box->writer.func = mp4_box_udta_entry_write;
	box->writer.args = meta;
	box->writer.need_free = 0;
	return box;
}


/**
 * ISO/IEC 14496-12 4.3
 */
off_t mp4_box_ftyp_write(struct mp4_mux *mux)
{
	off_t bytesWritten = 0;
	uint32_t val32;
	size_t maxBytes = mux->data_offset - ftello(mux->file);

	/* Box size */
	off_t boxSize = 8 * sizeof(uint32_t);
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box name */
	val32 = htonl(MP4_FILE_TYPE_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'major_brand' */
	val32 = htonl(MP4_ISOM);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'minor_version' */
	val32 = htonl(2);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* 'compatible_brands[]' */
	val32 = htonl(MP4_ISOM);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(MP4_ISO2);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(MP4_MP41);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	val32 = htonl(MP4_AVC1);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


static off_t
mp4_box_free_write_internal(struct mp4_mux *mux, size_t len, size_t maxBytes)
{
	off_t bytesWritten = 0;
	uint32_t val32;

	if (len < 8 || len > UINT32_MAX)
		return -EINVAL;

	/* Box size */
	off_t boxSize = len;
	val32 = htonl(boxSize);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Box name */
	val32 = htonl(MP4_FREE_BOX);
	MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

	/* Skip rest */
	MP4_WRITE_SKIP(mux->file, len - bytesWritten, bytesWritten, maxBytes);

	MP4_WRITE_CHECK_SIZE(mux->file, boxSize, bytesWritten);

	return bytesWritten;
}


/**
 * ISO/IEC 14496-12 8.1.2
 */
off_t mp4_box_free_write(struct mp4_mux *mux, size_t len)
{
	size_t maxBytes = mux->data_offset - ftello(mux->file);

	return mp4_box_free_write_internal(mux, len, maxBytes);
}


/**
 * ISO/IEC 14496-12 8.1.1
 */
off_t mp4_box_mdat_write(struct mp4_mux *mux, uint64_t size)
{
	off_t bytesWritten = 0;
	uint32_t val32;

	size_t maxBytes = 16;

	if (size <= UINT32_MAX) {
		/* Reserve for wide size if required */
		bytesWritten = mp4_box_free_write_internal(mux, 8, maxBytes);
		if (bytesWritten < 0)
			return bytesWritten;

		/* Box size */
		val32 = htonl(size);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* Box name */
		val32 = htonl(MP4_MDAT_BOX);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	} else {
		/* 8 more bytes as we use the free */
		size += 8;
		/* Wide size */
		val32 = htonl(1);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* Box name */
		val32 = htonl(MP4_MDAT_BOX);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);

		/* 64 bits size */
		val32 = htonl(size >> 32);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
		val32 = htonl(size & UINT32_MAX);
		MP4_WRITE_32(mux->file, val32, bytesWritten, maxBytes);
	}

	return bytesWritten;
}
