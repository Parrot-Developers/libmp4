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
#include <ctime>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <string>

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

std::string format_3(int x)
{
	auto xStr = std::to_string(x);
	if (x < 10)
		return "00" + xStr;
	else if (x < 100)
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
	
	// params
	struct mp4_mux_track_params params;
	params.type = MP4_TRACK_TYPE_VIDEO;
	params.name = "VideoHandler";
	params.enabled = 1;
	params.in_movie = 1;
	params.in_preview = 0;
	params.timescale = 30000;
	params.creation_time = 18446744071626706816;
	params.modification_time = 18446744071626706816;
	
	// for MP4_TRACK_TYPE_VIDEO
	//uint8_t tempsps = 39;
	//uint8_t *sps_temp = &tempsps;
	videotrack = mp4_mux_add_track(mux, &params);
	struct mp4_video_decoder_config vdc; // TODO: discusssion pending
	vdc.width = 3619;
	vdc.height = 3619;
	vdc.codec = MP4_VIDEO_CODEC_MP4V;
	//vdc.avc.sps = sps_temp;
	//*(vdc.avc.pps) = 40;
	//vdc.avc.pps_size = 4;
	//vdc.avc.sps_size = 35;

	mp4_mux_track_set_video_decoder_config(mux, videotrack, &vdc);
	std::cout << "3\n";
	struct mp4_mux_track_params params2;
	params2 = params;
	params2.type = MP4_TRACK_TYPE_METADATA;
	params2.name = "APRA METADATA";

	metatrack = mp4_mux_add_track(mux, &params2);
	char *content_encoding = "base64";
	char *mime_format = "video/mp4";

	mp4_mux_track_set_metadata_mime_type(
		mux,
		metatrack,
		content_encoding,
		mime_format);

	/* Add track reference */
	if (metatrack > 0) {
		ULOGI("metatrack = %d, videotrack = %d", metatrack, videotrack);
		ret = mp4_mux_add_ref_to_track(mux, metatrack, videotrack);
		if (ret != 0) {
			ULOG_ERRNO("mp4_mux_add_ref_to_track", -ret);
		}

	}

	has_more_video = sample_count > 0;
	current_track = videotrack;

	// read jpeg encoded frames
	std::string prefPath = "../data/streamer_frames/out_"; //xxx.jpg
	std::string sufPath = ".jpg"; 

	
	int i;
	bool isKeyFrame;
	struct mp4_mux_sample mux_sample;
	std::string framePath = "../data/bigjpeg.jpg";
	std::ifstream file(framePath, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	char *buffer = new char[size];
	file.read(buffer, size);
	for (i = 1; i < 3000; ++i) // has_more_video
	{	
		std::cout << "frame=>" << std::to_string(i) <<"\n";

		int lc_video = 0;
		step_ts += increment_ts;

		video.id = 1;
		mux_sample.buffer = (uint8_t *) buffer;
		mux_sample.len = size;
		mux_sample.sync = 0; //isKeyFrame ? 1 : 0;
		mux_sample.dts = 512 * (i - 1); // harcoded - dts ?
		std::cout << "sample add start\n";
		mp4_mux_track_add_sample(mux, videotrack, &mux_sample);
		std::cout<<"sample done" << i << std::endl;
		if (metatrack != -1) {
			std::string temp = "frame_" + std::to_string(i);
			mux_sample.buffer = (uint8_t *) temp.data();
			mux_sample.len = 6 + std::to_string(i).length();
			mp4_mux_track_add_sample(
				mux, metatrack, &mux_sample);
		}
		// mp4_mux_sync(mux); // write per frame
		
		if (!(i % 100 ))
		{
			std::cout << "====SYNC=====\n";
			mp4_mux_sync(mux); // write per 100 frames
		}

		if (crash_at == i) // crash at i th frame
		{
			//raise(SIGSEGV);
		}
	}
	
	std::cout << "4\n";
	free(sample_buffer);
	mp4_mux_close(mux);
}