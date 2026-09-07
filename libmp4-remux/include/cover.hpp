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

#include <string>
#include <vector>


class Cover {
public:
	Cover() = default;

	Cover(enum mp4_metadata_cover_type coverType,
	      const std::vector<uint8_t> &cover) :
			mType(coverType),
			mBuffer(cover){};

	Cover(enum mp4_metadata_cover_type coverType,
	      const uint8_t *cover,
	      size_t coverSize) :
			mType(coverType),
			mBuffer(cover, cover + coverSize){};

	~Cover() = default;

	enum mp4_metadata_cover_type getType() const
	{
		return mType;
	}

	const uint8_t *getBuffer() const
	{
		return mBuffer.data();
	}

	size_t getSize() const
	{
		return mBuffer.size();
	}

private:
	enum mp4_metadata_cover_type mType = MP4_METADATA_COVER_TYPE_UNKNOWN;
	std::vector<uint8_t> mBuffer = {};
};
