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

#include "remuxer.hpp"
#include "track.hpp"

#include <arpa/inet.h>
#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>


int Track::feedSamples(const struct mp4_mux &mux, int trackHandle) const
{
	int ret = 0;
	std::string path = "";
	std::ifstream file;

	for (const auto &s : mSamples) {
		if (path == "" || path != s.getPath()) {
			path = s.getPath();
			file.open(path, std::ios::binary);
		}

		if (!file) {
			ULOG_ERRNO("open ('%s')", -errno, path.c_str());
			return -EPROTO;
		}

		ret = s.feedMux(mux, trackHandle, file);
		if (ret < 0) {
			ULOG_ERRNO("feedMux", -ret);
			return ret;
		}
	}

	for (const auto &s : mMuxSamples) {
		ret = s->feedMux(mux, trackHandle);
		if (ret < 0) {
			ULOG_ERRNO("feedMux", -ret);
			return ret;
		}
	}

	return ret;
}


static std::vector<uint8_t> generateChapterSample(std::string_view chapter_str)
{
	if (chapter_str.empty())
		return {};

	auto chap_len = static_cast<uint16_t>(chapter_str.size());
	uint16_t val16 = htons(chap_len);

	std::vector<uint8_t> buf(sizeof(val16) + chap_len, 0);

	std::memcpy(buf.data(), &val16, sizeof(val16));

	std::memcpy(buf.data() + sizeof(val16), chapter_str.data(), chap_len);

	return buf;
}


int Track::addChapter(uint64_t timestamp, std::string_view name)
{
	int ret = 0;
	std::vector<uint8_t> buffer;
	int64_t dts = 0;
	struct mp4_mux_sample sample {
	};

	buffer = generateChapterSample(name);
	if (buffer.empty()) {
		ULOG_ERRNO("generateChapterSample", -ret);
		return ret;
	}

	dts = mp4_convert_timescale(
		timestamp * 10000000, 1000000, DEFAULT_MP4_TIMESCALE);

	sample = {
		.buffer = buffer.data(),
		.len = buffer.size(),
		.sync = 1,
		.dts = dts,
	};

	try {
		mMuxSamples.push_back(std::make_unique<MuxSample>(sample));
	} catch (const std::bad_alloc &) {
		ULOG_ERRNO("MuxSample allocation failed", ENOMEM);
		ret = -ENOMEM;
	}

	return ret;
}


void Track::setMetadataTrack(Track &metadata)
{
	metadata.mReferenceTrackHandles.push_back(this);

	metadata.mMetadataContentEncoding =
		mInfo.metadata_content_encoding
			? mInfo.metadata_content_encoding
			: "";
	metadata.mMetadataMimeFormat =
		mInfo.metadata_mime_format ? mInfo.metadata_mime_format : "";
}


int Track::setConfig(const struct mp4_demux *demux)
{
	int ret = 0;

	switch (mInfo.type) {
	case MP4_TRACK_TYPE_VIDEO:
		ret = mp4_demux_get_track_video_decoder_config(
			demux, mInfo.id, &mVdc);
		if (ret < 0)
			ULOG_ERRNO("mp4_demux_get_track_video_decoder_config",
				   -ret);
		break;
	case MP4_TRACK_TYPE_AUDIO: {
		unsigned int asc_size;
		uint8_t *asc;
		ret = mp4_demux_get_track_audio_specific_config(
			demux, mInfo.id, &asc, &asc_size);
		if (ret < 0)
			ULOG_ERRNO("mp4_demux_get_track_audio_specific_config",
				   -ret);
		mAudioSpecificConfig =
			std::vector<uint8_t>(asc, asc + asc_size);
		break;
	}
	default:
		return 0;
	}

	return ret;
}


static void writeAvccNalu(std::ofstream &out, const uint8_t *data, size_t size)
{
	out.write(reinterpret_cast<const char *>(START_CODE.data()),
		  START_CODE.size());
	out.write(reinterpret_cast<const char *>(data), size);
}


static inline void dumpPsData(const uint8_t *data, size_t dataLen)
{
	for (size_t i = 0; i < dataLen; ++i) {
		std::cout << std::setw(2) << std::setfill('0') << std::hex
			  << (int)data[i] << " ";
	}
	std::cout << std::endl;
	std::cout << std::dec;
}


int Track::dumpPs() const
{
	std::cout << mInfo.name << ":" << std::endl;

	if (mInfo.type == MP4_TRACK_TYPE_AUDIO) {
		std::cout << "ASC: ";
		dumpPsData(mAudioSpecificConfig.data(),
			   mAudioSpecificConfig.size());

		return 0;
	} else if (mInfo.type != MP4_TRACK_TYPE_VIDEO) {
		return 0;
	}

	std::cout << "width: " << mVdc.width << std::endl;
	std::cout << "height: " << mVdc.height << std::endl;

	if (mVdc.codec == MP4_VIDEO_CODEC_AVC) {
		std::cout << "SPS: ";
		dumpPsData(mVdc.avc.c_sps, mVdc.avc.sps_size);
		std::cout << "PPS: ";
		dumpPsData(mVdc.avc.c_pps, mVdc.avc.pps_size);
	} else if (mVdc.codec == MP4_VIDEO_CODEC_HEVC) {
		std::cout << "VPS: ";
		dumpPsData(mVdc.hevc.c_vps, mVdc.hevc.vps_size);
		std::cout << "SPS: ";
		dumpPsData(mVdc.hevc.c_sps, mVdc.hevc.sps_size);
		std::cout << "PPS: ";
		dumpPsData(mVdc.hevc.c_pps, mVdc.hevc.pps_size);
	}

	return 0;
}


int Track::dump(std::string_view path) const
{
	int ret = 0;
	std::ofstream oFile;
	std::ifstream iFile;
	std::string iPath = "";

	oFile.open(path.data(), std::ios::binary);
	if (!oFile) {
		ULOG_ERRNO("open ('%s')", -errno, path.data());
		return -EPROTO;
	}

	if (mVdc.codec == MP4_VIDEO_CODEC_AVC) {
		writeAvccNalu(oFile, mVdc.avc.c_sps, mVdc.avc.sps_size);
		writeAvccNalu(oFile, mVdc.avc.c_pps, mVdc.avc.pps_size);
	} else if (mVdc.codec == MP4_VIDEO_CODEC_HEVC) {
		writeAvccNalu(oFile, mVdc.hevc.vps, mVdc.hevc.vps_size);
		writeAvccNalu(oFile, mVdc.hevc.sps, mVdc.hevc.sps_size);
		writeAvccNalu(oFile, mVdc.hevc.pps, mVdc.hevc.pps_size);
	} else
		return 0;

	for (const auto &s : mSamples) {
		if (iPath == "" || iPath != s.getPath()) {
			iPath = s.getPath();
			iFile.open(iPath, std::ios::binary);
		}

		if (!iFile) {
			ULOG_ERRNO("open ('%s')", -errno, path.data());
			return -EPROTO;
		}

		ret = s.dump(iFile, oFile);
		if (ret < 0) {
			ULOG_ERRNO("dump", -ret);
			return ret;
		}
	}

	return ret;
}


int Track::feedTrackSpecificConfigMux(const struct mp4_mux &mux,
				      int trackHandle) const
{
	int ret = 0;

	switch (mInfo.type) {
	case MP4_TRACK_TYPE_VIDEO:
		ret = mp4_mux_track_set_video_decoder_config(
			&mux, trackHandle, &mVdc);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_track_set_video_decoder_config",
				   -ret);
			return ret;
		}
		break;
	case MP4_TRACK_TYPE_AUDIO:
		ret = mp4_mux_track_set_audio_specific_config(
			&mux,
			trackHandle,
			mAudioSpecificConfig.data(),
			mAudioSpecificConfig.size(),
			mInfo.audio_channel_count,
			mInfo.audio_sample_size,
			mInfo.audio_sample_rate);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_track_set_audio_specific_config",
				   -ret);
			return ret;
		}
		break;
	case MP4_TRACK_TYPE_METADATA:
		ret = mp4_mux_track_set_metadata_mime_type(
			&mux,
			trackHandle,
			mInfo.content_encoding,
			mInfo.mime_format);
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_track_set_metadata_mime_type",
				   -ret);
			return ret;
		}

		for (const auto &ref : mReferenceTrackHandles) {
			ret = mp4_mux_add_ref_to_track(
				&mux, trackHandle, ref->mIndex);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_add_ref_to_track", -ret);
				return ret;
			}
		}
		break;
	case MP4_TRACK_TYPE_CHAPTERS:
		for (const auto &ref : mReferenceTrackHandles) {
			ret = mp4_mux_add_ref_to_track(
				&mux, ref->mIndex, trackHandle);
			if (ret < 0) {
				ULOG_ERRNO("mp4_mux_add_ref_to_track", -ret);
				return ret;
			}
		}
		break;
	default:
		break;
	}

	return ret;
}


int Track::feedMux(struct mp4_mux &mux) const
{
	int ret = 0;

	if (mInfo.type != MP4_TRACK_TYPE_VIDEO &&
	    mInfo.type != MP4_TRACK_TYPE_AUDIO &&
	    mInfo.type != MP4_TRACK_TYPE_METADATA &&
	    mInfo.type != MP4_TRACK_TYPE_CHAPTERS)
		return 0;

	struct mp4_mux_track_params params {
		params.type = mInfo.type, params.name = mInfo.name,
		params.enabled = mInfo.enabled,
		params.in_movie = mInfo.in_movie,
		params.in_preview = mInfo.in_preview,
		params.timescale = mInfo.timescale * mTimescaleFactor,
		params.creation_time = mInfo.creation_time,
		params.modification_time = mInfo.modification_time,
	};

	if (!mName.empty())
		params.name = mName.c_str();

	int trackHandle = mp4_mux_add_track(&mux, &params);
	if (trackHandle < 0) {
		ULOG_ERRNO("mp4_mux_add_track", -trackHandle);
		return trackHandle;
	}
	ULOGI("add track %d (%s)", trackHandle, params.name);

	for (const auto &m : mTrackMetadata) {
		ret = mp4_mux_add_track_metadata(
			&mux, trackHandle, m.getKey(), m.getValue());
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_add_track_metadata", -ret);
			return ret;
		}
		ULOGD("add track metadata (%s:%s)", m.getKey(), m.getValue());
	}

	ret = feedTrackSpecificConfigMux(mux, trackHandle);
	if (ret < 0) {
		ULOG_ERRNO("feedTrackSpecificConfigMux", -ret);
		return ret;
	}

	ret = feedSamples(mux, trackHandle);
	if (ret < 0) {
		ULOG_ERRNO("feedSamples", -ret);
		return ret;
	}

	return ret;
}
