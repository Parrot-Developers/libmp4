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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ULOG_TAG mp4_demux_test
#include <ulog.h>
ULOG_DECLARE_TAG(mp4_demux_test);

#include <futils/futils.h>
#include <libmp4.h>


#define DATE_SIZE 26


/* Enable to write the cover to a file */
#define WRITE_COVER 0

/* Enable to log all frames */
#define LOG_FRAMES 0


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
	time_local_format(info.creation_time,
			  0,
			  TIME_FMT_LONG,
			  creation_time_str,
			  DATE_SIZE);
	time_local_format(info.modification_time,
			  0,
			  TIME_FMT_LONG,
			  modification_time_str,
			  DATE_SIZE);

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
		time_local_format(tk.creation_time,
				  0,
				  TIME_FMT_LONG,
				  creation_time_str,
				  DATE_SIZE);
		time_local_format(tk.modification_time,
				  0,
				  TIME_FMT_LONG,
				  modification_time_str,
				  DATE_SIZE);

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
			if (tk.has_metadata) {
				printf("  metadata: present\n");
				printf("  metadata content encoding: %s\n",
				       tk.metadata_content_encoding);
				printf("  metadata mime format: %s\n",
				       tk.metadata_mime_format);
			}
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
			printf("  content encoding: %s\n",
			       tk.metadata_content_encoding);
			printf("  mime format: %s\n", tk.metadata_mime_format);
			break;
		default:
			break;
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
		cover_buffer = malloc(cover_buffer_size);
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
		ULOG_ERRNO("mp4_demux_get_metadata_cover", -ret);
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

	if (!found)
		return;

	i = 0;
	do {
		ret = mp4_demux_get_track_sample(
			demux, id, 1, NULL, 0, NULL, 0, &sample);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_get_track_sample", -ret);
			i++;
			continue;
		}

		printf("Frame #%d size=%06" PRIu32 " metadata_size=%" PRIu32
		       " dts=%" PRIu64 " next_dts=%" PRIu64 " sync=%d\n",
		       i,
		       sample.size,
		       sample.metadata_size,
		       sample.dts,
		       sample.next_dts,
		       sample.sync);
		i++;
	} while (sample.size);

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
	struct mp4_demux *demux;
	struct timespec ts = {0, 0};
	uint64_t start_time = 0, end_time = 0;

	welcome(argv[0]);

	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	time_get_monotonic(&ts);
	time_timespec_to_us(&ts, &start_time);

	ret = mp4_demux_open(argv[1], &demux);

	time_get_monotonic(&ts);
	time_timespec_to_us(&ts, &end_time);

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
#endif /* LOG_FRAMES */

cleanup:
	if (demux != NULL) {
		ret = mp4_demux_close(demux);
		if (ret < 0) {
			ULOG_ERRNO("mp4_demux_close", -ret);
			status = EXIT_FAILURE;
		}
	}

	printf("%s\n", (status == EXIT_SUCCESS) ? "Done!" : "Failed!");
	exit(status);
}
