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

#include <fstream>
#include <string>
#include <vector>

#include "util.hpp"


class Sample {
	MP4_DISABLE_COPY(Sample);

public:
	Sample(const struct mp4_track_sample &trackSample,
	       std::string_view path) :
			mInfo(trackSample),
			mPath(path){};

	Sample(Sample &&) noexcept = default;

	Sample &operator=(Sample &&) noexcept = default;

	~Sample() = default;

	int dump(std::ifstream &iFile, std::ofstream &oFile) const;

	struct mp4_track_sample getInfo() const
	{
		return mInfo;
	}

	int feedMux(const struct mp4_mux &mux,
		    int trackHandle,
		    std::ifstream &file) const;

	std::string_view getPath() const
	{
		return mPath;
	}

private:
	struct mp4_track_sample mInfo {
	};
	std::string mPath = "";
};


class MuxSample {
	MP4_DISABLE_COPY(MuxSample);

public:
	explicit MuxSample(const struct mp4_mux_sample &s) :
			mBuf(s.buffer, s.buffer + s.len),
			mMuxSample({
				.buffer = mBuf.data(),
				.len = s.len,
				.sync = s.sync,
				.dts = s.dts,
			})
	{
	}

	~MuxSample() = default;

	int feedMux(const struct mp4_mux &mux, int trackHandle) const
	{
		int ret = 0;

		ret = mp4_mux_track_add_sample(&mux, trackHandle, &mMuxSample);
		if (ret < 0)
			ULOG_ERRNO("mp4_mux_track_add_sample", -ret);

		return ret;
	}

	struct mp4_mux_sample getInfo() const
	{
		return mMuxSample;
	}

private:
	std::vector<uint8_t> mBuf{};
	struct mp4_mux_sample mMuxSample {
	};
};
