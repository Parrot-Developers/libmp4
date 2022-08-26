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

#ifndef _FILE_OFFSET_BITS
#	define _FILE_OFFSET_BITS 64
#endif

#include <fstream>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#define ULOG_TAG mp4_demux_test
#include "libmp4.h"
#include "list.h"
#include "ulog.h"

#define DATE_SIZE 26


/* Enable to write the cover to a file */
#define WRITE_COVER 0

/* Enable to log all frames */
#define LOG_FRAMES 1


static void print_info(struct mp4_demux *demux)
{
	struct mp4_media_info info;
	int ret;

	ret = mp4_demux_get_media_info(demux, &info);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_media_info", -ret);
		return;
	}

	char creation_time_str[DATE_SIZE + 1];
	char modification_time_str[DATE_SIZE + 1];
	/*time_local_format(info.creation_time,
			  0,
			  TIME_FMT_LONG,
			  creation_time_str,
			  DATE_SIZE);
	time_local_format(info.modification_time,
			  0,
			  TIME_FMT_LONG,
			  modification_time_str,
			  DATE_SIZE);*/

	printf("Media\n");
	unsigned int hrs =
		(unsigned int)((info.duration + 500000) / 1000000 / 60 / 60);
	unsigned int min =
		(unsigned int)((info.duration + 500000) / 1000000 / 60 -
			       hrs * 60);
	unsigned int sec = (unsigned int)((info.duration + 500000) / 1000000 -
					  hrs * 60 * 60 - min * 60);
	printf("  duration: %02d:%02d:%02d\n", hrs, min, sec);
	printf("  creation time: %s\n", creation_time_str);
	printf("  modification time: %s\n", modification_time_str);
	printf("\n");
}

std::string format_2(int x)
{
	auto xStr = std::to_string(x);
	if (x < 10)
		return "0" + xStr;
	else
		return xStr;
}

static void print_tracks(struct mp4_demux *demux)
{
	struct mp4_track_info tk;
	int i, count, ret;
	uint32_t duration_usec;

	count = mp4_demux_get_track_count(demux);
	if (count < 0) {
		ULOG_ERRNO("mp4_demux_get_track_count", -count);
		return;
	}

	for (i = 0; i < count; i++) {
		ret = mp4_demux_get_track_info(demux, i, &tk);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_track_info", -ret);
			continue;
		}

		char creation_time_str[DATE_SIZE + 1];
		char modification_time_str[DATE_SIZE + 1];
		/*time_local_format(tk.creation_time,
				  0,
				  TIME_FMT_LONG,
				  creation_time_str,
				  DATE_SIZE);
		time_local_format(tk.modification_time,
				  0,
				  TIME_FMT_LONG,
				  modification_time_str,
				  DATE_SIZE);*/

		printf("Track #%d ID=%d\n", i, tk.id);
		printf("  type: %s\n", mp4_track_type_str(tk.type));
		printf("  name: %s\n", tk.name);
		printf("  enabled: %d\n", tk.enabled);
		printf("  in_movie: %d\n", tk.in_movie);
		printf("  in_preview: %d\n", tk.in_preview);
		switch (tk.type) {
		case MP4_TRACK_TYPE_VIDEO:
			printf("  codec: %s\n",
			       mp4_video_codec_str(tk.video_codec));
			printf("  dimensions=%" PRIu32 "x%" PRIu32 "\n",
			       tk.video_width,
			       tk.video_height);
			break;
		case MP4_TRACK_TYPE_AUDIO:
			printf("  codec: %s\n",
			       mp4_audio_codec_str(tk.audio_codec));
			printf("  channels: %" PRIu32 "\n",
			       tk.audio_channel_count);
			printf("  samples: %" PRIu32 "bit @ %.2fkHz\n",
			       tk.audio_sample_size,
			       tk.audio_sample_rate / 1000.);
			break;
		case MP4_TRACK_TYPE_METADATA:
			printf("  content encoding: %s\n", tk.content_encoding);
			printf("  mime format: %s\n", tk.mime_format);
			break;
		default:
			break;
		}
		if (tk.has_metadata) {
			printf("  metadata: present\n");
			printf("  metadata content encoding: %s\n",
			       tk.metadata_content_encoding);
			printf("  metadata mime format: %s\n",
			       tk.metadata_mime_format);
		}
		duration_usec =
			mp4_sample_time_to_usec(tk.duration, tk.timescale);
		unsigned int hrs = (unsigned int)((duration_usec + 500000) /
						  1000000 / 60 / 60);
		unsigned int min =
			(unsigned int)((duration_usec + 500000) / 1000000 / 60 -
				       hrs * 60);
		unsigned int sec =
			(unsigned int)((duration_usec + 500000) / 1000000 -
				       hrs * 60 * 60 - min * 60);
		printf("  duration: %02d:%02d:%02d\n", hrs, min, sec);
		printf("  creation time: %s\n", creation_time_str);
		printf("  modification time: %s\n", modification_time_str);
		printf("  timescale: %" PRIu32 "\n", tk.timescale);

		unsigned int meta_count = 0;
		char **keys = NULL;
		char **values = NULL;
		ret = mp4_demux_get_track_metadata_strings(
			demux, tk.id, &meta_count, &keys, &values);
		if ((ret == 0) && (meta_count > 0)) {
			printf("  static metadata:\n");
			unsigned int j;
			for (j = 0; j < meta_count; j++) {
				if ((keys[j]) && (values[j])) {
					printf("    %s: %s\n",
					       keys[j],
					       values[j]);
				}
			}
		}

		printf("\n");
	}
}


static void print_metadata(struct mp4_demux *demux)
{
	int ret;
	unsigned int count = 0;
	char **keys = NULL;
	char **values = NULL;

	ret = mp4_demux_get_metadata_strings(demux, &count, &keys, &values);
	if (ret < 0)
		ULOG_ERRNO("mp4_demux_get_metadata_strings", -ret);

	if (count > 0) {
		printf("Metadata\n");
		unsigned int i;
		for (i = 0; i < count; i++) {
			if ((keys[i]) && (values[i]))
				printf("  %s: %s\n", keys[i], values[i]);
		}
		printf("\n");
	}

	uint8_t *cover_buffer = NULL;
	unsigned int cover_buffer_size = 0, cover_size = 0;
	enum mp4_metadata_cover_type type;
	ret = mp4_demux_get_metadata_cover(
		demux, cover_buffer, cover_buffer_size, &cover_size, &type);
	if (ret < 0)
		ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);

	if (cover_size > 0) {
		cover_buffer_size = cover_size;
		cover_buffer = (uint8_t *)malloc(cover_buffer_size);
		if (!cover_buffer)
			return;

		ret = mp4_demux_get_metadata_cover(demux,
						   cover_buffer,
						   cover_buffer_size,
						   &cover_size,
						   &type);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);
		} else {
			printf("Cover present (%s)\n\n",
			       mp4_metadata_cover_type_str(type));
#if WRITE_COVER
			FILE *fCover = fopen("cover.jpg", "wb");
			if (fCover) {
				fwrite(cover_buffer, cover_size, 1, fCover);
				fclose(fCover);
			}
#endif /* WRITE_COVER */
		}

		free(cover_buffer);
	}
}


static void print_chapters(struct mp4_demux *demux)
{
	int ret;
	unsigned int chapters_count = 0, i;
	uint64_t *chapters_time = NULL;
	char **chapters_name = NULL;

	ret = mp4_demux_get_chapters(
		demux, &chapters_count, &chapters_time, &chapters_name);
	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_get_chapters", -ret);
		return;
	}

	if (chapters_count == 0)
		return;

	printf("Chapters\n");
	for (i = 0; i < chapters_count; i++) {
		unsigned int hrs = (unsigned int)((chapters_time[i] + 500000) /
						  1000000 / 60 / 60);
		unsigned int min = (unsigned int)((chapters_time[i] + 500000) /
							  1000000 / 60 -
						  hrs * 60);
		unsigned int sec =
			(unsigned int)((chapters_time[i] + 500000) / 1000000 -
				       hrs * 60 * 60 - min * 60);
		printf("  chapter #%d time=%02d:%02d:%02d '%s'\n",
		       i + 1,
		       hrs,
		       min,
		       sec,
		       chapters_name[i]);
	}
	printf("\n");
}


static void print_frames(struct mp4_demux *demux)
{
 	unsigned int track_id = 1;
	struct mp4_video_decoder_config *vdc =
		(mp4_video_decoder_config *)malloc(
			sizeof(mp4_video_decoder_config));
	struct mp4_track_info tk;
	struct mp4_track_sample sample;
	int i, count, ret, found = 0;
	unsigned int id;

	count = mp4_demux_get_track_count(demux);
	if (count < 0) {
		ULOG_ERRNO("mp4_demux_get_track_count", -count);
		return;
	}

	for (i = 0; i < count; i++) {
		ret = mp4_demux_get_track_info(demux, i, &tk);
		if ((ret == 0) && (tk.type == MP4_TRACK_TYPE_VIDEO)) {
			id = tk.id;
			found = 1;
			break;
		}
	}
	printf("%d", found);
	if (!found)
		return;

	i = 0;
	char *filename1 = "data/SyncNumber1.txt";
	FILE *fSync = fopen(filename1, "w");
	std::string fsps = "data/sps_size1.txt";
	std::string fpps = "data/pps_size1.txt";
	std::ofstream sps_file;
	std::ofstream pps_file;
	sps_file.open(fsps, std::ios::binary | std::ios::ate | std::ios::app);
	
	pps_file.open(fpps, std::ios::binary | std::ios::ate | std::ios::app);
	do
	{
		ret = mp4_demux_get_track_sample(
			demux, id, 1, NULL, 0, NULL, 0, &sample);

		if (ret < 0 || sample.size == 0) {
			printf("sample size is zero %d", sample.size);
			ULOG_ERRNO("mp4_demux_get_track_sample", -ret);
			i++;
			break;
		}

		printf("size=>%d\n", sample.size);
		printf("Frame #%d size=%06" PRIu32 " metadata_size=%" PRIu32
		       " dts=%" PRIu64 " next_dts=%" PRIu64 " sync=%d\n",
		       i,
		       sample.size,
		       sample.metadata_size,
		       sample.dts,
		       sample.next_dts,
		       sample.sync);

		if (sample.sync) {
			fprintf(fSync, "%d\n", i);
		}
		ret = mp4_demux_get_track_video_decoder_config(
			demux, track_id, vdc);
		uint8_t *sps_buffer = (uint8_t*)malloc(sizeof(vdc->avc.sps_size));
		uint8_t *pps_buffer = (uint8_t *)malloc(sizeof(vdc->avc.pps_size));
		memcpy(sps_buffer, vdc->avc.sps, vdc->avc.sps_size);
		memcpy(pps_buffer, vdc->avc.pps, vdc->avc.pps_size);
		if (vdc->avc.sps_size) {
			sps_file << sps_buffer;
			sps_file << "\n";
		}
		if (vdc->avc.pps_size) {
			pps_file << pps_buffer;
			pps_file << "\n";
		}
		i++;

		printf("track sample %d\n", i);
		
	}
	while (sample.size);
	fclose(fSync);
	sps_file.close();
	pps_file.close();
	printf("done");
	printf("\n");
}

static void write_frames(struct mp4_demux *demux)
{
	struct mp4_track_info tk;
	struct mp4_track_sample sample;
	int i, count, ret, found = 0;
	unsigned int id;
	const uint8_t *sps1 = (uint8_t*)malloc(sizeof(22));
	unsigned int sps_size1=22;
	const uint8_t *pps1 = (uint8_t *)malloc(sizeof(4));
	unsigned int pps_size1=4;
	uint8_t *avcc1 = (uint8_t *)malloc(sizeof(uint8_t *));
	unsigned int *avcc_size1 = (unsigned int*)malloc(sizeof(uint8_t *));
	mp4_generate_avc_decoder_config(sps1,sps_size1,pps1, pps_size1,	avcc1,avcc_size1);
	count = mp4_demux_get_track_count(demux);
	if (count < 0) {
		ULOG_ERRNO("mp4_demux_get_track_count", -count);
		return;
	}

	for (i = 0; i < count; i++) {
		ret = mp4_demux_get_track_info(demux, i, &tk);
		if ((ret == 0) && (tk.type == MP4_TRACK_TYPE_VIDEO)) {
			id = tk.id;
			found = 1;
			break;
		}
	}
	unsigned int bbSize = 1024 * 1024 * 14;
	uint8_t *bigBuffer = (uint8_t *)malloc(bbSize);
	i = 0;
	do {
		ret = mp4_demux_get_track_sample(
			demux, id, 1, bigBuffer, bbSize, NULL, 0, &sample);
		printf("size=>%d\n", sample.size);
		uint8_t *buffer = new uint8_t[sample.size];
		memcpy(buffer, bigBuffer, sample.size);
		std::string fNoStr = format_2(i);
		std::string outPath = "data/outFrames1/" + std::to_string(i) ;
		std::string extension = ".h264";
		std::string framePath = outPath + extension;
		std::cout << framePath << "\n";
		std::ofstream file(framePath, std::ios::binary | std::ios::ate);
		file.write(reinterpret_cast<char *>(buffer), sample.size);
		file.close();
		i++;
		delete buffer;
	} while (sample.size);
	free(bigBuffer);
	printf("done");
	printf("\n");
}

static void print_ith_frames(struct mp4_demux *demux)
{
	struct mp4_track_info tk;
	struct mp4_track_sample sample;
	int i, count, ret, found = 0;
	unsigned int id;

	count = mp4_demux_get_track_count(demux);
	if (count < 0) {
		ULOG_ERRNO("mp4_demux_get_track_count", -count);
		return;
	}

	for (i = 0; i < count; i++) {
		ret = mp4_demux_get_track_info(demux, i, &tk);
		if ((ret == 0) && (tk.type == MP4_TRACK_TYPE_VIDEO)) {
			id = tk.id;
			found = 1;
			break;
		}
	}
	printf("%d", found);
	if (!found)
		return;

	i = 0;
	do {
		ret = mp4_demux_get_track_sample(
			demux, id, 1, NULL, 0, NULL, 0, &sample);
		if (ret < 0 || sample.size == 0) {
			printf("sample size is zero %d", sample.size);
			ULOG_ERRNO("mp4_demux_get_track_sample", -ret);
			i++;
			break;
		}

		printf("size=>%d\n", sample.size);
		printf("Frame #%d size=%06" PRIu32 " metadata_size=%" PRIu32
		       " dts=%" PRIu64 " next_dts=%" PRIu64 " sync=%d\n",
		       i,
		       sample.size,
		       sample.metadata_size,
		       sample.dts,
		       sample.next_dts,
		       sample.sync);
		i++;
		printf("track sample %d\n", i);
	} while (sample.size);

	printf("done");
	printf("\n");
}

static void welcome(char *prog_name)
{
	printf("\n%s - MP4 file library demuxer test program\n"
	       "Copyright (c) 2018 Parrot Drones SAS\n"
	       "Copyright (c) 2016 Aurelien Barre\n\n",
	       prog_name);
}


static void usage(char *prog_name)
{
	printf("Usage: %s <file>\n", prog_name);
}


int main(int argc, char **argv)
{

	int ret = 0, status = EXIT_SUCCESS;
	int rett = 0;
	struct mp4_demux *demux;
	struct timespec ts = {0, 0};
	uint64_t start_time = 0, end_time = 0;
	struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	// ret = get_seek_sample(track, start_time, 0);
	welcome(argv[0]);

	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	// time_get_monotonic(&ts);
	// time_timespec_to_us(&ts, &start_time);

	printf("opening demux\n");
	char *file = argv[1];
	ret = mp4_demux_open(file, &demux);

	printf("demux open\n");

	// time_get_monotonic(&ts);
	// time_timespec_to_us(&ts, &end_time);


	if (ret < 0) {
		ULOG_ERRNO("mp4_demux_open", -ret);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	printf("File '%s'\n", argv[1]);
	printf("Processing time: %.2fms\n\n",
	       (float)(end_time - start_time) / 1000.);
	print_info(demux);

	print_tracks(demux);

	print_metadata(demux);
	print_chapters(demux);
#if LOG_FRAMES
	print_frames(demux);
	ret = mp4_demux_close(demux);
	ret = mp4_demux_open(file, &demux);
	write_frames(demux);

	printf("print frames done\n");
#endif /* LOG_FRAMES */

cleanup:
	if (demux != NULL) {
		printf("closing the demux\n");
		ret = mp4_demux_close(demux);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_close", -ret);
			status = EXIT_FAILURE;
		}
	}

	printf("%s\n", (status == EXIT_SUCCESS) ? "Done!" : "Failed!");
	exit(status);
}
