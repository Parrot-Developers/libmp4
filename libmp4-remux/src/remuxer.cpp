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
#include "util.hpp"

#include <algorithm>
#include <iostream>


int Remuxer::linkMetadataTracks(std::string_view path)
{
	int ret = 0;
	const struct mp4_demux *demux = getDemuxer(path);
	struct mp4_track_info info;

	if (demux == nullptr)
		return -EPROTO;

	for (unsigned int i = 0; i < mTracks.size(); i++) {
		ret = mp4_demux_get_track_info(demux, i, &info);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_track_info", -ret);
			return ret;
		}
		if (!info.has_metadata)
			continue;

		auto it = std::find_if(mTracks.begin(),
				       mTracks.end(),
				       [&info](const Track &track) {
					       return track.getInfo().id ==
						      info.metadata_track_id;
				       });
		if (it == mTracks.end()) {
			ULOGE("track id=%d not found", info.metadata_track_id);
			return -ENOENT;
		}

		mTracks[i].setMetadataTrack(*it);
	}

	return ret;
}


int Remuxer::addMetadataTrackFromTrack(std::string_view path,
				       unsigned int trackIndex,
				       const struct mp4_track_info &info)
{
	int ret = 0;
	struct mp4_track_info metaTkInfo;
	const struct mp4_demux *demux = getDemuxer(path);
	struct mp4_track_info trackInfo;

	if (demux == nullptr)
		return -EPROTO;

	if (!info.has_metadata) {
		ULOGE("track has no metadata");
		return -EINVAL;
	}

	ret = mp4_demux_get_track_info(demux, trackIndex, &trackInfo);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_track_info", -ret);
		return ret;
	}

	if (!trackInfo.has_metadata) {
		ULOGE("track id=%d has no metadata track", info.id);
		return -ENOENT;
	}

	ret = mp4_demux_get_track_info(
		demux, trackInfo.metadata_track_id, &metaTkInfo);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_track_info", -ret);
		return ret;
	}

	return addTrack(path, trackIndex, metaTkInfo);
}


int Remuxer::addMetadataTrackFromTrack(std::string_view path,
				       unsigned int trackId)
{
	int ret = 0;
	struct mp4_track_info info {
	};
	const struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	ret = mp4_demux_get_track_info(demux, trackId, &info);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_track_info", -ret);
		return ret;
	}

	return addMetadataTrackFromTrack(
		path, static_cast<unsigned int>(mTracks.size()), info);
}


int Remuxer::addTrack(std::string_view path,
		      size_t trackIndex,
		      const struct mp4_track_info &info)
{
	int ret = 0;
	unsigned int count = 0;
	char **keys = nullptr;
	char **values = nullptr;
	const struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	mTracks.push_back(Track(info));

	for (size_t i = 0; i < info.sample_count; i++) {
		struct mp4_track_sample sample {
		};
		ret = mp4_demux_get_track_sample(
			demux, info.id, 1, nullptr, 0, nullptr, 0, &sample);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_track_sample", -ret);
			return ret;
		}

		mTracks[mTracks.size() - 1].addSample(sample, path);
	}
	ULOGI("%" PRIu32 " samples added in track %zu (%s) has_meta:%d",
	      info.sample_count,
	      trackIndex,
	      info.name,
	      info.has_metadata);

	ret = mp4_demux_get_track_metadata_strings(
		demux, info.id, &count, &keys, &values);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_track_metadata_strings", -ret);
		return ret;
	}

	mTracks[trackIndex].setMetadataStrings(count, keys, values);

	mTracks[trackIndex].setConfig(demux);

	return ret;
}


int Remuxer::addTrack(std::string_view path, unsigned int trackId)
{
	int ret = 0;
	struct mp4_track_info info {
	};
	const struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	ret = mp4_demux_get_track_info(demux, trackId, &info);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_track_info", -ret);
		return ret;
	}

	return addTrack(path, mTracks.size(), info);
}


int Remuxer::feedMux(struct mp4_mux &mux) const
{
	int ret = 0;

	if (mCover.getType() != MP4_METADATA_COVER_TYPE_UNKNOWN &&
	    (mCover.getBuffer() != nullptr)) {
		ULOGD("set cover");
		ret = mp4_mux_set_file_cover(&mux,
					     mCover.getType(),
					     mCover.getBuffer(),
					     mCover.getSize());
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_set_file_cover", -ret);
			return ret;
		}
	}

	for (const auto &m : mFileMetadata) {
		ret = mp4_mux_add_file_metadata(&mux, m.getKey(), m.getValue());
		if (ret < 0) {
			ULOG_ERRNO("mp4_mux_add_file_metadata", -ret);
			return ret;
		}
		ULOGD("add file metadata (%s:%s)", m.getKey(), m.getValue());
	}

	for (auto &t : mTracks) {
		ret = t.feedMux(mux);
		if (ret < 0) {
			ULOG_ERRNO("Track::feedMux", -ret);
			return ret;
		}
	}

	return ret;
}


int Remuxer::dump(std::string_view path) const
{
	int ret = 0;

	for (const auto &t : mTracks) {
		ret = t.dump(path);
		if (ret < 0) {
			ULOG_ERRNO("Track::dump", -ret);
			return ret;
		}
	}

	return ret;
}


int Remuxer::fillFromPath(std::string_view path)
{
	int ret = 0;

	ret = fillFileMetadata(path);
	if (ret < 0) {
		ULOG_ERRNO("fillFileMetadata", -ret);
		return ret;
	}

	ret = fillCover(path);
	if (ret < 0) {
		ULOG_ERRNO("fillCover", -ret);
		return ret;
	}

	ret = addTracks(path);
	if (ret < 0) {
		ULOG_ERRNO("addTracks", -ret);
		return ret;
	}

	ret = linkMetadataTracks(path);
	if (ret < 0) {
		ULOG_ERRNO("linkMetadataTracks", -ret);
		return ret;
	}

	return ret;
}


int Remuxer::fillFileMetadata(std::string_view path)
{
	int ret = 0;
	unsigned int count = 0;
	char **keys = nullptr;
	char **values = nullptr;
	struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	mFileMetadata.clear();

	ret = mp4_demux_get_metadata_strings(demux, &count, &keys, &values);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_metadata_strings", -ret);
		return ret;
	}

	for (unsigned int i = 0; i < count; i++) {
		MetadataPair m(keys[i], values[i]);
		mFileMetadata.push_back(m);
	}

	return ret;
}


static inline bool contains(std::string_view s, std::string_view pattern)
{
	return std::search(
		       s.begin(),
		       s.end(),
		       pattern.begin(),
		       pattern.end(),
		       [](unsigned char a, unsigned char b) {
			       return std::tolower(a) == std::tolower(b);
		       }) != s.end();
}


static inline bool isJpeg(std::string_view path)
{
	return contains(path, ".jpg") || contains(path, ".jpeg");
}


static inline bool isPng(std::string_view path)
{
	return contains(path, ".png") || contains(path, ".PNG");
}


int Remuxer::setCover(enum mp4_metadata_cover_type coverType,
		      const std::vector<uint8_t> &cover)
{
	mCover = Cover(coverType, cover);

	if (mFileMetadata.empty()) {
		/* add metadata to ensure thumbnail is written */
		MetadataPair m("covr", "thumbnail");
		mFileMetadata.push_back(m);
	}

	return 0;
}


int Remuxer::fillCoverPicture(std::string_view path)
{
	std::ifstream file(path.data(), std::ios::binary);
	std::vector<uint8_t> picture;
	enum mp4_metadata_cover_type coverType =
		MP4_METADATA_COVER_TYPE_UNKNOWN;

	if (isJpeg(path))
		coverType = MP4_METADATA_COVER_TYPE_JPEG;
	else if (isPng(path))
		coverType = MP4_METADATA_COVER_TYPE_PNG;
	else
		return -ENOSYS;

	picture = std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
				       std::istreambuf_iterator<char>());
	return setCover(coverType, picture);
}


int Remuxer::fillCoverMp4(std::string_view path)
{
	int ret = 0;
	unsigned int coverSize = 0;
	unsigned int sample_buffer_size = 0;
	enum mp4_metadata_cover_type coverType;
	const struct mp4_demux *demux = getDemuxer(path);

	if (demux == nullptr)
		return -EPROTO;

	ret = mp4_demux_get_metadata_cover(
		demux, nullptr, sample_buffer_size, &coverSize, &coverType);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);
		return ret;
	}

	std::vector<uint8_t> buffer(coverSize);
	ret = mp4_demux_get_metadata_cover(
		demux,
		buffer.data(),
		static_cast<unsigned int>(buffer.size()),
		&coverSize,
		&coverType);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);
		return ret;
	}

	return setCover(coverType, buffer);
}


int Remuxer::fillCover(std::string_view path)
{
	if (contains(path, ".mp4") || contains(path, ".MP4"))
		return fillCoverMp4(path);
	else if (isJpeg(path) || isPng(path))
		return fillCoverPicture(path);
	return -ENOSYS;
}


int Remuxer::extractThumbnail(std::string_view path) const
{
	int ret = 0;
	std::ofstream oFile;

	if (mCover.getType() == MP4_METADATA_COVER_TYPE_UNKNOWN)
		return -ENOENT;

	oFile.open(path.data(), std::ios::binary);
	if (!oFile) {
		ULOG_ERRNO("open ('%s')", -errno, path.data());
		return -EPROTO;
	}

	oFile.write(reinterpret_cast<const char *>(mCover.getBuffer()),
		    mCover.getSize());

	return ret;
}


int Remuxer::newChapterTrack()
{
	struct mp4_track_info info {
	};
	int ret = 0;

	info.type = MP4_TRACK_TYPE_CHAPTERS;
	info.name = "chapters";
	info.enabled = false;
	info.in_movie = false;
	info.in_preview = false;
	info.timescale = DEFAULT_MP4_TIMESCALE;
	info.creation_time = 0;
	info.modification_time = 0;

	mChapterTrackIndex = newTrack(info);

	for (int i = 0; i < mChapterTrackIndex; i++) {
		ret = linkMetadataTracks(i, mChapterTrackIndex);
		if (ret < 0) {
			ULOG_ERRNO("linkMetadataTracks", -ret);
			return ret;
		}
	}

	return ret;
}


int Remuxer::addChapter(uint64_t timestamp, std::string_view name)
{
	int ret = 0;
	if (mChapterTrackIndex < 0) {
		ret = newChapterTrack();
		if (ret < 0) {
			ULOG_ERRNO("newChapterTrack", -ret);
			return ret;
		}
	}

	return mTracks[mChapterTrackIndex].addChapter(timestamp, name);
}


struct mp4_demux *Remuxer::getDemuxer(std::string_view path)
{
	for (const auto &d : mDemuxers) {
		if (d->getPath() == path)
			return d->getDemuxer();
	}

	try {
		mDemuxers.push_back(std::make_unique<Demuxer>(path));
		return mDemuxers[mDemuxers.size() - 1]->getDemuxer();
	} catch (const std::bad_alloc &) {
		ULOG_ERRNO("Demuxer allocation failed", ENOMEM);
		return nullptr;
	}
}
