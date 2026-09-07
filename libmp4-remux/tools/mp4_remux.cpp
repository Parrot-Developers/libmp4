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

#include <array>
#include <iostream>
#include <string>


std::string expandHomePath(std::string_view path)
{
	std::string home;

	if (path.empty() || path[0] != '~')
		return path.data();

	const char *cHome = std::getenv("HOME");
#ifdef _WIN32
	if (!cHome)
		home = std::getenv("USERPROFILE");
#endif

	if (!cHome)
		return "";

	home = cHome;

	if (path.size() == 1)
		return home;
	else if (path[1] == '/')
		return home + path.substr(1).data();
	else
		return path.data();
}


static int split(std::string_view input, std::string &out1, std::string &out2)
{
	size_t pos = input.find(':');
	if (pos == std::string::npos) {
		return -EINVAL;
	}

	out1 = input.substr(0, pos);
	out2 = input.substr(pos + 1);

	return 0;
}

static int parseInt(std::string_view input, int &out)
{
	try {
		size_t pos = 0;
		out = std::stoi(std::string(input), &pos);
		if (pos != input.size())
			return -EINVAL;
	} catch (const std::exception &) {
		return -EINVAL;
	}

	return 0;
}


static int
splitPathArg(std::string_view input, std::string &path, std::string &arg)
{
	std::string pathTmp;
	int ret = split(input, pathTmp, arg);
	if (ret < 0)
		return ret;

	path = expandHomePath(pathTmp);

	return 0;
}


static int splitPathArg(std::string_view input,
			std::string &path,
			std::string &arg1,
			std::string &arg2)
{
	std::string tmpStr;
	std::string pathTmp;
	int ret = split(input, pathTmp, tmpStr);
	if (ret < 0)
		return ret;

	path = expandHomePath(pathTmp);

	return split(tmpStr, arg1, arg2);
}


static bool fill(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	std::string path = expandHomePath(arg);

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.fillFromPath(expandHomePath(arg));
	if (ret < 0) {
		ULOG_ERRNO("fillFromPath", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool fillFileMetadata(Remuxer &remuxer,

			     std::string_view arg)
{
	int ret = 0;
	std::string path = expandHomePath(arg);

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.fillFileMetadata(path);
	if (ret < 0) {
		ULOG_ERRNO("fillFileMetadata", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool cover(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	std::string path = expandHomePath(arg);

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.fillCover(path);
	if (ret < 0) {
		ULOG_ERRNO("fillCover", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool linkMetadata(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::string path;
	std::string trackId1;
	std::string trackId2;
	int trackRef = 0;
	int trackMeta = 0;

	ret = split(arg, trackId1, trackId2);
	if (ret < 0) {
		ULOG_ERRNO("split", -ret);
		return false;
	}

	ret = parseInt(trackId1, trackRef);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	ret = parseInt(trackId2, trackMeta);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << path << arg << std::endl;
	ret = remuxer.linkMetadataTracks(trackRef, trackMeta);
	if (ret < 0) {
		ULOG_ERRNO("linkMetadataTracks", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool renameTrack(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::string path;
	std::string trackId1;
	std::string name;
	int index = 0;

	ret = split(arg, trackId1, name);
	if (ret < 0) {
		ULOG_ERRNO("split", -ret);
		return false;
	}

	ret = parseInt(trackId1, index);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << path << arg << std::endl;
	ret = remuxer.renameTrack(index, name);
	if (ret < 0) {
		ULOG_ERRNO("renameTrack", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool multiplyTrackTimescale(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::string path;
	std::string trackId1;
	std::string timescaleFactor;
	int index = 0;
	int factor = 0;

	ret = split(arg, trackId1, timescaleFactor);
	if (ret < 0) {
		ULOG_ERRNO("split", -ret);
		return false;
	}

	ret = parseInt(trackId1, index);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	ret = parseInt(timescaleFactor, factor);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << path << arg << std::endl;
	ret = remuxer.setTrackTimescaleFactor(index, factor);
	if (ret < 0) {
		ULOG_ERRNO("setTrackTimescaleFactor", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool addAllTracks(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	std::string path = expandHomePath(arg);

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.addTracks(path);
	if (ret < 0) {
		ULOG_ERRNO("addTracks", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool addTrack(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::string path;
	std::string trackId;
	int index = 0;

	ret = splitPathArg(arg, path, trackId);
	if (ret < 0) {
		ULOG_ERRNO("splitPathArg", -ret);
		return false;
	}

	ret = parseInt(trackId, index);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << path
		  << " trackId: " << trackId << std::endl;
	ret = remuxer.addTrack(path, index);
	if (ret < 0) {
		ULOG_ERRNO("addTrack", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool removeTrack(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	int index = 0;

	ret = parseInt(arg, index);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << " trackId: " << index << std::endl;
	ret = remuxer.removeTrack(index);
	if (ret < 0) {
		ULOG_ERRNO("removeTrack", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool endTime(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	int tsMs = 0;

	ret = parseInt(arg, tsMs);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.truncate(tsMs);
	if (ret < 0) {
		ULOG_ERRNO("truncate", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool startTime(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	int tsMs = 0;

	ret = parseInt(arg, tsMs);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	ret = remuxer.startAtTs(tsMs);
	if (ret < 0) {
		ULOG_ERRNO("startAtTs", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool addChapter(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;
	std::string chapterName;
	std::string chapterTime;
	int timeSec = 0;

	ret = split(arg, chapterName, chapterTime);
	if (ret < 0) {
		ULOG_ERRNO("split", -ret);
		return false;
	}

	ret = parseInt(chapterTime, timeSec);
	if (ret < 0) {
		ULOG_ERRNO("parseInt", -ret);
		return false;
	}

	std::cout << "-> " << __func__ << ": \"" << chapterName
		  << "\" at time: " << (timeSec / 60) << "\""
		  << (timeSec % 60) << "\'" << std::endl;
	ret = remuxer.addChapter(timeSec, chapterName);
	if (ret < 0) {
		ULOG_ERRNO("addChapter", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool sanitize(Remuxer &remuxer, [[maybe_unused]] std::string_view arg)
{
	int ret = 0;

	std::cout << "-> " << __func__ << std::endl;
	ret = remuxer.sanitize();
	if (ret < 0) {
		ULOG_ERRNO("sanitize", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


/* 0 lets mp4_mux_open() apply its own default file permissions */
static constexpr mode_t OUTPUT_FILEMODE = 0;

/* Size of the pre-allocated mp4 tables area; large enough for the movies
 * this tool typically handles without growing the file needlessly */
static constexpr size_t OUTPUT_TABLES_SIZE_MBYTES = 2;


static bool output(Remuxer &remuxer, std::string_view arg)
{
	struct mp4_mux *mux = nullptr;
	int ret = 0;

	std::cout << "-> " << __func__ << ": " << arg << std::endl;
	struct mp4_mux_config config = {
		.filename = arg.data(),
		.filemode = OUTPUT_FILEMODE,
		.timescale = DEFAULT_MP4_TIMESCALE,
		.creation_time = 0,
		.modification_time = 0,
		.tables_size_mbytes = OUTPUT_TABLES_SIZE_MBYTES,
		.recovery = {},
	};

	ret = mp4_mux_open(&config, &mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_open", -ret);
		goto out;
	}

	ret = remuxer.feedMux(*mux);
	if (ret < 0) {
		ULOG_ERRNO("feedMux", -ret);
		mp4_mux_close(mux);
		goto out;
	}

	ret = mp4_mux_close(mux);
	if (ret < 0) {
		ULOG_ERRNO("mp4_mux_close", -ret);
		goto out;
	}
out:
	return (ret == 0);
}


static bool thumbnail(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::cout << "-> " << __func__ << ": " << arg << std::endl;

	ret = remuxer.extractThumbnail(arg);
	if (ret < 0) {
		ULOG_ERRNO("extractThumbnail", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool dumpPs(Remuxer &remuxer, [[maybe_unused]] std::string_view arg)
{
	int ret = 0;

	std::cout << "-> " << __func__ << std::endl;
	ret = remuxer.dumpPs();
	if (ret < 0) {
		ULOG_ERRNO("dumpPs", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool dump(Remuxer &remuxer, std::string_view arg)
{
	int ret = 0;

	std::cout << "-> " << __func__ << ": " << arg << std::endl;

	ret = remuxer.dump(expandHomePath(arg));
	if (ret < 0) {
		ULOG_ERRNO("dump", -ret);
		goto out;
	}

out:
	return (ret == 0);
}


static bool usage([[maybe_unused]] Remuxer &remuxer, std::string_view arg)
{
	std::cout << std::endl;
	std::cout << arg << " - Parrot Drones video remuxer tool" << std::endl;
	std::cout << "Copyright (c) 2018 Parrot Drones SAS" << std::endl;
	std::cout << std::endl;

	std::cout << "Usage:" << std::endl;
	std::cout << "  " << arg << " [options] --output    <file>"
		  << std::endl;
	std::cout << "  " << arg << " [options] --thumbnail <file>"
		  << std::endl;
	std::cout << "  " << arg << " [options] --dump      <file>"
		  << std::endl;
	std::cout << "  " << arg << " [options] --dump-ps" << std::endl;
	std::cout << std::endl;

	std::cout << "Getting help:" << std::endl;
	std::cout << "  -h  | --help                              ";
	std::cout << "Print this message" << std::endl;
	std::cout << std::endl;

	std::cout << "Options:" << std::endl;
	std::cout << "  -f  | --fill <input>                      ";
	std::cout << "fill the mp4 from the input" << std::endl;
	std::cout << "      | --file-metadata <input>             ";
	std::cout << "fill the file metadata from input" << std::endl;
	std::cout << "  -c  | --cover <input>                     ";
	std::cout
		<< "fill the cover from input (input can be jpg, png or mp4 - "
		   "if mp4 the thumbnail is extracted from the metadata)"
		<< std::endl;
	std::cout << "      | --add-all-tracks <input>            ";
	std::cout << "fill all the tracks from input" << std::endl;
	std::cout << "  -t  | --add-track <input>:<index>         ";
	std::cout << "fill the index track from input "
		     "(index is the track's native id in <input>)"
		  << std::endl;
	std::cout << "      | --remove-track <index>              ";
	std::cout << "remove the track at index "
		     "(index is the position among tracks already added)"
		  << std::endl;
	std::cout << "  -l  | --link-metadata <vt>:<mt>           ";
	std::cout << "associate mt as metadata of track vt "
		     "(vt/mt are positions among tracks already added)"
		  << std::endl;
	std::cout << "      | --rename-track <t>:<name>           ";
	std::cout << "rename the track t "
		     "(t is the position among tracks already added)"
		  << std::endl;
	std::cout << "      | --multiply-track-timescale <t>:<f>  ";
	std::cout << "multiply the timescale of track t by f "
		     "(t is the position among tracks already added)"
		  << std::endl;
	std::cout << "      | --end-time <tsMs>                  ";
	std::cout << "make the mp4 end at ts (ms) "
		     "(cannot make the video longer)"
		  << std::endl;
	std::cout << "      | --start-time <tsMs>                ";
	std::cout << "make the mp4 start at ts (ms)" << std::endl;
	std::cout << "      | --add-chapter <name>:<ts_s>         ";
	std::cout << "add a chapter called name at ts_s seconds" << std::endl;
	std::cout << "  -s  | --sanitize                          ";
	std::cout << "fix various issues in the mp4 file" << std::endl;
	std::cout << std::endl;

	std::cout << "Output options:" << std::endl;
	std::cout << "  -o  | --output <output>                   ";
	std::cout << "output the file in MP4 format" << std::endl;
	std::cout << "      | --thumbnail <output>                ";
	std::cout << "extract the thumbnail of the mp4" << std::endl;
	std::cout << "  -d  | --dump <output>                     ";
	std::cout << "output the h264/h265 stream" << std::endl;
	std::cout << "      | --dump-ps                           ";
	std::cout << "dump the ps" << std::endl;
	std::cout << std::endl;

	std::cout << "Notes:" << std::endl;
	std::cout << "  Options are executed from left to right." << std::endl;
	std::cout << std::endl;

	return true;
}


struct Options {
	std::string shortOpt = "";
	std::string longOpt = "";

	bool match(std::string_view arg) const
	{
		return (shortOpt == arg || longOpt == arg);
	}
};


struct mapper {
	Options option;
	std::function<bool(Remuxer &remuxer, std::string_view arg)> operation;
	bool has_arg = false;
	bool quit = false;
};


static const std::array<mapper, 18> s_commands{{
	{{"-h", "--help"}, &usage, false, true},
	{{"-f", "--fill"}, &fill, true, false},
	{{"", "--file-metadata"}, &fillFileMetadata, true, false},
	{{"-c", "--cover"}, &cover, true, false},
	{{"", "--add-all-tracks"}, &addAllTracks, true, false},
	{{"-t", "--add-track"}, &addTrack, true, false},
	{{"", "--remove-track"}, &removeTrack, true, false},
	{{"-l", "--link-metadata"}, &linkMetadata, true, false},
	{{"", "--rename-track"}, &renameTrack, true, false},
	{{"", "--multiply-track-timescale"},
	 &multiplyTrackTimescale,
	 true,
	 false},
	{{"", "--end-time"}, &endTime, true, false},
	{{"", "--start-time"}, &startTime, true, false},
	{{"", "--add-chapter"}, &addChapter, true, false},
	{{"-s", "--sanitize"}, &sanitize, false, false},
	{{"-o", "--output"}, &output, true, true},
	{{"", "--thumbnail"}, &thumbnail, true, true},
	{{"", "--dump"}, &dump, true, true},
	{{"", "--dump-ps"}, &dumpPs, false, true},
}};


static int checkArgs(int argc, char *argv[])
{
	int endingCount = 0;
	int i = 1;

	if (argc < 2)
		return -EINVAL;

	while (i < argc) {
		bool found = false;
		const std::string arg = argv[i];
		int increment = 2;

		for (const auto &c : s_commands) {
			if (!c.option.match(arg))
				continue;

			found = true;

			if (!c.has_arg) {
				increment = 1;
			} else if (!(i < argc - 1)) {
				ULOGE("invalid number of argument");
				return -EINVAL;
			}

			if (c.quit)
				endingCount++;

			if (c.option.shortOpt == "-h")
				return 1;
		}
		if (!found) {
			ULOGE("invalid option '%s'", arg.data());
			return -EINVAL;
		}

		i += increment;
	}

	if (endingCount != 1) {
		ULOGE("invalid output option count (%d)", endingCount);
		return -EINVAL;
	}

	return 0;
}


int main(int argc, char *argv[])
{
	Remuxer remuxer;
	int i = 1;

	int err = checkArgs(argc, argv);
	if (err != 0) {
		usage(remuxer, argv[0]);
		return 0;
	}

	while (i < argc) {
		int increment = 2;
		const std::string arg = argv[i];
		std::string arg2 = argv[(i < argc - 1) ? i + 1 : i];
		for (const auto &c : s_commands) {
			if (!c.option.match(arg))
				continue;
			if (!c.has_arg) {
				increment = 1;
				arg2 = argv[0];
			}
			if (!c.operation(remuxer, arg2))
				return 1;
			if (c.quit)
				return 0;
		}
		i += increment;
	}

	return 0;
}
