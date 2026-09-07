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

#pragma once

#include <libmp4.h>
#include <ulog.h>

#include "metadata.hpp"
#include "sample.hpp"
#include "util.hpp"

#include <string>
#include <vector>

class Track {
	MP4_DISABLE_COPY(Track);

public:
	Track() = default;

	explicit Track(const struct mp4_track_info &info) : mInfo(info){};

	Track(Track &&) noexcept = default;

	Track &operator=(Track &&) noexcept = default;

	~Track() = default;

	int dump(std::string_view path) const;

	void truncate(uint64_t tsMs)
	{
		while (!mSamples.empty()) {
			uint64_t curr_ts = mp4_sample_time_to_usec(
				mSamples[mSamples.size() - 1].getInfo().dts,
				mInfo.timescale);

			if (curr_ts <= (tsMs * 1000))
				break;

			mSamples.pop_back();
		}
	}

	void startAtTs(uint64_t tsMs)
	{
		size_t indexIdr = 0;
		for (size_t i = 0; i < mSamples.size(); i++) {
			uint64_t curr_ts = mp4_sample_time_to_usec(
				mSamples[i].getInfo().dts, mInfo.timescale);

			if (mSamples[i].getInfo().sync)
				indexIdr = i;

			if (curr_ts >= (tsMs * 1000))
				break;
		}

		mSamples.erase(mSamples.begin(), mSamples.begin() + indexIdr);
	}

	void setMetadataTrack(Track &metadata);

	void addSample(const struct mp4_track_sample &trackSample,
		       std::string_view path)
	{
		mSamples.emplace_back(trackSample, path);
	}

	void setMetadataStrings(unsigned int count, char **keys, char **values)
	{
		for (unsigned int i = 0; i < count; i++)
			mTrackMetadata.emplace_back(keys[i], values[i]);
	}

	int setConfig(const struct mp4_demux *demux);

	int feedSamples(const struct mp4_mux &mux, int trackHandle) const;

	int feedMux(struct mp4_mux &mux) const;

	int addChapter(uint64_t timestamp, std::string_view name);

	struct mp4_track_info getInfo() const
	{
		return mInfo;
	}

	void setIndex(unsigned int index)
	{
		mIndex = index;
	}

	void removeFramesUntilIdr()
	{
		size_t i = 0;
		for (i = 0; i < mSamples.size(); i++) {
			if (mSamples[i].getInfo().sync)
				break;
		}

		mSamples.erase(mSamples.begin(), mSamples.begin() + i);
	}

	void rename(std::string_view name)
	{
		mName = name;
	}

	void setTimescaleFactor(uint32_t timescaleFactor)
	{
		mTimescaleFactor = timescaleFactor;
	}

	int dumpPs() const;


private:
	int feedTrackSpecificConfigMux(const struct mp4_mux &mux,
				       int trackHandle) const;

	std::string mName = "";
	uint32_t mTimescaleFactor = 1;
	struct mp4_track_info mInfo {
	};
	unsigned int mIndex = 0;
	struct mp4_video_decoder_config mVdc {
	};

	std::vector<Track *> mReferenceTrackHandles{};
	std::string mMetadataContentEncoding = "";
	std::string mMetadataMimeFormat = "";

	std::vector<uint8_t> mAudioSpecificConfig{};

	std::vector<Sample> mSamples{};
	std::vector<MetadataPair> mTrackMetadata{};
	std::vector<std::unique_ptr<MuxSample>> mMuxSamples{};
};
