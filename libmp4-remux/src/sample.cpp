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

#include "sample.hpp"
#include "util.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>


static std::vector<std::byte> convertAvccToAnnexB(const std::byte *data,
						  size_t size)
{
	std::vector<std::byte> output;
	size_t offset = 0;

	while (offset + 4 <= size) {
		uint32_t naluSize =
			(std::to_integer<uint32_t>(data[offset]) << 24) |
			(std::to_integer<uint32_t>(data[offset + 1]) << 16) |
			(std::to_integer<uint32_t>(data[offset + 2]) << 8) |
			(std::to_integer<uint32_t>(data[offset + 3]));

		offset += 4;

		if (offset + naluSize > size)
			return {};

		output.insert(output.end(), START_CODE.begin(), START_CODE.end());
		output.insert(
			output.end(), data + offset, data + offset + naluSize);

		offset += naluSize;
	}

	return output;
}


int Sample::dump(std::ifstream &iFile, std::ofstream &oFile) const
{
	iFile.seekg(mInfo.offset);
	if (!iFile) {
		ULOG_ERRNO("file::seekg", -errno);
		return -EPROTO;
	}

	std::vector<char> fileBuffer(mInfo.size);
	iFile.read(fileBuffer.data(), mInfo.size);

	std::vector<std::byte> buffer(fileBuffer.size());
	charBufferToBytesBuffer(fileBuffer, buffer);

	std::vector<std::byte> bufferAnnexB =
		convertAvccToAnnexB(buffer.data(), buffer.size());

	std::vector<char> outputBuffer(bufferAnnexB.size());
	byteBufferToCharBuffer(bufferAnnexB, outputBuffer);
	oFile.write(outputBuffer.data(), outputBuffer.size());

	return 0;
}


int Sample::feedMux(const struct mp4_mux &mux,
		    int trackHandle,
		    std::ifstream &file) const
{
	struct mp4_mux_sample muxSample {
	};

	file.seekg(mInfo.offset);
	if (!file) {
		ULOG_ERRNO("file::seekg", -errno);
		return -EPROTO;
	}

	std::vector<char> buffer(mInfo.size);
	if (!file.read(buffer.data(), mInfo.size)) {
		ULOG_ERRNO("file::read", -errno);
		return -EPROTO;
	}

	muxSample.buffer = reinterpret_cast<const uint8_t *>(buffer.data());
	muxSample.len = mInfo.size;
	muxSample.sync = mInfo.sync;
	muxSample.dts = mInfo.dts;

	return mp4_mux_track_add_sample(&mux, trackHandle, &muxSample);
}
