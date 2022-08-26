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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#	include <winsock2.h>
#	include <sys/types.h>
	#ifdef _WIN64
	#	define fseeko _fseeki64
	#	define ftello _ftelli64
	#else
	#	define fseeko fseek
	#	define ftello ftell
	#endif
#else /* !_WIN32 */
#	include <arpa/inet.h>
#endif /* !_WIN32 */

#define ULOG_TAG libmp4
#include "libmp4.h"
#include "list.h"
#include "ulog.h"


#pragma comment(lib, "Ws2_32.lib")

/* clang-format off */
#define MP4_ISOM                            0x69736f6d /* "isom" */
#define MP4_ISO2                            0x69736f32 /* "iso2" */
#define MP4_MP41                            0x6d703431 /* "mp41" */
#define MP4_AVC1                            0x61766331 /* "avc1" */
#define MP4_HVC1                            0x68766331 /* "hvc1" */
#define MP4_MP4A                            0x6d703461 /* "mp4a" */
#define MP4_MP4V                            0x6d703476 /* "mp4v" */
#define MP4_UUID                            0x75756964 /* "uuid" */
#define MP4_MHLR                            0x6d686c72 /* "mhlr" */
#define MP4_ROOT_BOX                        0x726f6f74 /* "root" */
#define MP4_FILE_TYPE_BOX                   0x66747970 /* "ftyp" */
#define MP4_FREE_BOX                        0x66726565 /* "free" */
#define MP4_MDAT_BOX                        0x6d646174 /* "mdat" */
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
#define MP4_DATA_REFERENCE_BOX              0x64726566 /* "dref" */
#define MP4_SAMPLE_TABLE_BOX                0x7374626c /* "stbl" */
#define MP4_SAMPLE_DESCRIPTION_BOX          0x73747364 /* "stsd" */
#define MP4_AVC_DECODER_CONFIG_BOX          0x61766343 /* "avcC" */
#define MP4_HEVC_DECODER_CONFIG_BOX         0x68766343 /* "hvcC" */
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

#define MP4_DATA_REFERENCE_TYPE_URL         0x75726c20 /* "url " */

#define MP4_XML_METADATA_SAMPLE_ENTRY       0x6d657478 /* "metx" */
#define MP4_TEXT_METADATA_SAMPLE_ENTRY      0x6d657474 /* "mett" */

#define MP4_METADATA_NAMESPACE_MDTA         0x6d647461 /* "mdta" */
#define MP4_METADATA_HANDLER_TYPE_MDIR      0x6d646972 /* "mdir" */
#define MP4_METADATA_HANDLER_TYPE_APPL      0x6170706c /* "appl" */

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

/* Track flags definition */
#define TRACK_FLAG_ENABLED (1 << 0)
#define TRACK_FLAG_IN_MOVIE (1 << 1)
#define TRACK_FLAG_IN_PREVIEW (1 << 2)


enum mp4_h265_nalu_type {
	MP4_H265_NALU_TYPE_UNKNOWN = 0, /* Unknown type */
	MP4_H265_NALU_TYPE_VPS = 32, /* Video parameter set */
	MP4_H265_NALU_TYPE_SPS = 33, /* Sequence parameter set */
	MP4_H265_NALU_TYPE_PPS = 34, /* Picture parameter set */
};


enum mp4_time_cmp {
	MP4_TIME_CMP_EXACT, /* Exact match */
	MP4_TIME_CMP_LT, /* Less than */
	MP4_TIME_CMP_GT, /* Greater than */
	MP4_TIME_CMP_LT_EQ, /* Less than or equal */
	MP4_TIME_CMP_GT_EQ, /* Greater than or equal */
};


enum mp4_mux_meta_storage {
	/* Stored in moov/meta, keys/ilst format */
	MP4_MUX_META_META = 0,
	/* Stored in moov/udta/meta, ilst only format */
	MP4_MUX_META_UDTA,
	/* Stored in moov/udta, ilst only format */
	MP4_MUX_META_UDTA_ROOT,
};


struct mp4_box {
	uint32_t size;
	uint32_t type;
	uint64_t largesize;
	uint8_t uuid[16];
	unsigned int level;
	struct mp4_box *parent;
	struct list_node children;

	struct {
		off_t (*func)(struct mp4_mux *mux,
			      struct mp4_box *box,
			      size_t maxBytes);
		void *args;
		int need_free;
	} writer;

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


/* track structure used by demuxer */
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
	uint32_t sampleMaxSize;
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

	struct mp4_video_decoder_config vdc;

	enum mp4_audio_codec audioCodec;
	uint32_t audioChannelCount;
	uint32_t audioSampleSize;
	uint32_t audioSampleRate;
	uint32_t audioSpecificConfigSize;
	uint8_t *audioSpecificConfig;

	char *contentEncoding;
	char *mimeFormat;
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
	ptrdiff_t fileSize;
	ptrdiff_t readBytes;
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


struct mp4_mux_metadata_info {
	struct list_node *metadatas;
	uint8_t *cover;
	enum mp4_metadata_cover_type cover_type;
	size_t cover_size;
};


/* track structure used by muxer */
struct mp4_mux_track {
	uint32_t id;
	char *name;
	uint32_t flags;
	uint32_t referenceTrackId[MP4_TRACK_REF_MAX];
	size_t referenceTrackIdCount;
	uint32_t ref;
	int has_ref;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t duration_moov;
	uint64_t creation_time;
	uint64_t modification_time;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint32_t *sizes;
		uint64_t *decoding_times;
		uint64_t *offsets;
	} samples;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint64_t *offsets;
	} chunks;
	struct {
		uint32_t count;
		uint32_t capacity;
		struct mp4_time_to_sample_entry *entries;
	} time_to_sample;
	struct {
		uint32_t count;
		uint32_t capacity;
		struct mp4_sample_to_chunk_entry *entries;
	} sample_to_chunk;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint32_t *entries;
	} sync;

	union {
		struct mp4_video_decoder_config video;
		struct {
			enum mp4_audio_codec codec;
			uint32_t channel_count;
			uint32_t sample_size;
			uint32_t sample_rate;
			uint32_t specific_config_size;
			uint8_t *specific_config;
		} audio;
		struct {
			char *content_encoding;
			char *mime_type;
		} metadata;
	};

	struct mp4_mux_metadata_info track_metadata;

	struct list_node metadatas;
	struct list_node node;
};

struct mp4_mux_metadata {
	char *key;
	char *value;
	enum mp4_mux_meta_storage storage;

	struct list_node node;
};

struct mp4_mux {
	FILE *file;
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t timescale;
	off_t data_offset;
	off_t boxes_offset;
	/* Tracks */
	struct list_node tracks;
	uint32_t track_count;
	/* Metadata */
	struct list_node metadatas;
	struct mp4_mux_metadata_info file_metadata;
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
		size_t _i_nBytes = _nBytes;                                    \
		if (_i_nBytes > 0) {                                           \
			int _ret = fseeko(_file, _i_nBytes, SEEK_CUR);         \
			if (_ret != 0) {                                       \
				return -errno;                                 \
			}                                                      \
			_readBytes += _i_nBytes;                               \
		}                                                              \
	} while (0)


#define MP4_WRITE_32(_file, _val32, _writeBytes, _maxBytes)                    \
	do {                                                                   \
		if (_writeBytes + sizeof(uint32_t) > _maxBytes)                \
			return -ENOSPC;                                        \
		size_t _count = fwrite(&_val32, sizeof(uint32_t), 1, _file);   \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fwrite", errno);                           \
			return -errno;                                         \
		}                                                              \
		_writeBytes += sizeof(uint32_t);                               \
	} while (0)


#define MP4_WRITE_16(_file, _val16, _writeBytes, _maxBytes)                    \
	do {                                                                   \
		if (_writeBytes + sizeof(uint16_t) > _maxBytes)                \
			return -ENOSPC;                                        \
		size_t _count = fwrite(&_val16, sizeof(uint16_t), 1, _file);   \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fwrite", errno);                           \
			return -errno;                                         \
		}                                                              \
		_writeBytes += sizeof(uint16_t);                               \
	} while (0)


#define MP4_WRITE_8(_file, _val8, _writeBytes, _maxBytes)                      \
	do {                                                                   \
		if (_writeBytes + sizeof(uint8_t) > _maxBytes)                 \
			return -ENOSPC;                                        \
		size_t _count = fwrite(&_val8, sizeof(uint8_t), 1, _file);     \
		if (_count != 1) {                                             \
			ULOG_ERRNO("fwrite", errno);                           \
			return -errno;                                         \
		}                                                              \
		_writeBytes += sizeof(uint8_t);                                \
	} while (0)


#define MP4_WRITE_SKIP(_file, _byteCount, _writeBytes, _maxBytes)              \
	do {                                                                   \
		size_t _i_nBytes = _byteCount;                                 \
		if (_writeBytes + _i_nBytes > _maxBytes)                       \
			return -ENOSPC;                                        \
		if (fseeko(_file, _i_nBytes, SEEK_CUR) != 0) {                 \
			ULOG_ERRNO("fseek", errno);                            \
			return -errno;                                         \
		}                                                              \
		_writeBytes += _i_nBytes;                                      \
	} while (0)

#define MP4_WRITE_CHECK_SIZE(_file, _computedSize, _actualSize)                \
	do {                                                                   \
		if (_computedSize != _actualSize) {                            \
			uint32_t _size32 = htonl(_actualSize);                 \
			if (_computedSize != 0)                                \
				ULOGE("bad size in box (%zu instead of %zu),"  \
				      " fixing size",                          \
				      (size_t)_actualSize,                     \
				      (size_t)_computedSize);                  \
			if (fseeko(_file, -_actualSize, SEEK_CUR) != 0) {      \
				ULOG_ERRNO("fseeko", errno);                    \
				return -errno;                                 \
			}                                                      \
			size_t _count =                                        \
				fwrite(&_size32, sizeof(uint32_t), 1, _file);  \
			if (_count != 1) {                                     \
				ULOG_ERRNO("fwrite", errno);                   \
				return -errno;                                 \
			}                                                      \
			if (fseeko(_file,                                      \
				   _actualSize - sizeof(uint32_t),             \
				   SEEK_CUR) != 0) {                           \
				ULOG_ERRNO("fseeko", errno);                    \
				return -errno;                                 \
			}                                                      \
		}                                                              \
	} while (0)


static inline char *xstrdup(const char *s)
{
	return s == NULL ? NULL : strdup(s);
}


struct mp4_box *mp4_box_new(struct mp4_box *parent);


/* mp4_box for mux creation */
struct mp4_box *mp4_box_new_container(struct mp4_box *parent, uint32_t type);
struct mp4_box *mp4_box_new_mvhd(struct mp4_box *parent, struct mp4_mux *mux);
struct mp4_box *mp4_box_new_tkhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_cdsc(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_mdhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_hdlr(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_vmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_smhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_nmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_dref(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stts(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stss(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsc(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsz(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stco(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_meta(struct mp4_box *parent,
				 struct mp4_mux_metadata_info *meta_info);
struct mp4_box *mp4_box_new_meta_udta(struct mp4_box *parent,
				      struct mp4_mux_metadata_info *meta_info);
struct mp4_box *mp4_box_new_udta_entry(struct mp4_box *parent,
				       struct mp4_mux_metadata *meta);


void mp4_box_destroy(struct mp4_box *box);


void mp4_box_log(struct mp4_box *box, int level);


ptrdiff_t mp4_box_children_read(struct mp4_file *mp4,
			    struct mp4_box *parent,
			    ptrdiff_t maxBytes,
			    struct mp4_track *track);


off_t mp4_box_ftyp_write(struct mp4_mux *mux);


off_t mp4_box_free_write(struct mp4_mux *mux, size_t len);


off_t mp4_box_mdat_write(struct mp4_mux *mux, uint64_t size);


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


void mp4_video_decoder_config_destroy(struct mp4_video_decoder_config *vdc);


#endif /* !_MP4_H_ */
