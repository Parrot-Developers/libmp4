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

#ifndef _MP4_H_
#define _MP4_H_

#ifndef ANDROID
#	ifndef _FILE_OFFSET_BITS
#		define _FILE_OFFSET_BITS 64
#	endif
#endif

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#	include <winsock2.h>
#else /* !_WIN32 */
#	include <arpa/inet.h>
#endif /* !_WIN32 */

#define ULOG_TAG libmp4
#include <ulog.h>

#include <futils/futils.h>
#include <libmp4.h>


/* clang-format off */
#define MP4_UUID                            0x75756964 /* "uuid" */
#define MP4_ROOT_BOX                        0x726f6f74 /* "root" */
#define MP4_FILE_TYPE_BOX                   0x66747970 /* "ftyp" */
#define MP4_MOVIE_BOX                       0x6d6f6f76 /* "moov" */
#define MP4_USER_DATA_BOX                   0x75647461 /* "udta" */
#define MP4_MOVIE_HEADER_BOX                0x6d766864 /* "mvhd" */
#define MP4_TRACK_BOX                       0x7472616b /* "trak" */
#define MP4_TRACK_HEADER_BOX                0x746b6864 /* "tkhd" */
#define MP4_TRACK_REFERENCE_BOX             0x74726566 /* "tref" */
#define MP4_MEDIA_BOX                       0x6d646961 /* "mdia" */
#define MP4_MEDIA_HEADER_BOX                0x6d646864 /* "mdhd" */
#define MP4_HANDLER_REFERENCE_BOX           0x68646c72 /* "hdlr" */
#define MP4_MEDIA_INFORMATION_BOX           0x6d696e66 /* "minf" */
#define MP4_VIDEO_MEDIA_HEADER_BOX          0x766d6864 /* "vmhd" */
#define MP4_SOUND_MEDIA_HEADER_BOX          0x736d6864 /* "smhd" */
#define MP4_HINT_MEDIA_HEADER_BOX           0x686d6864 /* "hmhd" */
#define MP4_NULL_MEDIA_HEADER_BOX           0x6e6d6864 /* "nmhd" */
#define MP4_DATA_INFORMATION_BOX            0x64696e66 /* "dinf" */
#define MP4_DATA_REFERENCE_BOX              0x64696566 /* "dref" */ /*TODO*/
#define MP4_SAMPLE_TABLE_BOX                0x7374626c /* "stbl" */
#define MP4_SAMPLE_DESCRIPTION_BOX          0x73747364 /* "stsd" */
#define MP4_AVC_DECODER_CONFIG_BOX          0x61766343 /* "avcC" */
#define MP4_AUDIO_DECODER_CONFIG_BOX        0x65736473 /* "esds" */
#define MP4_DECODING_TIME_TO_SAMPLE_BOX     0x73747473 /* "stts" */
#define MP4_SYNC_SAMPLE_BOX                 0x73747373 /* "stss" */
#define MP4_SAMPLE_SIZE_BOX                 0x7374737a /* "stsz" */
#define MP4_SAMPLE_TO_CHUNK_BOX             0x73747363 /* "stsc" */
#define MP4_CHUNK_OFFSET_BOX                0x7374636f /* "stco" */
#define MP4_CHUNK_OFFSET_64_BOX             0x636f3634 /* "co64" */
#define MP4_META_BOX                        0x6d657461 /* "meta" */
#define MP4_KEYS_BOX                        0x6b657973 /* "keys" */
#define MP4_ILST_BOX                        0x696c7374 /* "ilst" */
#define MP4_DATA_BOX                        0x64617461 /* "data" */
#define MP4_LOCATION_BOX                    0xa978797a /* ".xyz" */

#define MP4_HANDLER_TYPE_VIDEO              0x76696465 /* "vide" */
#define MP4_HANDLER_TYPE_AUDIO              0x736f756e /* "soun" */
#define MP4_HANDLER_TYPE_HINT               0x68696e74 /* "hint" */
#define MP4_HANDLER_TYPE_METADATA           0x6d657461 /* "meta" */
#define MP4_HANDLER_TYPE_TEXT               0x74657874 /* "text" */

#define MP4_REFERENCE_TYPE_HINT             0x68696e74 /* "hint" */
#define MP4_REFERENCE_TYPE_DESCRIPTION      0x63647363 /* "cdsc" */
#define MP4_REFERENCE_TYPE_HINT_USED        0x68696e64 /* "hind" */
#define MP4_REFERENCE_TYPE_CHAPTERS         0x63686170 /* "chap" */

#define MP4_METADATA_CLASS_UTF8             (1)
#define MP4_METADATA_CLASS_JPEG             (13)
#define MP4_METADATA_CLASS_PNG              (14)
#define MP4_METADATA_CLASS_BMP              (27)

#define MP4_METADATA_TAG_TYPE_ARTIST        0x00415254 /* ".ART" */
#define MP4_METADATA_TAG_TYPE_TITLE         0x006e616d /* ".nam" */
#define MP4_METADATA_TAG_TYPE_DATE          0x00646179 /* ".day" */
#define MP4_METADATA_TAG_TYPE_COMMENT       0x00636d74 /* ".cmt" */
#define MP4_METADATA_TAG_TYPE_COPYRIGHT     0x00637079 /* ".cpy" */
#define MP4_METADATA_TAG_TYPE_MAKER         0x006d616b /* ".mak" */
#define MP4_METADATA_TAG_TYPE_MODEL         0x006d6f64 /* ".mod" */
#define MP4_METADATA_TAG_TYPE_VERSION       0x00737772 /* ".swr" */
#define MP4_METADATA_TAG_TYPE_ENCODER       0x00746f6f /* ".too" */
#define MP4_METADATA_TAG_TYPE_COVER         0x636f7672 /* "covr" */
/* clang-format on */

#define MP4_METADATA_KEY_COVER "com.apple.quicktime.artwork"

#define MP4_MAC_TO_UNIX_EPOCH_OFFSET (0x7c25b080UL)

#define MP4_CHAPTERS_MAX 100
#define MP4_TRACK_REF_MAX 10


enum mp4_time_cmp {
	MP4_TIME_CMP_EXACT, /* exact match */
	MP4_TIME_CMP_LT, /* less than */
	MP4_TIME_CMP_GT, /* greater than */
	MP4_TIME_CMP_LT_EQ, /* less than or equal */
	MP4_TIME_CMP_GT_EQ, /* greater than or equal */
};


struct mp4_box {
	uint32_t size;
	uint32_t type;
	uint64_t largesize;
	uint8_t uuid[16];
	unsigned int level;
	struct mp4_box *parent;
	struct list_node children;

	struct list_node node;
};


struct mp4_time_to_sample_entry {
	uint32_t sampleCount;
	uint32_t sampleDelta;
};


struct mp4_sample_to_chunk_entry {
	uint32_t firstChunk;
	uint32_t samplesPerChunk;
	uint32_t sampleDescriptionIndex;
};


struct mp4_track {
	uint32_t id;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;
	uint32_t nextSample;
	uint64_t pendingSeekTime;
	uint32_t sampleCount;
	uint32_t *sampleSize;
	uint64_t *sampleDecodingTime;
	uint64_t *sampleOffset;
	uint32_t chunkCount;
	uint64_t *chunkOffset;
	uint32_t timeToSampleEntryCount;
	struct mp4_time_to_sample_entry *timeToSampleEntries;
	uint32_t sampleToChunkEntryCount;
	struct mp4_sample_to_chunk_entry *sampleToChunkEntries;
	uint32_t syncSampleEntryCount;
	uint32_t *syncSampleEntries;
	uint32_t referenceType;
	uint32_t referenceTrackId[MP4_TRACK_REF_MAX];
	unsigned int referenceTrackIdCount;

	enum mp4_video_codec videoCodec;
	uint32_t videoWidth;
	uint32_t videoHeight;
	uint16_t videoSpsSize;
	uint8_t *videoSps;
	uint16_t videoPpsSize;
	uint8_t *videoPps;

	enum mp4_audio_codec audioCodec;
	uint32_t audioChannelCount;
	uint32_t audioSampleSize;
	uint32_t audioSampleRate;
	uint32_t audioSpecificConfigSize;
	uint8_t *audioSpecificConfig;

	char *metadataContentEncoding;
	char *metadataMimeFormat;
	unsigned int staticMetadataCount;
	char **staticMetadataKey;
	char **staticMetadataValue;

	struct mp4_track *metadata;
	struct mp4_track *chapters;

	char *name;
	int enabled;
	int in_movie;
	int in_preview;

	struct list_node node;
};


struct mp4_file {
	FILE *file;
	off_t fileSize;
	off_t readBytes;
	struct mp4_box *root;
	struct list_node tracks;
	unsigned int trackCount;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;

	char *chaptersName[MP4_CHAPTERS_MAX];
	uint64_t chaptersTime[MP4_CHAPTERS_MAX];
	unsigned int chaptersCount;
	unsigned int finalMetadataCount;
	char **finalMetadataKey;
	char **finalMetadataValue;
	char *udtaLocationKey;
	char *udtaLocationValue;
	off_t finalCoverOffset;
	uint32_t finalCoverSize;
	enum mp4_metadata_cover_type finalCoverType;

	off_t udtaCoverOffset;
	uint32_t udtaCoverSize;
	enum mp4_metadata_cover_type udtaCoverType;
	off_t metaCoverOffset;
	uint32_t metaCoverSize;
	enum mp4_metadata_cover_type metaCoverType;

	unsigned int udtaMetadataCount;
	unsigned int udtaMetadataParseIdx;
	char **udtaMetadataKey;
	char **udtaMetadataValue;
	unsigned int metaMetadataCount;
	char **metaMetadataKey;
	char **metaMetadataValue;
};


struct mp4_demux {
	struct mp4_file mp4;
};


#define MP4_READ_32(_file, _val32, _readBytes)                                 \
	do {                                                                   \
		size_t _count = fread(&_val32, sizeof(uint32_t), 1, _file);    \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fread", errno);                            \
			return -errno;                                         \
		}                                                              \
		_readBytes += sizeof(uint32_t);                                \
	} while (0)

#define MP4_READ_16(_file, _val16, _readBytes)                                 \
	do {                                                                   \
		size_t _count = fread(&_val16, sizeof(uint16_t), 1, _file);    \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fread", errno);                            \
			return -errno;                                         \
		}                                                              \
		_readBytes += sizeof(uint16_t);                                \
	} while (0)

#define MP4_READ_8(_file, _val8, _readBytes)                                   \
	do {                                                                   \
		size_t _count = fread(&_val8, sizeof(uint8_t), 1, _file);      \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fread", errno);                            \
			return -errno;                                         \
		}                                                              \
		_readBytes += sizeof(uint8_t);                                 \
	} while (0)

#define MP4_READ_SKIP(_file, _nBytes, _readBytes)                              \
	do {                                                                   \
		__typeof__(_readBytes) _i_nBytes = _nBytes;                    \
		if (_i_nBytes > 0) {                                           \
			int _ret = fseeko(_file, _i_nBytes, SEEK_CUR);         \
			if (_ret != 0) {                                       \
				ULOG_ERRNO("fseeko", errno);                   \
				return -errno;                                 \
			}                                                      \
			_readBytes += _i_nBytes;                               \
		}                                                              \
	} while (0)

struct mp4_box *mp4_box_new(struct mp4_box *parent);


int mp4_box_destroy(struct mp4_box *box);


void mp4_box_log(struct mp4_box *box, int level);


off_t mp4_box_children_read(struct mp4_file *mp4,
			    struct mp4_box *parent,
			    off_t maxBytes,
			    struct mp4_track *track);


int mp4_track_is_sync_sample(struct mp4_track *track,
			     unsigned int sampleIdx,
			     int *prevSyncSampleIdx);


int mp4_track_find_sample_by_time(struct mp4_track *track,
				  uint64_t time,
				  enum mp4_time_cmp cmp,
				  int sync,
				  int start);


struct mp4_track *mp4_track_add(struct mp4_file *mp4);


int mp4_track_remove(struct mp4_file *mp4, struct mp4_track *track);


struct mp4_track *mp4_track_find(struct mp4_file *mp4, struct mp4_track *track);


struct mp4_track *mp4_track_find_by_idx(struct mp4_file *mp4,
					unsigned int track_idx);


struct mp4_track *mp4_track_find_by_id(struct mp4_file *mp4,
				       unsigned int track_id);


void mp4_tracks_destroy(struct mp4_file *mp4);


int mp4_tracks_build(struct mp4_file *mp4);


#endif /* !_MP4_H_ */
