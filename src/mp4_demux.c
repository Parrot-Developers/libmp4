/**
 * @file mp4_demux.c
 * @brief MP4 file library - demuxer implementation
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

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <libmp4.h>

#include "mp4_log.h"


#define MP4_UUID                            0x75756964 // "uuid"
#define MP4_FILE_TYPE_BOX                   0x66747970 // "ftyp"
#define MP4_MOVIE_BOX                       0x6d6f6f76 // "moov"
#define MP4_USER_DATA_BOX                   0x75647461 // "udta"
#define MP4_MOVIE_HEADER_BOX                0x6d766864 // "mvhd"
#define MP4_TRACK_BOX                       0x7472616b // "trak"
#define MP4_TRACK_HEADER_BOX                0x746b6864 // "tkhd"
#define MP4_TRACK_REFERENCE_BOX             0x74726566 // "tref"
#define MP4_MEDIA_BOX                       0x6d646961 // "mdia"
#define MP4_MEDIA_HEADER_BOX                0x6d646864 // "mdhd"
#define MP4_HANDLER_REFERENCE_BOX           0x68646c72 // "hdlr"
#define MP4_MEDIA_INFORMATION_BOX           0x6d696e66 // "minf"
#define MP4_VIDEO_MEDIA_HEADER_BOX          0x766d6864 // "vmhd"
#define MP4_SOUND_MEDIA_HEADER_BOX          0x736d6864 // "smhd"
#define MP4_HINT_MEDIA_HEADER_BOX           0x686d6864 // "hmhd"
#define MP4_NULL_MEDIA_HEADER_BOX           0x6e6d6864 // "nmhd"
#define MP4_DATA_INFORMATION_BOX            0x64696e66 // "dinf"
#define MP4_DATA_REFERENCE_BOX              0x64696566 // "dref" //TODO
#define MP4_SAMPLE_TABLE_BOX                0x7374626c // "stbl"
#define MP4_SAMPLE_DESCRIPTION_BOX          0x73747364 // "stsd"
#define MP4_AVC_DECODER_CONFIG_BOX          0x61766343 // "avcC"
#define MP4_DECODING_TIME_TO_SAMPLE_BOX     0x73747473 // "stts"
#define MP4_SYNC_SAMPLE_BOX                 0x73747373 // "stss"
#define MP4_SAMPLE_SIZE_BOX                 0x7374737a // "stsz"
#define MP4_SAMPLE_TO_CHUNK_BOX             0x73747363 // "stsc"
#define MP4_CHUNK_OFFSET_BOX                0x7374636f // "stco"
#define MP4_CHUNK_OFFSET_64_BOX             0x636f3634 // "co64"
#define MP4_META_BOX                        0x6d657461 // "meta"
#define MP4_ILST_BOX                        0x696c7374 // "ilst"
#define MP4_DATA_BOX                        0x64617461 // "data"

#define MP4_HANDLER_TYPE_VIDEO              0x76696465 // "vide"
#define MP4_HANDLER_TYPE_AUDIO              0x736f756e // "soun"
#define MP4_HANDLER_TYPE_HINT               0x68696e74 // "hint"
#define MP4_HANDLER_TYPE_METADATA           0x6d657461 // "meta"
#define MP4_HANDLER_TYPE_TEXT               0x74657874 // "text"

#define MP4_REFERENCE_TYPE_HINT             0x68696e74 // "hint"
#define MP4_REFERENCE_TYPE_DESCRIPTION      0x63647363 // "cdsc"
#define MP4_REFERENCE_TYPE_HINT_USED        0x68696e64 // "hind"
#define MP4_REFERENCE_TYPE_CHAPTERS         0x63686170 // "chap"

#define MP4_TAG_TYPE_ARTIST                 0x00415254 // ".ART"
#define MP4_TAG_TYPE_TITLE                  0x006e616d // ".nam"
#define MP4_TAG_TYPE_DATE                   0x00646179 // ".day"
#define MP4_TAG_TYPE_COMMENT                0x00636d74 // ".cmt"
#define MP4_TAG_TYPE_COPYRIGHT              0x00637079 // ".cpy"
#define MP4_TAG_TYPE_MAKER                  0x006d616b // ".mak"
#define MP4_TAG_TYPE_MODEL                  0x006d6f64 // ".mod"
#define MP4_TAG_TYPE_VERSION                0x00737772 // ".swr"
#define MP4_TAG_TYPE_ENCODER                0x00746f6f // ".too"
#define MP4_TAG_TYPE_COVER                  0x636f7672 // "covr"


#define MP4_CHAPTERS_MAX (100)


typedef struct
{
    uint32_t size;
    uint32_t type;
    uint64_t largesize;
    uint8_t uuid[16];

} mp4_box_t;


typedef struct mp4_box_item_s
{
    struct mp4_box_item_s *parent;
    struct mp4_box_item_s *child;
    struct mp4_box_item_s *prev;
    struct mp4_box_item_s *next;

    mp4_box_t box;

} mp4_box_item_t;


typedef struct
{
    uint32_t sampleCount;
    uint32_t sampleDelta;

} mp4_time_to_sample_entry_t;


typedef struct
{
    uint32_t firstChunk;
    uint32_t samplesPerChunk;
    uint32_t sampleDescriptionIndex;

} mp4_sample_to_chunk_entry_t;


typedef struct mp4_track_s
{
    uint32_t id;
    mp4_track_type_t type;
    uint32_t timescale;
    uint64_t duration;
    uint32_t currentSample;
    uint32_t sampleCount;
    uint32_t *sampleSize;
    uint64_t *sampleDecodingTime;
    uint32_t *sampleToChunk;
    uint32_t chunkCount;
    uint64_t *chunkOffset;
    uint32_t timeToSampleEntryCount;
    mp4_time_to_sample_entry_t *timeToSampleEntries;
    uint32_t sampleToChunkEntryCount;
    mp4_sample_to_chunk_entry_t *sampleToChunkEntries;
    uint32_t syncSampleEntryCount;
    uint32_t *syncSampleEntries;
    uint32_t referenceType;
    uint32_t referenceTrackId;

    mp4_video_codec_t videoCodec;
    uint32_t videoWidth;
    uint32_t videoHeight;
    uint16_t videoSpsSize;
    uint8_t *videoSps;
    uint16_t videoPpsSize;
    uint8_t *videoPps;

    mp4_audio_codec_t audioCodec;
    uint32_t audioChannelCount;
    uint32_t audioSampleSize;
    uint32_t audioSampleRate;

    char *metadataContentEncoding;
    char *metadataMimeFormat;

    struct mp4_track_s *ref;
    struct mp4_track_s *metadata;
    struct mp4_track_s *chapters;

    struct mp4_track_s *prev;
    struct mp4_track_s *next;

} mp4_track_t;


typedef struct mp4_demux
{
    FILE *file;
    off_t fileSize;
    off_t readBytes;
    mp4_box_item_t root;
    mp4_track_t *track;
    unsigned int trackCount;
    uint32_t timescale;

    char *chaptersName[MP4_CHAPTERS_MAX];
    uint64_t chaptersTime[MP4_CHAPTERS_MAX];
    unsigned int chaptersCount;
    char *tags[MP4_METADATA_TAG_MAX];
    off_t coverOffset;
    uint32_t coverSize;

} mp4_demux_t;


#define MP4_READ_32(_file, _val32, _readBytes) \
    do { \
        size_t _count = fread(&_val32, sizeof(uint32_t), 1, _file); \
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -1, \
                "failed to read %zu bytes from file", sizeof(uint32_t)); \
        _readBytes += sizeof(uint32_t); \
    } while (0)

#define MP4_READ_16(_file, _val16, _readBytes) \
    do { \
        size_t _count = fread(&_val16, sizeof(uint16_t), 1, _file); \
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -1, \
                "failed to read %zu bytes from file", sizeof(uint16_t)); \
        _readBytes += sizeof(uint16_t); \
    } while (0)

#define MP4_READ_8(_file, _val8, _readBytes) \
    do { \
        size_t _count = fread(&_val8, sizeof(uint8_t), 1, _file); \
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -1, \
                "failed to read %zu bytes from file", sizeof(uint8_t)); \
        _readBytes += sizeof(uint8_t); \
    } while (0)

#define MP4_SKIP(_file, _readBytes, _maxBytes) \
    do { \
        if (_readBytes < _maxBytes) \
        { \
            int _ret = fseeko(_file, _maxBytes - _readBytes, SEEK_CUR); \
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_ret != -1), -1, \
                    "failed to seek %ld bytes forward in file", \
                    _maxBytes - _readBytes); \
            _readBytes = _maxBytes; \
        } \
    } while (0)


static off_t mp4_demux_parse_children(mp4_demux_t *demux,
                                      mp4_box_item_t *parent,
                                      off_t maxBytes,
                                      mp4_track_t *track);


static int mp4_demux_is_sync_sample(mp4_demux_t *demux,
                                    mp4_track_t *track,
                                    unsigned int sampleIdx,
                                    int *prevSyncSampleIdx)
{
    unsigned int i;

    if (!track->syncSampleEntries)
    {
        return 1;
    }

    for (i = 0; i < track->syncSampleEntryCount; i++)
    {
        if (track->syncSampleEntries[i] - 1 == sampleIdx)
        {
            return 1;
        }
        else if (track->syncSampleEntries[i] - 1 > sampleIdx)
        {
            if ((prevSyncSampleIdx) && (i > 0))
            {
                *prevSyncSampleIdx = track->syncSampleEntries[i - 1] - 1;
            }
            return 0;
        }
    }

    if ((prevSyncSampleIdx) && (i > 0))
    {
        *prevSyncSampleIdx = track->syncSampleEntries[i - 1] - 1;
    }
    return 0;
}


static off_t mp4_demux_parse_file_type_box(mp4_demux_t *demux,
                                           mp4_box_item_t *parent,
                                           off_t maxBytes)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* major_brand */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t majorBrand = ntohl(val32);
    MP4_LOGD("# ftyp: major_brand=%c%c%c%c",
             (char)((majorBrand >> 24) & 0xFF), (char)((majorBrand >> 16) & 0xFF),
             (char)((majorBrand >> 8) & 0xFF), (char)(majorBrand & 0xFF));

    /* minor_version */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t minorVersion = ntohl(val32);
    MP4_LOGD("# ftyp: minor_version=%" PRIu32, minorVersion);

    int k = 0;
    while (boxReadBytes + 4 <= maxBytes)
    {
        /* compatible_brands[] */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t compatibleBrands = ntohl(val32);
        MP4_LOGD("# ftyp: compatible_brands[%d]=%c%c%c%c", k,
                 (char)((compatibleBrands >> 24) & 0xFF), (char)((compatibleBrands >> 16) & 0xFF),
                 (char)((compatibleBrands >> 8) & 0xFF), (char)(compatibleBrands & 0xFF));
        k++;
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_movie_header_box(mp4_demux_t *demux,
                                              mp4_box_item_t *parent,
                                              off_t maxBytes)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 25 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 25 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# mvhd: version=%d", version);
    MP4_LOGD("# mvhd: flags=%" PRIu32, flags);

    if (version == 1)
    {
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 28 * 4), -1,
                "invalid size: %ld expected %d", maxBytes, 28 * 4);

        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# mvhd: creation_time=%" PRIu64, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# mvhd: modification_time=%" PRIu64, modificationTime);

        /* timescale */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        demux->timescale = ntohl(val32);
        MP4_LOGD("# mvhd: timescale=%" PRIu32, demux->timescale);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t duration = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        unsigned int hrs = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 / 60);
        unsigned int min = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((duration + demux->timescale / 2) / demux->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)", duration, hrs, min, sec);
    }
    else
    {
        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t creationTime = ntohl(val32);
        MP4_LOGD("# mvhd: creation_time=%" PRIu32, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t modificationTime = ntohl(val32);
        MP4_LOGD("# mvhd: modification_time=%" PRIu32, modificationTime);

        /* timescale */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        demux->timescale = ntohl(val32);
        MP4_LOGD("# mvhd: timescale=%" PRIu32, demux->timescale);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t duration = ntohl(val32);
        unsigned int hrs = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 / 60);
        unsigned int min = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((duration + demux->timescale / 2) / demux->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# mvhd: duration=%" PRIu32 " (%02d:%02d:%02d)", duration, hrs, min, sec);
    }

    /* rate */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float rate = (float)ntohl(val32) / 65536.;
    MP4_LOGD("# mvhd: rate=%.4f", rate);

    /* volume & reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
    MP4_LOGD("# mvhd: volume=%.2f", volume);

    /* reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    MP4_READ_32(demux->file, val32, boxReadBytes);

    /* matrix */
    int k;
    for (k = 0; k < 9; k++)
    {
        MP4_READ_32(demux->file, val32, boxReadBytes);
    }

    /* pre_defined */
    for (k = 0; k < 6; k++)
    {
        MP4_READ_32(demux->file, val32, boxReadBytes);
    }

    /* next_track_ID */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t next_track_ID = ntohl(val32);
    MP4_LOGD("# mvhd: next_track_ID=%" PRIu32, next_track_ID);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_track_header_box(mp4_demux_t *demux,
                                              mp4_box_item_t *parent,
                                              off_t maxBytes,
                                              mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 21 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 21 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# tkhd: version=%d", version);
    MP4_LOGD("# tkhd: flags=%" PRIu32, flags);

    if (version == 1)
    {
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24 * 4), -1,
                "invalid size: %ld expected %d", maxBytes, 24 * 4);

        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# tkhd: creation_time=%" PRIu64, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# tkhd: modification_time=%" PRIu64, modificationTime);

        /* track_ID */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->id = ntohl(val32);
        MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

        /* reserved */
        MP4_READ_32(demux->file, val32, boxReadBytes);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t duration = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        unsigned int hrs = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 / 60);
        unsigned int min = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((duration + demux->timescale / 2) / demux->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# tkhd: duration=%" PRIu64 " (%02d:%02d:%02d)", duration, hrs, min, sec);
    }
    else
    {
        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t creationTime = ntohl(val32);
        MP4_LOGD("# tkhd: creation_time=%" PRIu32, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t modificationTime = ntohl(val32);
        MP4_LOGD("# tkhd: modification_time=%" PRIu32, modificationTime);

        /* track_ID */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->id = ntohl(val32);
        MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

        /* reserved */
        MP4_READ_32(demux->file, val32, boxReadBytes);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t duration = ntohl(val32);
        unsigned int hrs = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 / 60);
        unsigned int min = (unsigned int)((duration + demux->timescale / 2) / demux->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((duration + demux->timescale / 2) / demux->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# tkhd: duration=%" PRIu32 " (%02d:%02d:%02d)", duration, hrs, min, sec);
    }

    /* reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    MP4_READ_32(demux->file, val32, boxReadBytes);

    /* layer & alternate_group */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    int16_t layer = (int16_t)(ntohl(val32) >> 16);
    int16_t alternateGroup = (int16_t)(ntohl(val32) & 0xFFFF);
    MP4_LOGD("# tkhd: layer=%i", layer);
    MP4_LOGD("# tkhd: alternate_group=%i", alternateGroup);

    /* volume & reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
    MP4_LOGD("# tkhd: volume=%.2f", volume);

    /* matrix */
    int k;
    for (k = 0; k < 9; k++)
    {
        MP4_READ_32(demux->file, val32, boxReadBytes);
    }

    /* width */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float width = (float)ntohl(val32) / 65536.;
    MP4_LOGD("# tkhd: width=%.2f", width);

    /* height */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float height = (float)ntohl(val32) / 65536.;
    MP4_LOGD("# tkhd: height=%.2f", height);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_track_reference_box(mp4_demux_t *demux,
                                                 mp4_box_item_t *parent,
                                                 off_t maxBytes,
                                                 mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 3 * 4);

    /* reference type size */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t referenceTypeSize = ntohl(val32);
    MP4_LOGD("# tref: reference_type_size=%" PRIu32, referenceTypeSize);

    /* reference type */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->referenceType = ntohl(val32);
    MP4_LOGD("# tref: reference_type=%c%c%c%c",
             (char)((track->referenceType >> 24) & 0xFF), (char)((track->referenceType >> 16) & 0xFF),
             (char)((track->referenceType >> 8) & 0xFF), (char)(track->referenceType & 0xFF));

    /* track IDs */
    //NB: only read the first track ID, ignore multiple references
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->referenceTrackId = ntohl(val32);
    MP4_LOGD("# tref: track_id=%" PRIu32, track->referenceTrackId);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_media_header_box(mp4_demux_t *demux,
                                              mp4_box_item_t *parent,
                                              off_t maxBytes,
                                              mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 6 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# mdhd: version=%d", version);
    MP4_LOGD("# mdhd: flags=%" PRIu32, flags);

    if (version == 1)
    {
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9 * 4), -1,
                "invalid size: %ld expected %d", maxBytes, 9 * 4);

        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# mdhd: creation_time=%" PRIu64, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        MP4_LOGD("# mdhd: modification_time=%" PRIu64, modificationTime);

        /* timescale */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->timescale = ntohl(val32);
        MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->duration = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        unsigned int hrs = (unsigned int)((track->duration + track->timescale / 2) / track->timescale / 60 / 60);
        unsigned int min = (unsigned int)((track->duration + track->timescale / 2) / track->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((track->duration + track->timescale / 2) / track->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)", track->duration, hrs, min, sec);
    }
    else
    {
        /* creation_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t creationTime = ntohl(val32);
        MP4_LOGD("# mdhd: creation_time=%" PRIu32, creationTime);

        /* modification_time */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        uint32_t modificationTime = ntohl(val32);
        MP4_LOGD("# mdhd: modification_time=%" PRIu32, modificationTime);

        /* timescale */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->timescale = ntohl(val32);
        MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

        /* duration */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->duration = (uint64_t)ntohl(val32);
        unsigned int hrs = (unsigned int)((track->duration + track->timescale / 2) / track->timescale / 60 / 60);
        unsigned int min = (unsigned int)((track->duration + track->timescale / 2) / track->timescale / 60 - hrs * 60);
        unsigned int sec = (unsigned int)((track->duration + track->timescale / 2) / track->timescale - hrs * 60 * 60 - min * 60);
        MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)", track->duration, hrs, min, sec);
    }

    /* language & pre_defined */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint16_t language = (uint16_t)(ntohl(val32) >> 16) & 0x7FFF;
    MP4_LOGD("# mdhd: language=%" PRIu16, language);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_video_media_header_box(mp4_demux_t *demux,
                                                    mp4_box_item_t *parent,
                                                    off_t maxBytes,
                                                    mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;
    uint16_t val16;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 3 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# vmhd: version=%d", version);
    MP4_LOGD("# vmhd: flags=%" PRIu32, flags);

    /* graphicsmode */
    MP4_READ_16(demux->file, val16, boxReadBytes);
    uint16_t graphicsmode = ntohs(val16);
    MP4_LOGD("# vmhd: graphicsmode=%" PRIu16, graphicsmode);

    /* opcolor */
    uint16_t opcolor[3];
    MP4_READ_16(demux->file, val16, boxReadBytes);
    opcolor[0] = ntohs(val16);
    MP4_READ_16(demux->file, val16, boxReadBytes);
    opcolor[1] = ntohs(val16);
    MP4_READ_16(demux->file, val16, boxReadBytes);
    opcolor[2] = ntohs(val16);
    MP4_LOGD("# vmhd: opcolor=(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
             opcolor[0], opcolor[1], opcolor[2]);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_sound_media_header_box(mp4_demux_t *demux,
                                                    mp4_box_item_t *parent,
                                                    off_t maxBytes,
                                                    mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 2 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 2 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# smhd: version=%d", version);
    MP4_LOGD("# smhd: flags=%" PRIu32, flags);

    /* balance & reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    float balance = (float)((int16_t)((ntohl(val32) >> 16) & 0xFFFF)) / 256.;
    MP4_LOGD("# smhd: balance=%.2f", balance);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_hint_media_header_box(mp4_demux_t *demux,
                                                   mp4_box_item_t *parent,
                                                   off_t maxBytes,
                                                   mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 5 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 5 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# hmhd: version=%d", version);
    MP4_LOGD("# hmhd: flags=%" PRIu32, flags);

    /* maxPDUsize & avgPDUsize */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint16_t maxPDUsize = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
    uint16_t avgPDUsize = (uint16_t)(ntohl(val32) & 0xFFFF);
    MP4_LOGD("# hmhd: maxPDUsize=%" PRIu16, maxPDUsize);
    MP4_LOGD("# hmhd: avgPDUsize=%" PRIu16, avgPDUsize);

    /* maxbitrate */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t maxbitrate = ntohl(val32);
    MP4_LOGD("# hmhd: maxbitrate=%" PRIu32, maxbitrate);

    /* avgbitrate */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t avgbitrate = ntohl(val32);
    MP4_LOGD("# hmhd: avgbitrate=%" PRIu32, avgbitrate);

    /* reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_null_media_header_box(mp4_demux_t *demux,
                                                   mp4_box_item_t *parent,
                                                   off_t maxBytes,
                                                   mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4), -1,
            "invalid size: %ld expected %d", maxBytes, 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# nmhd: version=%d", version);
    MP4_LOGD("# nmhd: flags=%" PRIu32, flags);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_handler_reference_box(mp4_demux_t *demux,
                                                   mp4_box_item_t *parent,
                                                   off_t maxBytes,
                                                   mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 6 * 4);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# hdlr: version=%d", version);
    MP4_LOGD("# hdlr: flags=%" PRIu32, flags);

    /* pre_defined */
    MP4_READ_32(demux->file, val32, boxReadBytes);

    /* handler_type */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t handlerType = ntohl(val32);
    MP4_LOGD("# hdlr: handler_type=%c%c%c%c",
             (char)((handlerType >> 24) & 0xFF), (char)((handlerType >> 16) & 0xFF),
             (char)((handlerType >> 8) & 0xFF), (char)(handlerType & 0xFF));

    if ((track) && (parent) && (parent->parent) && (parent->parent->box.type == MP4_MEDIA_BOX))
    {
        switch (handlerType)
        {
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
    {
        MP4_READ_32(demux->file, val32, boxReadBytes);
    }

    char name[100];
    for (k = 0; (k < sizeof(name) - 1) && (boxReadBytes < maxBytes); k++)
    {
        MP4_READ_8(demux->file, name[k], boxReadBytes);
        if (name[k] == '\0')
        {
            break;
        }
    }
    name[k + 1] = '\0';
    MP4_LOGD("# hdlr: name=%s", name);

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_avc_decoder_configuration_box(mp4_demux_t *demux,
                                                           off_t maxBytes,
                                                           mp4_track_t *track)
{
    off_t boxReadBytes = 0, minBytes = 6;
    uint32_t val32;
    uint16_t val16;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
            "invalid size: %ld expected %ld", maxBytes, minBytes);

    /* version & profile & level */
    MP4_READ_32(demux->file, val32, boxReadBytes);
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
    MP4_READ_16(demux->file, val16, boxReadBytes);
    val16 = htons(val16);
    uint8_t lengthSize = ((val16 >> 8) & 0x3) + 1;
    uint8_t spsCount = val16 & 0x1F;
    MP4_LOGD("# avcC: length_size=%d", lengthSize);
    MP4_LOGD("# avcC: sps_count=%d", spsCount);

    minBytes += 2 * spsCount;
    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
            "invalid size: %ld expected %ld", maxBytes, minBytes);

    int i;
    for (i = 0; i < spsCount; i++)
    {
        /* sps_length */
        MP4_READ_16(demux->file, val16, boxReadBytes);
        uint16_t spsLength = htons(val16);
        MP4_LOGD("# avcC: sps_length=%" PRIu16, spsLength);

        minBytes += spsLength;
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
                "invalid size: %ld expected %ld", maxBytes, minBytes);

        if ((!track->videoSps) && (spsLength))
        {
            /* First SPS found */
            track->videoSpsSize = spsLength;
            track->videoSps = malloc(spsLength);
            MP4_RETURN_ERR_IF_FAILED((track->videoSps != NULL), -ENOMEM);
            size_t count = fread(track->videoSps, spsLength, 1, demux->file);
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                    "failed to read %u bytes from file", spsLength);
        }
        else
        {
            /* Ignore any other SPS */
            if (fseeko(demux->file, spsLength, SEEK_CUR) == -1)
            {
                MP4_LOGE("Failed to seek %u bytes forward in file", spsLength);
                return -1;
            }
        }
        boxReadBytes += spsLength;
    }

    minBytes++;
    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
            "invalid size: %ld expected %ld", maxBytes, minBytes);

    /* pps_count */
    uint8_t ppsCount;
    MP4_READ_8(demux->file, ppsCount, boxReadBytes);
    MP4_LOGD("# avcC: pps_count=%d", ppsCount);

    minBytes += 2 * ppsCount;
    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
            "invalid size: %ld expected %ld", maxBytes, minBytes);

    for (i = 0; i < ppsCount; i++)
    {
        /* pps_length */
        MP4_READ_16(demux->file, val16, boxReadBytes);
        uint16_t ppsLength = htons(val16);
        MP4_LOGD("# avcC: pps_length=%" PRIu16, ppsLength);

        minBytes += ppsLength;
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -1,
                "invalid size: %ld expected %ld", maxBytes, minBytes);

        if ((!track->videoPps) && (ppsLength))
        {
            /* First PPS found */
            track->videoPpsSize = ppsLength;
            track->videoPps = malloc(ppsLength);
            MP4_RETURN_ERR_IF_FAILED((track->videoPps != NULL), -ENOMEM);
            size_t count = fread(track->videoPps, ppsLength, 1, demux->file);
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                    "failed to read %u bytes from file", ppsLength);
        }
        else
        {
            /* Ignore any other PPS */
            if (fseeko(demux->file, ppsLength, SEEK_CUR) == -1)
            {
                MP4_LOGE("Failed to seek %d bytes forward in file", ppsLength);
                return -1;
            }
        }
        boxReadBytes += ppsLength;
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_sample_description_box(mp4_demux_t *demux,
                                                    mp4_box_item_t *parent,
                                                    off_t maxBytes,
                                                    mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;
    uint16_t val16;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stsd: version=%d", version);
    MP4_LOGD("# stsd: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t entryCount = ntohl(val32);
    MP4_LOGD("# stsd: entry_count=%" PRIu32, entryCount);

    unsigned int i;
    for (i = 0; i < entryCount; i++)
    {
        switch (track->type)
        {
            case MP4_TRACK_TYPE_VIDEO:
            {
                MP4_LOGD("# stsd: video handler type");

                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 102), -1,
                        "invalid size: %ld expected %d", maxBytes, 102);

                /* size */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t size = ntohl(val32);
                MP4_LOGD("# stsd: size=%" PRIu32, size);

                /* type */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t type = ntohl(val32);
                MP4_LOGD("# stsd: type=%c%c%c%c",
                         (char)((type >> 24) & 0xFF), (char)((type >> 16) & 0xFF),
                         (char)((type >> 8) & 0xFF), (char)(type & 0xFF));

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);

                /* reserved & data_reference_index */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint16_t dataReferenceIndex = (uint16_t)(ntohl(val32) & 0xFFFF);
                MP4_LOGD("# stsd: data_reference_index=%" PRIu16, dataReferenceIndex);

                int k;
                for (k = 0; k < 4; k++)
                {
                    /* pre_defined & reserved */
                    MP4_READ_32(demux->file, val32, boxReadBytes);
                }

                /* width & height */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                track->videoWidth = ((ntohl(val32) >> 16) & 0xFFFF);
                track->videoHeight = (ntohl(val32) & 0xFFFF);
                MP4_LOGD("# stsd: width=%" PRIu16, track->videoWidth);
                MP4_LOGD("# stsd: height=%" PRIu16, track->videoHeight);

                /* horizresolution */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                float horizresolution = (float)(ntohl(val32)) / 65536.;
                MP4_LOGD("# stsd: horizresolution=%.2f", horizresolution);

                /* vertresolution */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                float vertresolution = (float)(ntohl(val32)) / 65536.;
                MP4_LOGD("# stsd: vertresolution=%.2f", vertresolution);

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);

                /* frame_count */
                MP4_READ_16(demux->file, val16, boxReadBytes);
                uint16_t frameCount = ntohs(val16);
                MP4_LOGD("# stsd: frame_count=%" PRIu16, frameCount);

                /* compressorname */
                char compressorname[32];
                size_t count = fread(&compressorname, sizeof(compressorname), 1, demux->file);
                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                        "failed to read %zu bytes from file", sizeof(compressorname));
                boxReadBytes += sizeof(compressorname);
                MP4_LOGD("# stsd: compressorname=%s", compressorname);

                /* depth & pre_defined */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint16_t depth = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
                MP4_LOGD("# stsd: depth=%" PRIu16, depth);

                /* codec specific size */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t codecSize = ntohl(val32);
                MP4_LOGD("# stsd: codec_size=%" PRIu32, codecSize);

                /* codec specific */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t codec = ntohl(val32);
                MP4_LOGD("# stsd: codec=%c%c%c%c", (char)((codec >> 24) & 0xFF), (char)((codec >> 16) & 0xFF),
                       (char)((codec >> 8) & 0xFF), (char)(codec & 0xFF));

                if (codec == MP4_AVC_DECODER_CONFIG_BOX)
                {
                    track->videoCodec = MP4_VIDEO_CODEC_AVC;
                    off_t ret = mp4_demux_parse_avc_decoder_configuration_box(demux, maxBytes - boxReadBytes, track);
                    if (ret < 0)
                    {
                        MP4_LOGE("mp4_demux_parse_avc_decoder_configuration_box() failed (%ld)", ret);
                        return -1;
                    }
                    boxReadBytes += ret;
                }
                break;
            }
            case MP4_TRACK_TYPE_AUDIO:
            {
                MP4_LOGD("# stsd: audio handler type");

                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 44), -1,
                        "invalid size: %ld expected %d", maxBytes, 44);

                /* size */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t size = ntohl(val32);
                MP4_LOGD("# stsd: size=%" PRIu32, size);

                /* type */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t type = ntohl(val32);
                MP4_LOGD("# stsd: type=%c%c%c%c",
                         (char)((type >> 24) & 0xFF), (char)((type >> 16) & 0xFF),
                         (char)((type >> 8) & 0xFF), (char)(type & 0xFF));

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);

                /* reserved & data_reference_index */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint16_t dataReferenceIndex = (uint16_t)(ntohl(val32) & 0xFFFF);
                MP4_LOGD("# stsd: data_reference_index=%" PRIu16, dataReferenceIndex);

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                MP4_READ_32(demux->file, val32, boxReadBytes);

                /* channelcount & samplesize */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                track->audioChannelCount = ((ntohl(val32) >> 16) & 0xFFFF);
                track->audioSampleSize = (ntohl(val32) & 0xFFFF);
                MP4_LOGD("# stsd: channelcount=%" PRIu16, track->audioChannelCount);
                MP4_LOGD("# stsd: samplesize=%" PRIu16, track->audioSampleSize);

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);

                /* samplerate */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                track->audioSampleRate = ntohl(val32);
                MP4_LOGD("# stsd: samplerate=%.2f", (float)track->audioSampleRate / 65536.);
                break;
            }
            case MP4_TRACK_TYPE_HINT:
                MP4_LOGD("# stsd: hint handler type");
                break;
            case MP4_TRACK_TYPE_METADATA:
            {
                MP4_LOGD("# stsd: metadata handler type");

                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24), -1,
                        "invalid size: %ld expected %d", maxBytes, 24);

                /* size */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t size = ntohl(val32);
                MP4_LOGD("# stsd: size=%" PRIu32, size);

                /* type */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                uint32_t type = ntohl(val32);
                MP4_LOGD("# stsd: type=%c%c%c%c",
                         (char)((type >> 24) & 0xFF), (char)((type >> 16) & 0xFF),
                         (char)((type >> 8) & 0xFF), (char)(type & 0xFF));

                /* reserved */
                MP4_READ_32(demux->file, val32, boxReadBytes);
                MP4_READ_16(demux->file, val16, boxReadBytes);

                /* data_reference_index */
                MP4_READ_16(demux->file, val16, boxReadBytes);
                uint16_t dataReferenceIndex = ntohl(val16);
                MP4_LOGD("# stsd: size=%d", dataReferenceIndex);

                char str[100];
                unsigned int k;
                for (k = 0; (k < sizeof(str) - 1) && (boxReadBytes < maxBytes); k++)
                {
                    MP4_READ_8(demux->file, str[k], boxReadBytes);
                    if (str[k] == '\0')
                    {
                        break;
                    }
                }
                str[k + 1] = '\0';
                if (strlen(str) > 0)
                {
                    track->metadataContentEncoding = strdup(str);
                }
                MP4_LOGD("# stsd: content_encoding=%s", str);

                for (k = 0; (k < sizeof(str) - 1) && (boxReadBytes < maxBytes); k++)
                {
                    MP4_READ_8(demux->file, str[k], boxReadBytes);
                    if (str[k] == '\0')
                    {
                        break;
                    }
                }
                str[k + 1] = '\0';
                if (strlen(str) > 0)
                {
                    track->metadataMimeFormat = strdup(str);
                }
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
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_decoding_time_to_sample_box(mp4_demux_t *demux,
                                                         mp4_box_item_t *parent,
                                                         off_t maxBytes,
                                                         mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->timeToSampleEntries == NULL), -1,
            "time to sample table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stts: version=%d", version);
    MP4_LOGD("# stts: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->timeToSampleEntryCount = ntohl(val32);
    MP4_LOGD("# stts: entry_count=%" PRIu32, track->timeToSampleEntryCount);

    track->timeToSampleEntries = malloc(track->timeToSampleEntryCount * sizeof(mp4_time_to_sample_entry_t));
    MP4_RETURN_ERR_IF_FAILED((track->timeToSampleEntries != NULL), -ENOMEM);

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8 + track->timeToSampleEntryCount * 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8 + track->timeToSampleEntryCount * 8);

    unsigned int i;
    for (i = 0; i < track->timeToSampleEntryCount; i++)
    {
        /* sample_count */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->timeToSampleEntries[i].sampleCount = ntohl(val32);

        /* sample_delta */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->timeToSampleEntries[i].sampleDelta = ntohl(val32);
        //MP4_LOGD("# stts: sample_count=%" PRIu32 " sample_delta=%" PRIu32,
        //         track->timeToSampleEntries[i].sampleCount,
        //         track->timeToSampleEntries[i].sampleDelta);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_sync_sample_box(mp4_demux_t *demux,
                                             mp4_box_item_t *parent,
                                             off_t maxBytes,
                                             mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->syncSampleEntries == NULL), -1,
            "sync sample table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stss: version=%d", version);
    MP4_LOGD("# stss: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->syncSampleEntryCount = ntohl(val32);
    MP4_LOGD("# stss: entry_count=%" PRIu32, track->syncSampleEntryCount);

    track->syncSampleEntries = malloc(track->syncSampleEntryCount * sizeof(uint32_t));
    MP4_RETURN_ERR_IF_FAILED((track->syncSampleEntries != NULL), -ENOMEM);

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8 + track->syncSampleEntryCount * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 8 + track->syncSampleEntryCount * 4);

    unsigned int i;
    for (i = 0; i < track->syncSampleEntryCount; i++)
    {
        /* sample_number */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->syncSampleEntries[i] = ntohl(val32);
        //MP4_LOGD("# stss: sample_number=%" PRIu32, track->syncSampleEntries[i]);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_sample_size_box(mp4_demux_t *demux,
                                             mp4_box_item_t *parent,
                                             off_t maxBytes,
                                             mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->sampleSize == NULL), -1,
            "sample size table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 12), -1,
            "invalid size: %ld expected %d", maxBytes, 12);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stsz: version=%d", version);
    MP4_LOGD("# stsz: flags=%" PRIu32, flags);

    /* sample_size */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t sampleSize = ntohl(val32);
    MP4_LOGD("# stsz: sample_size=%" PRIu32, sampleSize);

    /* sample_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->sampleCount = ntohl(val32);
    MP4_LOGD("# stsz: sample_count=%" PRIu32, track->sampleCount);

    track->sampleSize = malloc(track->sampleCount * sizeof(uint32_t));
    MP4_RETURN_ERR_IF_FAILED((track->sampleSize != NULL), -ENOMEM);

    if (sampleSize == 0)
    {
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 12 + track->sampleCount * 4), -1,
                "invalid size: %ld expected %d", maxBytes, 12 + track->sampleCount * 4);

        unsigned int i;
        for (i = 0; i < track->sampleCount; i++)
        {
            /* entry_size */
            MP4_READ_32(demux->file, val32, boxReadBytes);
            track->sampleSize[i] = ntohl(val32);
            //MP4_LOGD("# stsz: entry_size=%" PRIu32, track->sampleSize[i]);
        }
    }
    else
    {
        unsigned int i;
        for (i = 0; i < track->sampleCount; i++)
        {
            track->sampleSize[i] = sampleSize;
        }
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_sample_to_chunk_box(mp4_demux_t *demux,
                                                 mp4_box_item_t *parent,
                                                 off_t maxBytes,
                                                 mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->sampleToChunkEntries == NULL), -1,
            "sample to chunk table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stsc: version=%d", version);
    MP4_LOGD("# stsc: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->sampleToChunkEntryCount = ntohl(val32);
    MP4_LOGD("# stsc: entry_count=%" PRIu32, track->sampleToChunkEntryCount);

    track->sampleToChunkEntries = malloc(track->sampleToChunkEntryCount * sizeof(mp4_sample_to_chunk_entry_t));
    MP4_RETURN_ERR_IF_FAILED((track->sampleToChunkEntries != NULL), -ENOMEM);

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8 + track->sampleToChunkEntryCount * 12), -1,
            "invalid size: %ld expected %d", maxBytes, 8 + track->sampleToChunkEntryCount * 12);

    unsigned int i;
    for (i = 0; i < track->sampleToChunkEntryCount; i++)
    {
        /* first_chunk */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->sampleToChunkEntries[i].firstChunk = ntohl(val32);
        //MP4_LOGD("# stsc: first_chunk=%" PRIu32, track->sampleToChunkEntries[i].firstChunk);

        /* samples_per_chunk */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->sampleToChunkEntries[i].samplesPerChunk = ntohl(val32);
        //MP4_LOGD("# stsc: samples_per_chunk=%" PRIu32, track->sampleToChunkEntries[i].samplesPerChunk);

        /* sample_description_index */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->sampleToChunkEntries[i].sampleDescriptionIndex = ntohl(val32);
        //MP4_LOGD("# stsc: sample_description_index=%" PRIu32,
        //         track->sampleToChunkEntries[i].sampleDescriptionIndex);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_chunk_offset_box(mp4_demux_t *demux,
                                              mp4_box_item_t *parent,
                                              off_t maxBytes,
                                              mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL), -1,
            "chunk offset table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# stco: version=%d", version);
    MP4_LOGD("# stco: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->chunkCount = ntohl(val32);
    MP4_LOGD("# stco: entry_count=%" PRIu32, track->chunkCount);

    track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
    MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8 + track->chunkCount * 4), -1,
            "invalid size: %ld expected %d", maxBytes, 8 + track->chunkCount * 4);

    unsigned int i;
    for (i = 0; i < track->chunkCount; i++)
    {
        /* chunk_offset */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->chunkOffset[i] = (uint64_t)ntohl(val32);
        //MP4_LOGD("# stco: chunk_offset=%" PRIu64, track->chunkOffset[i]);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_chunk_offset64_box(mp4_demux_t *demux,
                                                mp4_box_item_t *parent,
                                                off_t maxBytes,
                                                mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -1, "invalid track");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL), -1,
            "chunk offset table already defined");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8);

    /* version & flags */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t flags = ntohl(val32);
    uint8_t version = (flags >> 24) & 0xFF;
    flags &= ((1 << 24) - 1);
    MP4_LOGD("# co64: version=%d", version);
    MP4_LOGD("# co64: flags=%" PRIu32, flags);

    /* entry_count */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    track->chunkCount = ntohl(val32);
    MP4_LOGD("# co64: entry_count=%" PRIu32, track->chunkCount);

    track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
    MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8 + track->chunkCount * 8), -1,
            "invalid size: %ld expected %d", maxBytes, 8 + track->chunkCount * 8);

    unsigned int i;
    for (i = 0; i < track->chunkCount; i++)
    {
        /* chunk_offset */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->chunkOffset[i] = (uint64_t)ntohl(val32) << 32;
        MP4_READ_32(demux->file, val32, boxReadBytes);
        track->chunkOffset[i] |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
        //MP4_LOGD("# co64: chunk_offset=%" PRIu64, track->chunkOffset[i]);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_tag_data_box(mp4_demux_t *demux,
                                          mp4_box_item_t *parent,
                                          off_t maxBytes,
                                          mp4_track_t *track)
{
    off_t boxReadBytes = 0;
    uint32_t val32;

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((parent->parent != NULL), -1,
            "invalid parent");

    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9), -1,
            "invalid size: %ld expected %d", maxBytes, 9);

    /* version & class */
    MP4_READ_32(demux->file, val32, boxReadBytes);
    uint32_t clazz = ntohl(val32);
    uint8_t version = (clazz >> 24) & 0xFF;
    clazz &= 0xFF;
    MP4_LOGD("# data: version=%d", version);
    MP4_LOGD("# data: class=%" PRIu32, clazz);

    /* reserved */
    MP4_READ_32(demux->file, val32, boxReadBytes);

    unsigned int valueLen = maxBytes - boxReadBytes;

    if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_ARTIST)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_ARTIST] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_ARTIST] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_ARTIST], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_ARTIST][valueLen] = '\0';
        MP4_LOGD("# data: ART=%s", demux->tags[MP4_METADATA_TAG_TYPE_ARTIST]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_TITLE)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_TITLE] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_TITLE] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_TITLE], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_TITLE][valueLen] = '\0';
        MP4_LOGD("# data: nam=%s", demux->tags[MP4_METADATA_TAG_TYPE_TITLE]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_DATE)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_DATE] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_DATE] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_DATE], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_DATE][valueLen] = '\0';
        MP4_LOGD("# data: day=%s", demux->tags[MP4_METADATA_TAG_TYPE_DATE]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_COMMENT)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_COMMENT] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_COMMENT] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_COMMENT], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_COMMENT][valueLen] = '\0';
        MP4_LOGD("# data: cmt=%s", demux->tags[MP4_METADATA_TAG_TYPE_COMMENT]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_COPYRIGHT)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_COPYRIGHT] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_COPYRIGHT] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_COPYRIGHT], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_COPYRIGHT][valueLen] = '\0';
        MP4_LOGD("# data: cpy=%s", demux->tags[MP4_METADATA_TAG_TYPE_COPYRIGHT]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_MAKER)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_MAKER] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_MAKER] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_MAKER], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_MAKER][valueLen] = '\0';
        MP4_LOGD("# data: mak=%s", demux->tags[MP4_METADATA_TAG_TYPE_MAKER]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_MODEL)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_MODEL] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_MODEL] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_MODEL], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_MODEL][valueLen] = '\0';
        MP4_LOGD("# data: mod=%s", demux->tags[MP4_METADATA_TAG_TYPE_MODEL]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_VERSION)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_VERSION] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_VERSION] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_VERSION], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_VERSION][valueLen] = '\0';
        MP4_LOGD("# data: swr=%s", demux->tags[MP4_METADATA_TAG_TYPE_VERSION]);
    }
    else if ((parent->parent->box.type & 0xFFFFFF) == MP4_TAG_TYPE_ENCODER)
    {
        demux->tags[MP4_METADATA_TAG_TYPE_ENCODER] = malloc(valueLen + 1);
        MP4_RETURN_ERR_IF_FAILED((demux->tags[MP4_METADATA_TAG_TYPE_ENCODER] != NULL), -ENOMEM);
        size_t count = fread(demux->tags[MP4_METADATA_TAG_TYPE_ENCODER], valueLen, 1, demux->file);
        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                "failed to read %u bytes from file", valueLen);
        boxReadBytes += valueLen;
        demux->tags[MP4_METADATA_TAG_TYPE_ENCODER][valueLen] = '\0';
        MP4_LOGD("# data: too=%s", demux->tags[MP4_METADATA_TAG_TYPE_ENCODER]);
    }
    else if (parent->parent->box.type == MP4_TAG_TYPE_COVER)
    {
        demux->coverOffset = ftello(demux->file);
        demux->coverSize = valueLen;
        MP4_LOGD("# data: covr offset=0x%lX size=%d", demux->coverOffset, demux->coverSize);
    }

    /* skip the rest of the box */
    MP4_SKIP(demux->file, boxReadBytes, maxBytes);

    return boxReadBytes;
}


static off_t mp4_demux_parse_children(mp4_demux_t *demux,
                                      mp4_box_item_t *parent,
                                      off_t maxBytes,
                                      mp4_track_t *track)
{
    off_t parentReadBytes = 0;
    int ret = 0, lastBox = 0;
    mp4_box_item_t *prev = NULL;

    while ((!feof(demux->file)) && (!lastBox) && (parentReadBytes + 8 < maxBytes))
    {
        off_t boxReadBytes = 0, realBoxSize;
        uint32_t val32;
        mp4_box_t box;
        memset(&box, 0, sizeof(mp4_box_t));

        /* box size */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        box.size = ntohl(val32);

        /* box type */
        MP4_READ_32(demux->file, val32, boxReadBytes);
        box.type = ntohl(val32);
        MP4_LOGD("offset 0x%lX box '%c%c%c%c'", ftello(demux->file),
                 (box.type >> 24) & 0xFF, (box.type >> 16) & 0xFF,
                 (box.type >> 8) & 0xFF, box.type & 0xFF);

        if (box.size == 0)
        {
            /* box extends to end of file */
            lastBox = 1;
            realBoxSize = demux->fileSize - demux->readBytes;
        }
        else if (box.size == 1)
        {
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= parentReadBytes + 16), -1,
                    "invalid size: %ld expected %ld", maxBytes, parentReadBytes + 16);

            /* large size */
            MP4_READ_32(demux->file, val32, boxReadBytes);
            box.largesize = (uint64_t)ntohl(val32) << 32;
            MP4_READ_32(demux->file, val32, boxReadBytes);
            box.largesize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
            realBoxSize = box.largesize;
        }
        else
        {
            realBoxSize = box.size;
        }

        MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= parentReadBytes + realBoxSize), -1,
                "invalid size: %ld expected %ld", maxBytes, parentReadBytes + realBoxSize);

        /* keep the box in the tree */
        mp4_box_item_t *item = malloc(sizeof(mp4_box_item_t));
        MP4_RETURN_ERR_IF_FAILED((item != NULL), -ENOMEM);
        memset(item, 0, sizeof(mp4_box_item_t));
        memcpy(&item->box, &box, sizeof(mp4_box_t));
        item->parent = parent;
        if (prev == NULL)
        {
            parent->child = item;
        }
        else
        {
            prev->next = item;
            item->prev = prev;
        }
        prev = item;

        switch (box.type)
        {
            case MP4_UUID:
            {
                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(((unsigned)(realBoxSize - boxReadBytes) >= sizeof(box.uuid)), -1,
                        "invalid size: %ld expected %zu", realBoxSize - boxReadBytes, sizeof(box.uuid));

                /* box extended type */
                size_t count = fread(box.uuid, sizeof(box.uuid), 1, demux->file);
                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                        "failed to read %zu bytes from file", sizeof(box.uuid));
                boxReadBytes += sizeof(box.uuid);
                break;
            }
            case MP4_MOVIE_BOX:
            case MP4_USER_DATA_BOX:
            case MP4_MEDIA_BOX:
            case MP4_MEDIA_INFORMATION_BOX:
            case MP4_DATA_INFORMATION_BOX:
            case MP4_SAMPLE_TABLE_BOX:
            case MP4_ILST_BOX:
            {
                off_t _ret = mp4_demux_parse_children(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_FILE_TYPE_BOX:
            {
                off_t _ret = mp4_demux_parse_file_type_box(demux, item, realBoxSize - boxReadBytes);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_MOVIE_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_movie_header_box(demux, item, realBoxSize - boxReadBytes);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_TRACK_BOX:
            {
                /* keep the track in the list */
                mp4_track_t *tk = malloc(sizeof(mp4_track_t));
                MP4_RETURN_ERR_IF_FAILED((tk != NULL), -ENOMEM);
                memset(tk, 0, sizeof(mp4_track_t));
                tk->next = demux->track;
                if (demux->track) demux->track->prev = tk;
                demux->track = tk;
                demux->trackCount++;

                off_t _ret = mp4_demux_parse_children(demux, item, realBoxSize - boxReadBytes, tk);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_TRACK_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_track_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_TRACK_REFERENCE_BOX:
            {
                off_t _ret = mp4_demux_parse_track_reference_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_HANDLER_REFERENCE_BOX:
            {
                off_t _ret = mp4_demux_parse_handler_reference_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_MEDIA_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_media_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_VIDEO_MEDIA_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_video_media_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_SOUND_MEDIA_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_sound_media_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_HINT_MEDIA_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_hint_media_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_NULL_MEDIA_HEADER_BOX:
            {
                off_t _ret = mp4_demux_parse_null_media_header_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_SAMPLE_DESCRIPTION_BOX:
            {
                off_t _ret = mp4_demux_parse_sample_description_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_DECODING_TIME_TO_SAMPLE_BOX:
            {
                off_t _ret = mp4_demux_parse_decoding_time_to_sample_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_SYNC_SAMPLE_BOX:
            {
                off_t _ret = mp4_demux_parse_sync_sample_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_SAMPLE_SIZE_BOX:
            {
                off_t _ret = mp4_demux_parse_sample_size_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_SAMPLE_TO_CHUNK_BOX:
            {
                off_t _ret = mp4_demux_parse_sample_to_chunk_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_CHUNK_OFFSET_BOX:
            {
                off_t _ret = mp4_demux_parse_chunk_offset_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_CHUNK_OFFSET_64_BOX:
            {
                off_t _ret = mp4_demux_parse_chunk_offset64_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            case MP4_META_BOX:
            {
                if (parent->box.type == MP4_USER_DATA_BOX)
                {
                    MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((realBoxSize - boxReadBytes >= 4), -1,
                            "invalid size: %ld expected %d", realBoxSize - boxReadBytes, 4);

                    /* version & flags */
                    MP4_READ_32(demux->file, val32, boxReadBytes);
                    uint32_t flags = ntohl(val32);
                    uint8_t version = (flags >> 24) & 0xFF;
                    flags &= ((1 << 24) - 1);
                    MP4_LOGD("# meta: version=%d", version);
                    MP4_LOGD("# meta: flags=%" PRIu32, flags);

                    off_t _ret = mp4_demux_parse_children(demux, item, realBoxSize - boxReadBytes, track);
                    MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                    boxReadBytes += _ret;
                }
                else if (parent->box.type == MP4_MOVIE_BOX)
                {
                    off_t _ret = mp4_demux_parse_children(demux, item, realBoxSize - boxReadBytes, track);
                    MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                    boxReadBytes += _ret;
                }
                break;
            }
            case MP4_DATA_BOX:
            {
                off_t _ret = mp4_demux_parse_tag_data_box(demux, item, realBoxSize - boxReadBytes, track);
                MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                boxReadBytes += _ret;
                break;
            }
            default:
            {
                if (parent->box.type == MP4_ILST_BOX)
                {
                    off_t _ret = mp4_demux_parse_children(demux, item, realBoxSize - boxReadBytes, track);
                    MP4_RETURN_ERR_IF_FAILED((_ret >= 0), -1);
                    boxReadBytes += _ret;
                }
                break;
            }
        }

        /* skip the rest of the box */
        if (realBoxSize < boxReadBytes)
        {
            MP4_LOGE("Invalid box size %ld (read bytes: %ld)", realBoxSize, boxReadBytes);
            ret = -1;
            break;
        }
        if (fseeko(demux->file, realBoxSize - boxReadBytes, SEEK_CUR) == -1)
        {
            MP4_LOGE("Failed to seek %ld bytes forward in file", realBoxSize - boxReadBytes);
            ret = -1;
            break;
        }

        parentReadBytes += realBoxSize;
    }

    return (ret < 0) ? ret : parentReadBytes;
}


static int mp4_demux_build_tracks(mp4_demux_t *demux)
{
    mp4_track_t *tk = NULL, *videoTk = NULL, *metaTk = NULL, *chapTk = NULL;
    int videoTrackCount = 0, audioTrackCount = 0, hintTrackCount = 0;
    int metadataTrackCount = 0, textTrackCount = 0;

    for (tk = demux->track; tk; tk = tk->next)
    {
        unsigned int i, j, k, n;
        uint32_t lastFirstChunk = 1, lastSamplesPerChunk = 0;
        uint32_t chunkCount, sampleCount = 0, chunkIdx;
        for (i = 0; i < tk->sampleToChunkEntryCount; i++)
        {
            chunkCount = tk->sampleToChunkEntries[i].firstChunk - lastFirstChunk;
            sampleCount += chunkCount * lastSamplesPerChunk;
            lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
            lastSamplesPerChunk = tk->sampleToChunkEntries[i].samplesPerChunk;
        }
        chunkCount = tk->chunkCount - lastFirstChunk + 1;
        sampleCount += chunkCount * lastSamplesPerChunk;

        if (sampleCount != tk->sampleCount)
        {
            MP4_LOGE("Sample count mismatch: %d vs. %d", sampleCount, tk->sampleCount);
            return -1;
        }

        tk->sampleToChunk = malloc(sampleCount * sizeof(uint32_t));
        MP4_RETURN_ERR_IF_FAILED((tk->sampleToChunk != NULL), -ENOMEM);

        lastFirstChunk = 1;
        lastSamplesPerChunk = 0;
        for (i = 0, n = 0, chunkIdx = 0; i < tk->sampleToChunkEntryCount; i++)
        {
            chunkCount = tk->sampleToChunkEntries[i].firstChunk - lastFirstChunk;
            for (j = 0; j < chunkCount; j++, chunkIdx++)
            {
                for (k = 0; k < tk->sampleToChunkEntries[i].samplesPerChunk; k++, n++)
                {
                    tk->sampleToChunk[n] = chunkIdx;
                }
            }
            lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
            lastSamplesPerChunk = tk->sampleToChunkEntries[i].samplesPerChunk;
        }
        chunkCount = tk->chunkCount - lastFirstChunk + 1;
        for (j = 0; j < chunkCount; j++, chunkIdx++)
        {
            for (k = 0; k < lastSamplesPerChunk; k++, n++)
            {
                tk->sampleToChunk[n] = chunkIdx;
            }
        }

        for (i = 0, sampleCount = 0; i < tk->timeToSampleEntryCount; i++)
        {
            sampleCount += tk->timeToSampleEntries[i].sampleCount;
        }

        if (sampleCount != tk->sampleCount)
        {
            MP4_LOGE("Sample count mismatch: %d vs. %d", sampleCount, tk->sampleCount);
            return -1;
        }

        tk->sampleDecodingTime = malloc(sampleCount * sizeof(uint64_t));
        MP4_RETURN_ERR_IF_FAILED((tk->sampleDecodingTime != NULL), -ENOMEM);

        uint64_t ts = 0;
        for (i = 0, k = 0; i < tk->timeToSampleEntryCount; i++)
        {
            for (j = 0; j < tk->timeToSampleEntries[i].sampleCount; j++, k++)
            {
                tk->sampleDecodingTime[k] = ts;
                ts += tk->timeToSampleEntries[i].sampleDelta;
            }
        }

        switch (tk->type)
        {
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
        if ((tk->referenceType != 0) && (tk->referenceTrackId))
        {
            mp4_track_t *tkRef;
            int found = 0;
            for (tkRef = demux->track; tkRef; tkRef = tkRef->next)
            {
                if (tkRef->id == tk->referenceTrackId)
                {
                    found = 1;
                    break;
                }
            }
            if (found)
            {
                if ((tk->referenceType == MP4_REFERENCE_TYPE_DESCRIPTION)
                        && (tk->type == MP4_TRACK_TYPE_METADATA))
                {
                    tkRef->metadata = tk;
                    tk->ref = tkRef;
                }
                else if ((tk->referenceType == MP4_REFERENCE_TYPE_CHAPTERS)
                        && (tkRef->type == MP4_TRACK_TYPE_TEXT))
                {
                    tk->chapters = tkRef;
                    tkRef->ref = tk;
                    tkRef->type = MP4_TRACK_TYPE_CHAPTERS;
                    chapTk = tkRef;
                }
            }
        }
    }

    /* Workaround: if we have only 1 video track and 1 metadata
     * track with no track reference, link them anyway */
    if ((videoTrackCount == 1) && (metadataTrackCount == 1)
            && (audioTrackCount == 0) && (hintTrackCount == 0)
            && (videoTk) && (metaTk) && (!videoTk->metadata))
    {
        videoTk->metadata = metaTk;
        metaTk->ref = videoTk;
    }

    /* Build the chapter list */
    if (chapTk)
    {
        unsigned int i;
        for (i = 0; i < chapTk->sampleCount; i++)
        {
            unsigned int sampleSize, readBytes = 0;
            uint16_t sz;
            sampleSize = chapTk->sampleSize[i];
            fseeko(demux->file, chapTk->chunkOffset[chapTk->sampleToChunk[i]], SEEK_SET);
            MP4_READ_16(demux->file, sz, readBytes);
            sz = ntohs(sz);
            if (sz <= sampleSize - readBytes)
            {
                demux->chaptersName[demux->chaptersCount] = malloc(sz + 1);
                MP4_RETURN_ERR_IF_FAILED((demux->chaptersName[demux->chaptersCount] != NULL), -ENOMEM);
                size_t count = fread(demux->chaptersName[demux->chaptersCount], sz, 1, demux->file);
                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                        "failed to read %u bytes from file", sz);
                readBytes += sz;
                demux->chaptersName[demux->chaptersCount][sz] = '\0';
                demux->chaptersTime[demux->chaptersCount] =
                        (chapTk->sampleDecodingTime[i] * 1000000 + chapTk->timescale / 2) / chapTk->timescale;
                MP4_LOGD("chapter #%d time=%" PRIu64 " '%s'", demux->chaptersCount + 1,
                         demux->chaptersTime[demux->chaptersCount], demux->chaptersName[demux->chaptersCount]);
                demux->chaptersCount++;
            }
        }
    }

    return 0;
}


static void mp4_demux_print_children(mp4_demux_t *demux,
                                     mp4_box_item_t *parent,
                                     int level)
{
    mp4_box_item_t *item = NULL;

    for (item = parent->child; item; item = item->next)
    {
        int k;
        char spaces[101];
        for (k = 0; k < level && k < 50; k++)
        {
            spaces[k * 2] = ' ';
            spaces[k * 2 + 1] = ' ';
        }
        spaces[k * 2] = '\0';
        MP4_LOGD("%s- %c%c%c%c size %" PRIu64 "\n", spaces,
                 (char)((item->box.type >> 24) & 0xFF), (char)((item->box.type >> 16) & 0xFF),
                 (char)((item->box.type >> 8) & 0xFF), (char)(item->box.type & 0xFF),
                 (item->box.size == 1) ? item->box.largesize : item->box.size);

        if (item->child)
            mp4_demux_print_children(demux, item, level + 1);
    }
}


static void mp4_demux_free_children(mp4_demux_t *demux,
                                    mp4_box_item_t *parent)
{
    mp4_box_item_t *item = NULL, *next = NULL;

    for (item = parent->child; item; item = next)
    {
        next = item->next;
        if (item->child)
            mp4_demux_free_children(demux, item);
        free(item);
    }
}


static void mp4_demux_free_tracks(mp4_demux_t *demux)
{
    mp4_track_t *tk = NULL, *next = NULL;

    for (tk = demux->track; tk; tk = next)
    {
        next = tk->next;
        free(tk->timeToSampleEntries);
        free(tk->sampleDecodingTime);
        free(tk->sampleSize);
        free(tk->chunkOffset);
        free(tk->sampleToChunkEntries);
        free(tk->sampleToChunk);
        free(tk->videoSps);
        free(tk->videoPps);
        free(tk->metadataContentEncoding);
        free(tk->metadataMimeFormat);
        free(tk);
    }
}


mp4_demux_t* mp4_demux_open(const char *filename)
{
    int ret = 0;
    off_t retBytes;
    mp4_demux_t *demux;

    if ((!filename) || (!strlen(filename)))
    {
        MP4_LOGE("Invalid file name");
        return NULL;
    }

    demux = malloc(sizeof(mp4_demux_t));
    if (demux == NULL)
    {
        MP4_LOGE("Allocation failed");
        ret = -1;
        goto cleanup;
    }
    memset(demux, 0, sizeof(mp4_demux_t));

    demux->file = fopen(filename, "rb");
    if (demux->file == NULL)
    {
        MP4_LOGE("Failed to open file '%s'", filename);
        ret = -1;
        goto cleanup;
    }

    fseeko(demux->file, 0, SEEK_END);
    demux->fileSize = ftello(demux->file);
    fseeko(demux->file, 0, SEEK_SET);

    retBytes = mp4_demux_parse_children(demux, &demux->root, demux->fileSize, NULL);
    if (retBytes < 0)
    {
        MP4_LOGE("mp4_demux_parse_children() failed (%ld)", retBytes);
        ret = -1;
        goto cleanup;
    }
    else
    {
        demux->readBytes += retBytes;
    }

    ret = mp4_demux_build_tracks(demux);
    if (ret < 0)
    {
        MP4_LOGE("mp4_demux_build_tracks() failed (%d)", ret);
        ret = -1;
        goto cleanup;
    }

    mp4_demux_print_children(demux, &demux->root, 0);

    return demux;

cleanup:
    if (demux->file)
    {
        fclose(demux->file);
    }

    if (demux)
    {
        mp4_demux_free_children(demux, &demux->root);
        mp4_demux_free_tracks(demux);
    }

    free(demux);

    return NULL;
}


int mp4_demux_close(mp4_demux_t *demux)
{
    if (!demux)
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    if (demux->file)
    {
        fclose(demux->file);
    }

    if (demux)
    {
        mp4_demux_free_children(demux, &demux->root);
        mp4_demux_free_tracks(demux);
        unsigned int i;
        for (i = 0; i < demux->chaptersCount; i++)
        {
            free(demux->chaptersName[i]);
        }
        for (i = 0; i < MP4_METADATA_TAG_MAX; i++)
        {
            free(demux->tags[i]);
        }
    }

    free(demux);

    return 0;
}


int mp4_demux_seek(mp4_demux_t *demux,
                   uint64_t time_offset,
                   int sync)
{
    mp4_track_t *tk = NULL;

    if (!demux)
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    for (tk = demux->track; tk; tk = tk->next)
    {
        if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
            continue;
        if ((tk->type == MP4_TRACK_TYPE_METADATA) && (tk->ref))
            continue;

        int found = 0, i;
        uint64_t ts = (time_offset * tk->timescale + 500000) / 1000000;
        int start = (unsigned int)(((uint64_t)tk->sampleCount * ts
                + tk->duration - 1) / tk->duration);
        if (start < 0) start = 0;
        if ((unsigned)start >= tk->sampleCount) start = tk->sampleCount - 1;
        while (((unsigned)start < tk->sampleCount)
                && (tk->sampleDecodingTime[start] < ts))
        {
            start++;
        }
        for (i = start; i >= 0; i--)
        {
            if (tk->sampleDecodingTime[i] <= ts)
            {
                int isSync, prevSync = -1;
                isSync = mp4_demux_is_sync_sample(demux, tk, i, &prevSync);
                if ((isSync) || (!sync))
                {
                    start = i;
                    found = 1;
                    break;
                }
                else if (prevSync >= 0)
                {
                    start = prevSync;
                    found = 1;
                    break;
                }
            }
        }
        if (found)
        {
            tk->currentSample = start;
            MP4_LOGI("Seek to %" PRIu64 " -> sample #%d time %" PRIu64, time_offset, start,
                     (tk->sampleDecodingTime[start] * 1000000 + tk->timescale / 2) / tk->timescale);
            if ((tk->metadata) && ((unsigned)start < tk->metadata->sampleCount)
                    && (tk->sampleDecodingTime[start] == tk->metadata->sampleDecodingTime[start]))
            {
                tk->metadata->currentSample = start;
            }
            else
            {
                MP4_LOGW("Failed to sync metadata with ref track");
            }
        }
        else
        {
            MP4_LOGE("Unable to seek in track");
            return -1;
        }
    }

    return 0;
}


int mp4_demux_get_track_count(mp4_demux_t *demux)
{
    if (!demux)
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    return demux->trackCount;
}


int mp4_demux_get_track_info(mp4_demux_t *demux,
                             unsigned int track_idx,
                             mp4_track_info_t *track_info)
{
    mp4_track_t *tk = NULL;
    unsigned int k;

    if ((!demux) || (!track_info))
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }
    if (track_idx >= demux->trackCount)
    {
        MP4_LOGE("Invalid track index");
        return -1;
    }

    memset(track_info, 0, sizeof(mp4_track_info_t));

    for (tk = demux->track, k = 0; (tk) && (k < track_idx); tk = tk->next, k++);

    if (tk)
    {
        track_info->id = tk->id;
        track_info->type = tk->type;
        track_info->duration = (tk->duration * 1000000 + tk->timescale / 2) / tk->timescale;
        track_info->sample_count = tk->sampleCount;
        track_info->has_metadata = (tk->metadata) ? 1 : 0;
        if (tk->metadata)
        {
            track_info->metadata_content_encoding = tk->metadata->metadataContentEncoding;
            track_info->metadata_mime_format = tk->metadata->metadataMimeFormat;
        }
        else if (tk->type == MP4_TRACK_TYPE_METADATA)
        {
            track_info->metadata_content_encoding = tk->metadataContentEncoding;
            track_info->metadata_mime_format = tk->metadataMimeFormat;
        }
        if (tk->type == MP4_TRACK_TYPE_VIDEO)
        {
            track_info->video_codec = tk->videoCodec;
            track_info->video_width = tk->videoWidth;
            track_info->video_height = tk->videoHeight;
        }
        else if (tk->type == MP4_TRACK_TYPE_AUDIO)
        {
            track_info->audio_codec = tk->audioCodec;
            track_info->audio_channel_count = tk->audioChannelCount;
            track_info->audio_sample_size = tk->audioSampleSize;
            track_info->audio_sample_rate = (float)tk->audioSampleRate / 65536.;
        }
    }
    else
    {
        MP4_LOGE("Track not found");
        return -1;
    }

    return 0;
}


int mp4_demux_get_track_avc_decoder_config(mp4_demux_t *demux,
                                           unsigned int track_id,
                                           uint8_t **sps,
                                           unsigned int *sps_size,
                                           uint8_t **pps,
                                           unsigned int *pps_size)
{
    mp4_track_t *tk = NULL;
    int found = 0;

    if ((!demux) || (!sps) || (!sps_size) || (!pps) || (!pps_size))
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    for (tk = demux->track; tk; tk = tk->next)
    {
        if (tk->id == track_id)
        {
            found = 1;
            break;
        }
    }

    if (!found)
    {
        MP4_LOGE("Track not found");
        return -1;
    }

    if (tk->videoSps)
    {
        *sps = tk->videoSps;
        *sps_size = tk->videoSpsSize;
    }
    if (tk->videoPps)
    {
        *pps = tk->videoPps;
        *pps_size = tk->videoPpsSize;
    }

    return 0;
}


int mp4_demux_get_track_next_sample(mp4_demux_t *demux,
                                    unsigned int track_id,
                                    uint8_t *sample_buffer,
                                    unsigned int sample_buffer_size,
                                    uint8_t *metadata_buffer,
                                    unsigned int metadata_buffer_size,
                                    mp4_track_sample_t *track_sample)
{
    mp4_track_t *tk = NULL;
    int found = 0;

    if ((!demux) || (!track_sample))
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    memset(track_sample, 0, sizeof(mp4_track_sample_t));

    for (tk = demux->track; tk; tk = tk->next)
    {
        if (tk->id == track_id)
        {
            found = 1;
            break;
        }
    }

    if (!found)
    {
        MP4_LOGE("Track not found");
        return -1;
    }

    if (tk->currentSample < tk->sampleCount)
    {
        track_sample->sample_size = tk->sampleSize[tk->currentSample];
        if ((sample_buffer) && (tk->sampleSize[tk->currentSample] <= sample_buffer_size))
        {
            fseeko(demux->file, tk->chunkOffset[tk->sampleToChunk[tk->currentSample]], SEEK_SET);
            size_t count = fread(sample_buffer, tk->sampleSize[tk->currentSample], 1, demux->file);
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                    "failed to read %d bytes from file", tk->sampleSize[tk->currentSample]);
        }
        else if (sample_buffer)
        {
            MP4_LOGE("Buffer too small (%d bytes)", sample_buffer_size);
            return -1;
        }
        if (tk->metadata)
        {
            //TODO: check sync between metadata and ref track
            track_sample->metadata_size = tk->metadata->sampleSize[tk->currentSample];
            if ((metadata_buffer) && (tk->metadata->sampleSize[tk->currentSample] <= metadata_buffer_size))
            {
                fseeko(demux->file, tk->metadata->chunkOffset[tk->metadata->sampleToChunk[tk->currentSample]], SEEK_SET);
                size_t count = fread(metadata_buffer, tk->metadata->sampleSize[tk->currentSample], 1, demux->file);
                MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                        "failed to read %d bytes from file", tk->metadata->sampleSize[tk->currentSample]);
            }
        }
        track_sample->sample_dts = (tk->sampleDecodingTime[tk->currentSample] * 1000000 + tk->timescale / 2) / tk->timescale;
        track_sample->next_sample_dts = (tk->currentSample < tk->sampleCount - 1) ?
                (tk->sampleDecodingTime[tk->currentSample + 1] * 1000000 + tk->timescale / 2) / tk->timescale : 0;
        tk->currentSample++;
    }

    return 0;
}


int mp4_demux_get_chapters(mp4_demux_t *demux,
                           unsigned int *chaptersCount,
                           uint64_t **chaptersTime,
                           char ***chaptersName)
{
    if ((!demux) || (!chaptersCount) || (!chaptersTime) || (!chaptersName))
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    *chaptersCount = demux->chaptersCount;
    *chaptersTime = demux->chaptersTime;
    *chaptersName = demux->chaptersName;

    return 0;
}


int mp4_demux_get_metadata_tags(mp4_demux_t *demux,
                                char ***tags)
{
    if ((!demux) || (!tags))
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    *tags = demux->tags;

    return 0;
}


int mp4_demux_get_metadata_cover(mp4_demux_t *demux,
                                 uint8_t *cover_buffer,
                                 unsigned int cover_buffer_size,
                                 unsigned int *cover_size)
{
    if (!demux)
    {
        MP4_LOGE("Invalid pointer");
        return -1;
    }

    if (demux->coverSize > 0)
    {
        if (cover_size)
        {
            *cover_size = demux->coverSize;
        }
        if ((cover_buffer) && (demux->coverSize <= cover_buffer_size))
        {
            fseeko(demux->file, demux->coverOffset, SEEK_SET);
            size_t count = fread(cover_buffer, demux->coverSize, 1, demux->file);
            MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -1,
                    "failed to read %d bytes from file", demux->coverSize);
        }
        else if (cover_buffer)
        {
            MP4_LOGE("Buffer too small (%d bytes)", cover_buffer_size);
            return -1;
        }
    }
    else
    {
        *cover_size = 0;
    }

    return 0;
}
