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

#include "cover.hpp"
#include "demux.hpp"
#include "metadata.hpp"
#include "track.hpp"
#include "util.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <vector>


constexpr uint32_t DEFAULT_MP4_TIMESCALE = 90000;


class TrackVector {
	MP4_DISABLE_COPY(TrackVector);

public:
	TrackVector() = default;

	~TrackVector() = default;

	void push_back(Track &&t)
	{
		mTracks.push_back(std::move(t));
		mTracks[mTracks.size() - 1].setIndex(
			static_cast<unsigned int>(mTracks.size()));
	}

	int remove(unsigned int index)
	{
		unsigned int newIndex = index;

		if (index >= mTracks.size())
			return -EINVAL;

		mTracks.erase(mTracks.begin() + index);

		for (size_t i = index; i < mTracks.size(); i++) {
			mTracks[i].setIndex(newIndex);
			newIndex++;
		}

		return 0;
	}

	Track &operator[](size_t index)
	{
		return mTracks[index];
	}

	size_t size() const
	{
		return mTracks.size();
	}

	auto begin()
	{
		return mTracks.begin();
	}

	auto end()
	{
		return mTracks.end();
	}

	auto begin() const
	{
		return mTracks.begin();
	}

	auto end() const
	{
		return mTracks.end();
	}

private:
	std::vector<Track> mTracks;
};


class Remuxer {
public:
	Remuxer() = default;

	~Remuxer() = default;

	int fillFromPath(std::string_view path);

	int truncate(uint64_t tsMs)
	{
		for (auto &t : mTracks)
			t.truncate(tsMs);

		return 0;
	}

	int startAtTs(uint64_t tsMs)
	{
		for (auto &t : mTracks)
			t.startAtTs(tsMs);

		return 0;
	}

	int dump(std::string_view path) const;

	/* Note: trackRef/trackMeta (and the index/t parameters of
	 * removeTrack()/renameTrack()/setTrackTimescaleFactor() below) are
	 * positions in the tracks already added to this Remuxer, unlike
	 * addTrack()'s trackId which refers to the source file's native
	 * track id. */
	int linkMetadataTracks(unsigned int trackRef, unsigned int trackMeta)
	{
		if (trackRef >= mTracks.size())
			return -ENOENT;

		mTracks[trackRef].setMetadataTrack(mTracks[trackMeta]);
		return 0;
	}

	int fillFileMetadata(std::string_view path);

	int setCover(enum mp4_metadata_cover_type coverType,
		     const std::vector<uint8_t> &cover);

	int fillCoverPicture(std::string_view path);

	int fillCoverMp4(std::string_view path);

	int fillCover(std::string_view path);

	/* Note: trackId is the track's native id in the source file at
	 * `path` (as returned by mp4_demux_get_track_info()), NOT a
	 * position in the tracks already added to this Remuxer. */
	int addTrack(std::string_view path, unsigned int trackId);

	int addTracks(std::string_view path)
	{
		return addTracks(path,
				 [](struct mp4_track_info &) { return true; });
	}

	int addChapter(uint64_t timestamp, std::string_view name);

	int feedMux(struct mp4_mux &mux) const;

	int removeTrack(unsigned int index)
	{
		return mTracks.remove(index);
	}

	int renameTrack(unsigned int index, std::string_view name)
	{
		if (index >= mTracks.size())
			return -ENOENT;

		mTracks[index].rename(name);
		return 0;
	}

	int setTrackTimescaleFactor(unsigned int index,
				    uint32_t timescaleFactor)
	{
		if (index >= mTracks.size())
			return -ENOENT;

		if (timescaleFactor == 0)
			return -EINVAL;

		mTracks[index].setTimescaleFactor(timescaleFactor);
		return 0;
	}

	int sanitize()
	{
		for (auto &t : mTracks)
			t.removeFramesUntilIdr();

		return 0;
	}

	int extractThumbnail(std::string_view path) const;

	int dumpPs() const
	{
		int count = 0;
		/* Dump video tracks first */
		for (auto &t : mTracks) {
			if (t.getInfo().type != MP4_TRACK_TYPE_VIDEO)
				continue;
			if (count > 0)
				std::cout << std::endl;

			t.dumpPs();
			count++;
		}

		for (auto &t : mTracks) {
			if (t.getInfo().type != MP4_TRACK_TYPE_AUDIO)
				continue;
			if (count > 0)
				std::cout << std::endl;

			t.dumpPs();
			count++;
		}

		return 0;
	}

private:
	int linkMetadataTracks(std::string_view path);

	int addMetadataTrackFromTrack(std::string_view path,
				      unsigned int trackIndex,
				      const struct mp4_track_info &info);

	int addMetadataTrackFromTrack(std::string_view path,
				      unsigned int trackId);

	int addTrack(std::string_view path,
		     size_t trackIndex,
		     const struct mp4_track_info &info);

	template <typename FilterFunc>
	int addTracks(std::string_view path, const FilterFunc &&filter);

	int newTrack(const struct mp4_track_info &info)
	{
		mTracks.push_back(Track(info));

		return static_cast<int>(mTracks.size() - 1);
	}

	int newChapterTrack();

	struct mp4_demux *getDemuxer(std::string_view path);

	TrackVector mTracks{};
	Cover mCover{};
	std::vector<MetadataPair> mFileMetadata{};
	std::vector<std::unique_ptr<Demuxer>> mDemuxers{};
	int mChapterTrackIndex = -1;
};


template <typename FilterFunc>
inline int Remuxer::addTracks(std::string_view path, const FilterFunc &&filter)
{
	int ret = 0;
	int count = 0;
	struct mp4_track_info info {
	};
	const struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	count = mp4_demux_get_track_count(demux);
	if (count < 0) {
		ULOG_ERRNO("mp4_demux_get_track_count", -count);
		return count;
	}

	for (unsigned int i = 0; i < static_cast<unsigned int>(count); i++) {
		ret = mp4_demux_get_track_info(demux, i, &info);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_track_info", -ret);
			return ret;
		}

		if (!filter(info))
			continue;

		ret = addTrack(path, i, info);
		if (ret < 0) {
			ULOG_ERRNO("addTrack", -ret);
			return ret;
		}
	}

	return 0;
}
