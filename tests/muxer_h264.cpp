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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <vector>
#include <set>

#include "libmp4.h"
#include "list.h"
#include "ulog.h"


char *mdata_video_keys[] = {"com.parrot.thermal.metaversion",
			    "com.parrot.thermal.alignment",
			    "com.parrot.thermal.scalefactor"};

char *mdata_video_values[] = {"2", "0.000000,0.000000,0.000000", "1.836559"};

unsigned int mdata_video_count = SIZEOF_ARRAY(mdata_video_keys);

static char *mdata_audio_keys[] = {
	"\xA9"
	"nam\0",
	"\xA9"
	"ART\0",
	"\xA9"
	"day\0",
	"\xA9"
	"too\0",
	"\xA9"
	"cmt\0"};

#if 0
static char *mdata_audio_values[] = {"incredible machine",
				     "3 years old scientist",
				     "2019",
				     "Lavf57.83.100",
				     "just a random test video"};
#endif

unsigned int mdata_audio_count = SIZEOF_ARRAY(mdata_audio_keys);

	std::string format_2(int x)
{
	auto xStr = std::to_string(x);
	if (x < 10)
		return "0" + xStr;
	else
		return xStr;
}

int main(int argc, char **argv)
{
	int ret;
	uint64_t now = 18446744071626706816;

	if (argc < 3) {
		std::cout << "Missing param";
		return 1;
	}

	//const char *in = argv[1];
	const char *out = argv[1];
	int crash_at = atoi(argv[2]);

	unsigned int sample_buffer_size = 5 * 1024 * 1024;
	unsigned int metadata_buffer_size = 1 * 1024 * 1024;

	uint8_t *sample_buffer = new uint8_t[sample_buffer_size];
	uint8_t *metadata_buffer = new uint8_t[metadata_buffer_size];

	int ntracks = 1; /* Set the ntracks here */

	unsigned int meta_file_count;
	char **meta_file_keys = (char **) malloc(1 * sizeof(char*));
	char **meta_file_vals = (char **) malloc(1 * sizeof(char*));

	int videotrack = -1;
	int metatrack = -1;
	int audiotrack = -1;

	int current_track = -1;
	int has_more_audio = 0;
	int has_more_video = 0;

	int vs_count = 0;
	int as_count = 0;
	uint64_t step_ts = 0;
	const uint64_t increment_ts = 100000; /* 100ms */
	unsigned int cover_size, j;
	enum mp4_metadata_cover_type cover_type;

	struct mp4_track_info info, video, audio;

	std::cout << "1\n";
	struct mp4_mux *mux = NULL;
	ret = mp4_mux_open(out, 30000, now, now, &mux);
	if (ret != 0) {
		ULOG_ERRNO("mp4_mux_open", -ret);
		return 1;
	}
	
	const char *temp1 = "\251too";
	const char *temp2 = "Lavf58.65.101";
	
	mp4_mux_add_file_metadata(mux, temp1, temp2);
	std::cout << "2\n";

	int sample_count = 60;
	std::ifstream spsfile("data/bunny.mp4",std::ios::binary);
	spsfile.seekg(0x00000274, std::ios::beg);
	char *spsBuffer = (char *) malloc(23);
	uint8_t *sps_Buffers = (uint8_t *)malloc(23);
	spsfile.read(spsBuffer, 23);
	memcpy(sps_Buffers, spsBuffer, 23);

	spsfile.seekg(0, spsfile.beg);
	spsfile.seekg(0x0000028e, std::ios::beg);
	char *ppsBuffer = (char *)malloc(4);
	uint8_t *pps_Buffers = (uint8_t *)malloc(4);
	spsfile.read(ppsBuffer, 4);
	memcpy(pps_Buffers, ppsBuffer, 4);
	// params
	struct mp4_mux_track_params params;
	params.type = MP4_TRACK_TYPE_VIDEO;
	params.name = "VideoHandler";
	params.enabled = 1;
	params.in_movie = 1;
	params.in_preview = 0;
	params.timescale = 15360;
	params.creation_time = 18446744071626706816;
	params.modification_time = 18446744071626706816;
	
	// for MP4_TRACK_TYPE_VIDEO

	uint8_t *sps_temp = sps_Buffers;
	uint8_t *pps_temp = pps_Buffers;
	
	videotrack = mp4_mux_add_track(mux, &params);
	struct mp4_video_decoder_config vdc;
	vdc.width = 424;
	vdc.height = 240;
	vdc.codec = MP4_VIDEO_CODEC_AVC;
	vdc.avc.sps = sps_temp;
	vdc.avc.pps = pps_temp;
	vdc.avc.pps_size = 4;
	vdc.avc.sps_size = 23;

	mp4_mux_track_set_video_decoder_config(mux, videotrack, &vdc);
	std::cout << "3\n";

	has_more_video = sample_count > 0;
	current_track = videotrack;

	// read h264 encoded frames
	std::string prefPath = "data/outFrames/" ; //xy.h264
	std::string sufPath = ".h264"; 

	
	int i = 0;
	bool isKeyFrame;
	std::ifstream syncfile("data/SyncNumber.txt");
	std::set<int> syncNumbers;
	std::string linep;
	while (getline(syncfile, linep))
	{
		syncNumbers.insert(std::stoi(linep));
	}
	for (i = 0; i < 14316; ++i) // has_more_video
	{	
		std::cout << "frame=>" << std::to_string(i + 1) <<"\n";
		struct mp4_track_sample sample;
		struct mp4_mux_sample mux_sample;
		int lc_video = 0;
		step_ts += increment_ts;

		if (syncNumbers.find(i) != syncNumbers.end())
		{
			isKeyFrame = true;
		}
		else
		{
			isKeyFrame = false;
		}
		std::string fNoStr = format_2(i);
		std::string framePath = prefPath + std::to_string(i) + sufPath;
		std::ifstream file(framePath, std::ios::binary | std::ios::ate);
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		
		char *buffer = new char[size];
		std::cout << "alloc "<< size << " bytes\n";

		if (crash_at == i + 1) // crash at i +1 th frame
		{
			//raise(SIGSEGV);
		}

		if (file.read(buffer, size))
		{
			video.id = 1;
			mux_sample.buffer = (uint8_t *) buffer;
			mux_sample.len = size;
			mux_sample.sync = isKeyFrame ? 1 : 0;
			mux_sample.dts = 512 * i; // harcoded - dts ?DemuxAndParserState
			std::cout << "sample add start\n";
			mp4_mux_track_add_sample(mux, videotrack, &mux_sample);
			std::cout<<"sample done" << i << std::endl;
			// mp4_mux_sync(mux); // write per frame
		}

		if (!(( i+ 1) % 10 ))
		{
			//mp4_mux_sync(mux); // write per 10 frames
		}
	}
	
	std::cout << "4\n";
	free(sample_buffer);
	mp4_mux_close(mux);
}