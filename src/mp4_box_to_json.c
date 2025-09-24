/**
 * Copyright(c) 2022 Parrot Drones SAS
 */

#include "mp4_priv.h"
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_SUB_BOXES 20000
#define MAX_BOX_SIZE 5000000

static struct json_object *json_arr;


struct mp4_to_json_param {
	bool verbose;
	struct {
		int fd;
		off_t size;
		off_t read_bytes;
	} file;
	struct {
		struct json_object *root;
		struct json_object *tracks;
		bool last_box;
	} mp4;
	struct {
		struct json_object *json;
		uint32_t type;
		uint32_t level;
	} parent;
	struct {
		uint32_t count;
		char **keys;
		char **values;
	} meta;
	struct {
		struct json_object *json;
		uint32_t type;
		off_t size;
		uint8_t version;
		uint32_t flags;
		enum mp4_track_type track_type;
	} box;
};


static void clear_param_meta(struct mp4_to_json_param *param)
{
	if (param->meta.keys == NULL)
		return;

	for (size_t i = 0; i < param->meta.count; i++) {
		free(param->meta.keys[i]);
		free(param->meta.values[i]);
	}
	param->meta.count = 0;
	free(param->meta.keys);
	param->meta.keys = NULL;
	free(param->meta.values);
	param->meta.values = NULL;
}


/* Forward declarations */
static int add_box_to_json(struct mp4_to_json_param *param);


static inline void key_to_printable(char key[5])
{
	for (size_t i = 0; i < 4; i++) {
		if (key[i] < 32)
			key[i] = '.';
	}
}


static int uint_to_str(uint32_t str, char res[5])
{
	if (!res)
		return -EINVAL;
	(res)[3] = (char)((str >> 0) & 0xff);
	(res)[2] = (char)((str >> 8) & 0xff);
	(res)[1] = (char)((str >> (8 * 2)) & 0xff);
	(res)[0] = (char)((str >> (8 * 3)) & 0xff);
	(res)[4] = '\0';
	key_to_printable(res);
	return 0;
}


static int json_add_hex_from_binary_data(uint16_t bin_data_size,
					 const char *name,
					 struct mp4_to_json_param *param,
					 off_t *box_read_bytes)
{
	int ret = 0;
	uint8_t *bin_data = NULL;
	char *hex_data = NULL;
	ssize_t count;

	bin_data = malloc(bin_data_size + 1);
	if (bin_data == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	count = read(param->file.fd, bin_data, bin_data_size);
	if (count == -1) {
		ret = -errno;
		ULOG_ERRNO("read", -ret);
		goto out;
	} else if (count != bin_data_size) {
		ret = -EIO;
		ULOG_ERRNO("read", -ret);
		goto out;
	}
	*box_read_bytes += count;
	bin_data[bin_data_size] = '\0';

	hex_data = malloc(bin_data_size * 2 + 1);
	if (hex_data == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	for (size_t i = 0; i < bin_data_size; i++)
		snprintf(hex_data + 2 * i, 3, "%02X", (bin_data[i]));
	json_object_object_add(
		param->box.json, name, json_object_new_string(hex_data));

out:
	free(bin_data);
	free(hex_data);
	return 0;
}


static int skip_rest_of_box(struct mp4_to_json_param *param,
			    off_t *box_read_bytes)
{
	char name[5];
	if (*box_read_bytes >= param->box.size)
		return 0;

	(void)uint_to_str(param->box.type, name);

	if (*box_read_bytes == (off_t)8) {
		/* Skip full box */
		ULOGD("%s: skipping %d byte%s",
		      name,
		      (int)(param->box.size - *box_read_bytes),
		      (param->box.size - *box_read_bytes) > 0 ? "s" : "");
	} else {
		/* Skip end of box */
		ULOGW("%s: skipping %d byte%s",
		      name,
		      (int)(param->box.size - *box_read_bytes),
		      (param->box.size - *box_read_bytes) > 0 ? "s" : "");
	}

	json_object_object_add(
		param->box.json,
		"skipped_bytes",
		json_object_new_int64(param->box.size - *box_read_bytes));

	MP4_READ_SKIP(param->file.fd,
		      param->box.size - *box_read_bytes,
		      *box_read_bytes);

	return 0;
}


static int read_time_vars(struct mp4_to_json_param *param,
			  off_t *box_read_bytes,
			  bool has_track_info)
{
	uint32_t val32;
	uint32_t creation_time;
	uint32_t timescale;
	uint32_t modification_time;
	uint32_t duration;
	uint64_t creation_time64;
	uint64_t modification_time64;
	uint64_t duration64;

	switch (param->box.version) {
	case 1:
		/* 'creation_time' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		creation_time64 = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		creation_time64 |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		json_object_object_add(param->box.json,
				       "creation_time",
				       json_object_new_int64(creation_time64));

		/* 'modification_time' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		modification_time64 = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		modification_time64 |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		json_object_object_add(
			param->box.json,
			"modification_time",
			json_object_new_int64(modification_time64));

		if (has_track_info) {
			MP4_READ_32(param->file.fd, val32, *box_read_bytes);
			json_object_object_add(
				param->box.json,
				"track_id",
				json_object_new_int64((uint64_t)ntohl(val32)));
		}

		/* 'timescale' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		timescale = ntohl(val32);
		json_object_object_add(param->box.json,
				       "timescale",
				       json_object_new_int64(timescale));

		/* 'duration' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		duration64 = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		duration64 |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		json_object_object_add(param->box.json,
				       "duration",
				       json_object_new_int64(duration64));
		break;

	case 2:
	default:
		/* 'creation_time' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		creation_time = ntohl(val32);
		json_object_object_add(param->box.json,
				       "creation_time",
				       json_object_new_int64(creation_time));

		/* 'modification_time' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		modification_time = ntohl(val32);
		json_object_object_add(
			param->box.json,
			"modification_time",
			json_object_new_int64(modification_time));

		if (has_track_info) {
			MP4_READ_32(param->file.fd, val32, *box_read_bytes);
			json_object_object_add(
				param->box.json,
				"track_id",
				json_object_new_int64((uint64_t)ntohl(val32)));
		}

		/* 'timescale' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		timescale = ntohl(val32);
		json_object_object_add(param->box.json,
				       "timescale",
				       json_object_new_int64(timescale));

		/* 'duration' */
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		duration = (uint64_t)ntohl(val32);
		json_object_object_add(param->box.json,
				       "duration",
				       json_object_new_int64(duration));
		break;
	}
	return 0;
}


static int read_version_flags(struct mp4_to_json_param *param,
			      off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t flags;
	uint8_t version;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	flags = ntohl(val32);
	version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	json_object_object_add(
		param->box.json, "version", json_object_new_int64(version));
	json_object_object_add(
		param->box.json, "flags", json_object_new_int64(flags));

	param->box.version = version;
	param->box.flags = flags;

	return 0;
}


static int read_version_flags_empty_box(struct mp4_to_json_param *param,
					off_t *box_read_bytes)
{
	read_version_flags(param, box_read_bytes);
	return skip_rest_of_box(param, box_read_bytes);
}


static int read_container_box(struct mp4_to_json_param *param,
			      off_t *box_read_bytes)
{
	off_t err = 0;
	off_t container_size = param->box.size;
	struct json_object *mp4_json_box = param->parent.json;
	struct json_object *old_box = param->box.json;
	uint32_t old_level = param->parent.level;
	uint32_t old_type = param->parent.type;

	param->parent.json = param->box.json;
	param->parent.level++;
	param->parent.type = param->box.type;
	while (*box_read_bytes + 8 <= container_size && !param->mp4.last_box) {
		err = add_box_to_json(param);
		if (err < 0) {
			ULOG_ERRNO("add_box_to_json", -err);
			goto error;
		}
		*box_read_bytes += param->box.size;
	}
	param->parent.json = mp4_json_box;
	param->parent.level = old_level;
	param->parent.type = old_type;
	param->box.json = old_box;
	param->box.size = container_size;

	return skip_rest_of_box(param, box_read_bytes);

error:
	return err;
}


static int read_container_max_n_box(struct mp4_to_json_param *param,
				    off_t *box_read_bytes,
				    uint32_t max)
{
	off_t err = 0;
	off_t container_size = param->box.size;
	struct json_object *mp4_json_box = param->parent.json;
	struct json_object *old_box = param->box.json;
	uint32_t old_level = param->parent.level;
	uint32_t old_type = param->parent.type;

	param->parent.json = param->box.json;
	param->parent.level++;
	param->parent.type = param->box.type;
	for (uint32_t i = 0; i < max; i++) {
		err = add_box_to_json(param);
		if (err < 0) {
			ULOG_ERRNO("add_box_to_json", -err);
			goto error;
		}
		*box_read_bytes += param->box.size;
	}
	param->parent.json = mp4_json_box;
	param->parent.level = old_level;
	param->parent.type = old_type;
	param->box.json = old_box;
	param->box.size = container_size;

	return skip_rest_of_box(param, box_read_bytes);

error:
	return err;
}


static int read_container_version_box(struct mp4_to_json_param *param,
				      off_t *box_read_bytes)
{
	read_version_flags(param, box_read_bytes);
	return read_container_box(param, box_read_bytes);
}


static int read_ftyp_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	char brand[5];

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	(void)uint_to_str(ntohl(val32), brand);
	json_object_object_add(
		param->box.json, "major_brand", json_object_new_string(brand));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "minor_version",
			       json_object_new_int64(ntohl(val32)));

	json_arr = json_object_new_array();
	while (*box_read_bytes + 4 <= param->box.size) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		(void)uint_to_str(ntohl(val32), brand);
		json_object_array_add(json_arr, json_object_new_string(brand));
	}

	json_object_object_add(param->box.json, "compatible_brand", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_pasp_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "hSpacing",
			       json_object_new_int64(ntohl(val32)));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "vSpacing",
			       json_object_new_int64(ntohl(val32)));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_btrt_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "buffer_size",
			       json_object_new_int64(ntohl(val32)));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "max_bitrate",
			       json_object_new_int64(ntohl(val32)));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "average_bitrate",
			       json_object_new_int64(ntohl(val32)));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_meta_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	switch (param->parent.type) {
	case MP4_USER_DATA_BOX:
		return read_container_version_box(param, box_read_bytes);
	case MP4_MOVIE_BOX:
		return read_container_box(param, box_read_bytes);
	case MP4_ROOT_BOX:
		return read_container_version_box(param, box_read_bytes);
	case MP4_TRACK_BOX:
		return read_container_box(param, box_read_bytes);
	default:
		return skip_rest_of_box(param, box_read_bytes);
	}
}


static int read_dref_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	return read_container_box(param, box_read_bytes);
}


static int read_esds_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	off_t min_bytes = 9;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;
	uint8_t tag;
	off_t size = 0;
	int cnt = 0;
	uint8_t flags;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		param->box.json, "esds_version", json_object_new_int64(val32));

	MP4_READ_8(param->file.fd, tag, *box_read_bytes);
	if (tag != 3) {
		ULOGE("invalid ES_descriptor tag: %" PRIu8 ", expected %d",
		      tag,
		      0x03);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(
		param->box.json, "ES_descriptor", json_object_new_int64(tag));
	do {
		MP4_READ_8(param->file.fd, val8, *box_read_bytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid ES_descriptor size: more than 4 bytes");
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(param->box.json,
			       "ES_descriptor_size",
			       json_object_new_int64(size));

	min_bytes = *box_read_bytes + size;

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val16 = ntohs(val16);
	json_object_object_add(param->box.json,
			       "ES_descriptor_ES_ID",
			       json_object_new_int64(val16));

	MP4_READ_8(param->file.fd, flags, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "ES_descriptor_flags",
			       json_object_new_int64(flags));

	if (flags & 0x80) {
		MP4_READ_16(param->file.fd, val16, *box_read_bytes);
		val16 = ntohs(val16);
		json_object_object_add(param->box.json,
				       "ES_descriptor dependsOn_ES_ID",
				       json_object_new_int64(val16));
	}
	if (flags & 0x40) {
		MP4_READ_8(param->file.fd, val8, *box_read_bytes);
		json_object_object_add(param->box.json,
				       "ES_descriptor_url_len",
				       json_object_new_int64(val8));
		MP4_READ_SKIP(param->file.fd, val8, *box_read_bytes);
	}

	MP4_READ_8(param->file.fd, tag, *box_read_bytes);
	if (tag != 4) {
		ULOGE("invalid DecoderConfigDescriptor tag: %" PRIu8
		      ", expected %d",
		      tag,
		      0x04);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(param->box.json,
			       "decoder_config_descriptor",
			       json_object_new_int64(tag));
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(param->file.fd, val8, *box_read_bytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid DecoderConfigDescriptor size: "
		      "more than 4 bytes");
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(
		param->box.json, "DCD_size", json_object_new_int64(size));

	min_bytes = *box_read_bytes + size;

	/* 'DecoderConfigDescriptor.object_type_indication' */
	uint8_t object_type_indication;
	MP4_READ_8(param->file.fd, object_type_indication, *box_read_bytes);
	if (object_type_indication != 0x40) {
		ULOGE("invalid object_type_indication: %" PRIu8
		      ", expected 0x%x",
		      object_type_indication,
		      0x40);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(param->box.json,
			       "object_type_indication",
			       json_object_new_int64(object_type_indication));

	/* 'DecoderConfigDescriptor.stream_type' */
	uint8_t stream_type;
	MP4_READ_8(param->file.fd, stream_type, *box_read_bytes);
	stream_type >>= 2;
	if (stream_type != 0x5) {
		ULOGE("invalid stream_type: %" PRIu8 ", expected 0x%x",
		      stream_type,
		      0x5);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(param->box.json,
			       "stream_type",
			       json_object_new_int64(stream_type));

	/* Next 11 bytes unused */
	MP4_READ_SKIP(param->file.fd, 11, *box_read_bytes);

	/* 'decoder_specific_info' */
	MP4_READ_8(param->file.fd, tag, *box_read_bytes);
	if (tag != 5) {
		ULOGE("invalid decoder_specific_info tag: %" PRIu8
		      ", expected %d",
		      tag,
		      0x05);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(param->box.json,
			       "decoder_specific_info",
			       json_object_new_int64(tag));
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(param->file.fd, val8, *box_read_bytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid decoder_specific_info size: more than 4 bytes");
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_object_object_add(
		param->box.json, "DSI_size", json_object_new_int64(size));

	min_bytes = *box_read_bytes + size;

	if (size > MAX_ALLOC_SIZE) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}
	if ((size > 0)) {
		uint8_t *audio_specific_config = malloc(size);
		if (audio_specific_config == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}
		ssize_t count =
			read(param->file.fd, audio_specific_config, size);
		if (count == -1) {
			ULOG_ERRNO("fread", errno);
			free(audio_specific_config);
			return -errno;
		}
		json_object_object_add(param->box.json,
				       "audio_specific_config_size",
				       json_object_new_int64(size));
		*box_read_bytes += size;

		uint8_t audio_object_type = val8 >> 3;
		json_object_object_add(
			param->box.json,
			"audio_object_type",
			json_object_new_int64(audio_object_type));
		free(audio_specific_config);
	}

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(
		param->box.json, "SL_packet_tag", json_object_new_int(val8));
	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(
		param->box.json, "SL_packet_size", json_object_new_int(val8));
	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(
		param->box.json, "SL_packet_header", json_object_new_int(val8));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_gmin_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint16_t val16;
	uint16_t graphics_mode;
	uint16_t opcolor[3];
	uint16_t sound_balance;

	read_version_flags(param, box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	graphics_mode = ntohs(val16);

	json_object_object_add(param->box.json,
			       "graphics_mode",
			       json_object_new_int(graphics_mode));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[0] = ntohs(val16);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[1] = ntohs(val16);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[2] = ntohs(val16);
	json_object_object_add(
		param->box.json, "opcolor0", json_object_new_int(opcolor[0]));
	json_object_object_add(
		param->box.json, "opcolor1", json_object_new_int(opcolor[1]));
	json_object_object_add(
		param->box.json, "opcolor2", json_object_new_int(opcolor[2]));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	sound_balance = ntohs(val16);
	json_object_object_add(param->box.json,
			       "sound_balance",
			       json_object_new_int(sound_balance));

	/* Next 2 bytes unused */
	MP4_READ_SKIP(param->file.fd, 2, *box_read_bytes);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_hdlr_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t handler_type;
	char name[100];
	size_t name_size;

	read_version_flags(param, box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	handler_type = ntohl(val32);
	switch (handler_type) {
	case MP4_HANDLER_TYPE_VIDEO:
		param->box.track_type = MP4_TRACK_TYPE_VIDEO;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("video"));
		break;
	case MP4_HANDLER_TYPE_AUDIO:
		param->box.track_type = MP4_TRACK_TYPE_AUDIO;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("audio"));
		break;
	case MP4_HANDLER_TYPE_HINT:
		param->box.track_type = MP4_TRACK_TYPE_HINT;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("hint"));
		break;
	case MP4_HANDLER_TYPE_METADATA:
	case MP4_METADATA_NAMESPACE_MDTA:
		param->box.track_type = MP4_TRACK_TYPE_METADATA;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("metadata"));
		break;
	case MP4_HANDLER_TYPE_TEXT:
		param->box.track_type = MP4_TRACK_TYPE_TEXT;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("text"));
		break;
	case MP4_METADATA_HANDLER_TYPE_MDIR:
		param->box.track_type = MP4_TRACK_TYPE_TEXT;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("mdir"));
		break;
	default:
		param->box.track_type = MP4_TRACK_TYPE_UNKNOWN;
		json_object_object_add(param->box.json,
				       "handler_type",
				       json_object_new_string("unknown"));
		break;
	}
	for (size_t k = 0; k < 3; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	name_size = param->box.size - *box_read_bytes;
	if (read(param->file.fd, name, name_size) == -1) {
		ULOG_ERRNO("read", errno);
		return -errno;
	}
	*box_read_bytes += name_size;
	name[name_size - 1] = '\0';
	json_object_object_add(
		param->box.json, "name", json_object_new_string(name));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_tkhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	int16_t layer;
	int16_t alternate_group;
	float width, height, volume;

	read_version_flags(param, box_read_bytes);

	json_object_object_add(param->box.json,
			       "enabled",
			       json_object_new_boolean(!!(param->box.flags &
							  TRACK_FLAG_ENABLED)));
	json_object_object_add(
		param->box.json,
		"in_movie",
		json_object_new_boolean(
			!!(param->box.flags & TRACK_FLAG_IN_MOVIE)));
	json_object_object_add(
		param->box.json,
		"in_preview",
		json_object_new_boolean(
			!!(param->box.flags & TRACK_FLAG_IN_PREVIEW)));

	read_time_vars(param, box_read_bytes, true);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	layer = (int16_t)(ntohl(val32) >> 16);
	alternate_group = (int16_t)(ntohl(val32) & 0xFFFF);
	json_object_object_add(
		param->box.json, "layer", json_object_new_int(layer));
	json_object_object_add(param->box.json,
			       "alternate_group",
			       json_object_new_int(alternate_group));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	json_object_object_add(
		param->box.json, "volume", json_object_new_double(volume));

	for (size_t k = 0; k < 9; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	width = (float)ntohl(val32) / 65536.;
	json_object_object_add(
		param->box.json, "width", json_object_new_double(width));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	height = (float)ntohl(val32) / 65536.;
	json_object_object_add(
		param->box.json, "height", json_object_new_double(height));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_mvhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t next_track_ID;

	read_version_flags(param, box_read_bytes);
	read_time_vars(param, box_read_bytes, false);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	float rate = (float)ntohl(val32) / 65536.;
	json_object_object_add(
		param->box.json, "rate", json_object_new_int64(rate));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	json_object_object_add(
		param->box.json, "volume", json_object_new_int64(volume));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	for (size_t k = 0; k < 9; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	for (size_t k = 0; k < 6; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	next_track_ID = ntohl(val32);
	json_object_object_add(param->box.json,
			       "next_track_ID",
			       json_object_new_int64(next_track_ID));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stco_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t chunk_count;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	chunk_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "entry_count",
			       json_object_new_int64(chunk_count));

	if (chunk_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < chunk_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_array_add(json_arr,
				      json_object_new_int64(ntohl(val32)));
	}
	json_object_object_add(param->box.json, "chunk_entries", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_co64_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t chunk_count;
	uint64_t chunk_offset;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	chunk_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "chunk_count",
			       json_object_new_int64(chunk_count));

	if (chunk_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_arr = json_object_new_array();
	for (unsigned int i = 0; i < chunk_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		chunk_offset = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		chunk_offset |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		json_object_array_add(
			json_arr, json_object_new_int64(ntohl(chunk_offset)));
	}

	json_object_object_add(param->box.json, "chunk_entries", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stsz_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t sample_count;
	uint32_t sample_size;

	read_version_flags(param, box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	sample_size = ntohl(val32);
	json_object_object_add(param->box.json,
			       "sample_size",
			       json_object_new_int64(sample_size));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	sample_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "sample_count",
			       json_object_new_int64(sample_count));

	if (sample_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}

	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < sample_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_array_add(json_arr,
				      json_object_new_int64(ntohl(val32)));
	}
	json_object_object_add(param->box.json, "sample_entries", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stsd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t entry_count;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	entry_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "entry_count",
			       json_object_new_int64(entry_count));

	if (entry_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}

	return read_container_max_n_box(param, box_read_bytes, entry_count);
}


static int read_text_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	int res;
	uint8_t val8;
	uint8_t font_name_len;
	uint16_t val16;
	uint16_t data_reference_index;
	uint32_t val32;
	uint32_t display_flags;
	uint32_t text_justification;
	uint16_t bg_color[3];
	uint16_t fg_color[3];
	uint16_t default_text_box[4];
	uint16_t font_number;
	uint16_t font_face;

	/* Next 6 bytes unused */
	MP4_READ_SKIP(param->file.fd, 6, *box_read_bytes);

	/* Data ref index */
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	data_reference_index = htons(val16);
	json_object_object_add(param->box.json,
			       "data_reference_index",
			       json_object_new_int(data_reference_index));

	/* Display flags */
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	display_flags = htonl(val32);
	json_object_object_add(param->box.json,
			       "display_flags",
			       json_object_new_int(display_flags));

	/* Text justification */
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	text_justification = htonl(val32);
	json_object_object_add(param->box.json,
			       "text_justification",
			       json_object_new_int(text_justification));

	/* Background color */
	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < ARRAY_SIZE(bg_color); i++) {
		MP4_READ_16(param->file.fd, val16, *box_read_bytes);
		bg_color[i] = htons(val16);
		json_object_array_add(json_arr,
				      json_object_new_int(bg_color[i]));
	}
	json_object_object_add(param->box.json, "background_color", json_arr);

	/* Default text box */
	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < ARRAY_SIZE(default_text_box); i++) {
		MP4_READ_16(param->file.fd, val16, *box_read_bytes);
		default_text_box[i] = htons(val16);
		json_object_array_add(json_arr,
				      json_object_new_int(default_text_box[i]));
	}
	json_object_object_add(param->box.json, "default_text_box", json_arr);

	/* Next 8 bytes unused */
	MP4_READ_SKIP(param->file.fd, 8, *box_read_bytes);

	/* Font number */
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	font_number = htonl(val16);
	json_object_object_add(param->box.json,
			       "font_number",
			       json_object_new_int(font_number));

	/* Font face */
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	font_face = htonl(val16);
	json_object_object_add(
		param->box.json, "font_face", json_object_new_int(font_face));

	/* Next 1 byte unused */
	MP4_READ_SKIP(param->file.fd, 1, *box_read_bytes);

	/* Next 1 byte unused */
	/* Note: specs say 16-bits, but mediainfo and samples both say 8-bits */
	MP4_READ_SKIP(param->file.fd, 1, *box_read_bytes);

	/* Foreground color */
	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < ARRAY_SIZE(fg_color); i++) {
		MP4_READ_16(param->file.fd, val16, *box_read_bytes);
		fg_color[i] = htons(val16);
		json_object_array_add(json_arr,
				      json_object_new_int(fg_color[i]));
	}
	json_object_object_add(param->box.json, "foreground_color", json_arr);

	/* Text name (font name) */
	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	font_name_len = val8;
	if (font_name_len > 0 && font_name_len < UINT8_MAX) {
		char *font_name = calloc(1, font_name_len + 1);
		res = read(param->file.fd, font_name, font_name_len);
		*box_read_bytes += font_name_len;
		if (res != -1 && (size_t)res == font_name_len) {
			font_name[font_name_len] = '\0';
			json_object_object_add(
				param->box.json,
				"text_name",
				json_object_new_string(font_name));
		}
		free(font_name);
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stts_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t time_to_sample_entry_count;
	uint32_t sample_count;
	uint32_t sample_delta;
	struct json_object *time_to_sample_entry;

	read_version_flags(param, box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	time_to_sample_entry_count = ntohl(val32);
	json_object_object_add(
		param->box.json,
		"time_to_sample_entry_count",
		json_object_new_int64(time_to_sample_entry_count));

	if (time_to_sample_entry_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}

	json_arr = json_object_new_array();
	for (uint32_t i = 0; i < time_to_sample_entry_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		sample_count = ntohl(val32);
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		sample_delta = ntohl(val32);
		time_to_sample_entry = json_object_new_object();
		json_object_object_add(time_to_sample_entry,
				       "sample_count",
				       json_object_new_int64(sample_count));
		json_object_object_add(time_to_sample_entry,
				       "sample_delta",
				       json_object_new_int64(sample_delta));
		json_object_array_add(json_arr, time_to_sample_entry);
	}
	json_object_object_add(
		param->box.json, "time_to_sample_entries", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stss_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t sync_sample_entry_count;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	sync_sample_entry_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "entry_count",
			       json_object_new_int64(sync_sample_entry_count));

	if (sync_sample_entry_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}
	json_arr = json_object_new_array();
	for (size_t i = 0; i < sync_sample_entry_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_array_add(json_arr,
				      json_object_new_int64(ntohl(val32)));
	}
	json_object_object_add(param->box.json, "sample_number", json_arr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_stsc_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t sample_to_chunk_entry_count;
	uint32_t first_chunk;
	uint32_t sample_per_chunk;
	uint32_t sample_description_index;
	struct json_object *sample_to_chunk_entry;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	sample_to_chunk_entry_count = ntohl(val32);
	json_object_object_add(
		param->box.json,
		"sample_to_chunk_entry_count",
		json_object_new_int64(sample_to_chunk_entry_count));

	if (sample_to_chunk_entry_count > MAX_SUB_BOXES) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}

	if (sample_to_chunk_entry_count > 0) {
		json_arr = json_object_new_array();
		for (size_t i = 0; i < sample_to_chunk_entry_count; i++) {
			MP4_READ_32(param->file.fd, val32, *box_read_bytes);
			first_chunk = ntohl(val32);
			MP4_READ_32(param->file.fd, val32, *box_read_bytes);
			sample_per_chunk = ntohl(val32);
			MP4_READ_32(param->file.fd, val32, *box_read_bytes);
			sample_description_index = ntohl(val32);
			sample_to_chunk_entry = json_object_new_object();
			json_object_object_add(
				sample_to_chunk_entry,
				"first_chunk",
				json_object_new_int64(first_chunk));
			json_object_object_add(
				sample_to_chunk_entry,
				"sample_per_chunk",
				json_object_new_int64(sample_per_chunk));
			json_object_object_add(
				sample_to_chunk_entry,
				"sample_description_index",
				json_object_new_int64(
					sample_description_index));
			json_object_array_add(json_arr, sample_to_chunk_entry);
		}
		json_object_object_add(
			param->box.json, "sample_to_chunk_entry", json_arr);
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_ilst_box_child(struct mp4_to_json_param *param,
			       off_t *box_read_bytes)
{
	uint32_t val32;
	char *value;
	size_t length;
	ssize_t res;

	if (param->parent.type != MP4_ILST_BOX)
		goto skip;

	read_version_flags(param, box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	length = param->box.size - *box_read_bytes;
	if (length > MAX_ALLOC_SIZE) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}

	value = malloc(length + 1);

	res = read(param->file.fd, value, length);
	*box_read_bytes += length;
	if (res != -1 && (size_t)res == length) {
		value[length] = '\0';
		json_object_object_add(param->box.json,
				       "value",
				       json_object_new_string(value));
	}
	free(value);

skip:
	return skip_rest_of_box(param, box_read_bytes);
}


static int read_mett_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint16_t val16;
	uint8_t val8;
	uint32_t val32;
	size_t len = 0;
	char *newstr;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "data_reference_index",
			       json_object_new_int(ntohs(val16)));
	len = param->box.size - *box_read_bytes;
	if (len > MAX_ALLOC_SIZE) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		return skip_rest_of_box(param, box_read_bytes);
	}
	newstr = calloc(len + 1, 1);
	val8 = 1;
	if (read(param->file.fd, newstr, len) == -1) {
		free(newstr);
		ULOG_ERRNO("read", errno);
		return -errno;
	}
	*box_read_bytes += len;
	newstr[len] = '\0';
	json_object_object_add(param->box.json,
			       "mime_type",
			       json_object_new_string(newstr + 1));
	free(newstr);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_elst_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t entry_count;
	struct json_object *obj;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	entry_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "entry_count",
			       json_object_new_int64(entry_count));

	json_arr = json_object_new_array();
	json_object_object_add(param->box.json, "edit_list_table", json_arr);
	for (size_t i = 0; i < entry_count; i++) {
		obj = json_object_new_object();

		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_object_add(obj,
				       "track_duration",
				       json_object_new_int64(ntohl(val32)));

		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_object_add(
			obj, "media_time", json_object_new_int64(ntohl(val32)));

		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_object_add(
			obj, "media_rate", json_object_new_int64(ntohl(val32)));

		json_object_array_add(json_arr, obj);
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_tref_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint32_t ref_type;
	char reference_type[5];
	uint32_t reference_type_size;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	reference_type_size = ntohl(val32);
	json_object_object_add(param->box.json,
			       "reference_type_size",
			       json_object_new_int64(reference_type_size));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	ref_type = ntohl(val32);
	(void)uint_to_str(ref_type, reference_type);
	json_object_object_add(param->box.json,
			       "reference_type",
			       json_object_new_string(reference_type));

	json_arr = json_object_new_array();
	json_object_object_add(param->box.json, "track_reference_id", json_arr);
	while (*box_read_bytes < param->box.size) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		json_object_array_add(json_arr,
				      json_object_new_int64(ntohl(val32)));
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_mdhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t language;

	read_version_flags(param, box_read_bytes);
	read_time_vars(param, box_read_bytes, false);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	language = (uint16_t)(ntohl(val32) >> 16) & 0x7FFF;
	json_object_object_add(
		param->box.json, "language", json_object_new_int64(language));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_vmhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint16_t val16;
	uint16_t opcolor[3];
	uint16_t graphics_mode;

	read_version_flags(param, box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	graphics_mode = ntohs(val16);

	json_object_object_add(param->box.json,
			       "graphics_mode",
			       json_object_new_int64(graphics_mode));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[0] = ntohs(val16);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[1] = ntohs(val16);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	opcolor[2] = ntohs(val16);
	json_object_object_add(
		param->box.json, "opcolor0", json_object_new_int64(opcolor[0]));
	json_object_object_add(
		param->box.json, "opcolor1", json_object_new_int64(opcolor[1]));
	json_object_object_add(
		param->box.json, "opcolor2", json_object_new_int64(opcolor[2]));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_smhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	float balance;

	read_version_flags(param, box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	balance = (float)((int16_t)((ntohl(val32) >> 16) & 0xFFFF)) / 256.;
	json_object_object_add(
		param->box.json, "balance", json_object_new_int64(balance));

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_avc1_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t val16;
	uint16_t data_reference_index;
	float horizresolution, vertresolution;
	uint16_t depth;
	ssize_t count;
	uint16_t frame_count;
	char compressorname[32];

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	data_reference_index = ntohs(val16);
	json_object_object_add(param->box.json,
			       "data_reference_index",
			       json_object_new_int64(data_reference_index));

	for (size_t k = 0; k < 4; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		param->box.json,
		"width",
		json_object_new_int64((ntohl(val32) >> 16) & 0xFFFF));
	json_object_object_add(param->box.json,
			       "height",
			       json_object_new_int64(ntohl(val32) & 0xFFFF));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	horizresolution = (float)(ntohl(val32)) / 65536.;
	json_object_object_add(param->box.json,
			       "horizontal_resolution",
			       json_object_new_double(horizresolution));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	vertresolution = (float)(ntohl(val32)) / 65536.;
	json_object_object_add(param->box.json,
			       "vertical_resolution",
			       json_object_new_double(vertresolution));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	frame_count = ntohs(val16);
	json_object_object_add(param->box.json,
			       "frame_count",
			       json_object_new_int64(frame_count));
	count = read(param->file.fd, compressorname, 32);
	if (count == -1 || (off_t)count != 32) {
		ULOG_ERRNO("read", errno);
		return skip_rest_of_box(param, box_read_bytes);
	}
	*box_read_bytes += 32;
	compressorname[31] = '\0';
	json_object_object_add(param->box.json,
			       "compressor_name",
			       json_object_new_string(compressorname));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	depth = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	json_object_object_add(
		param->box.json, "depth", json_object_new_int64(depth));

	return read_container_box(param, box_read_bytes);
}


static int read_hvc1_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t val16;
	uint16_t data_reference_index;
	float horizresolution, vertresolution;
	uint16_t depth;
	ssize_t count;
	uint16_t frame_count;
	char compressorname[32];

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	data_reference_index = ntohs(val16);
	json_object_object_add(param->box.json,
			       "data_reference_index",
			       json_object_new_int64(data_reference_index));

	for (size_t k = 0; k < 4; k++)
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		param->box.json,
		"width",
		json_object_new_int64((ntohl(val32) >> 16) & 0xFFFF));
	json_object_object_add(param->box.json,
			       "height",
			       json_object_new_int64(ntohl(val32) & 0xFFFF));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	horizresolution = (float)(ntohl(val32)) / 65536.;
	json_object_object_add(param->box.json,
			       "horizontal_resolution",
			       json_object_new_double(horizresolution));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	vertresolution = (float)(ntohl(val32)) / 65536.;
	json_object_object_add(param->box.json,
			       "vertical_resolution",
			       json_object_new_double(vertresolution));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	frame_count = ntohs(val16);
	json_object_object_add(param->box.json,
			       "frame_count",
			       json_object_new_int64(frame_count));
	count = read(param->file.fd, compressorname, 32);
	if (count == -1 || (off_t)count != 32) {
		ULOG_ERRNO("read", errno);
		return skip_rest_of_box(param, box_read_bytes);
	}
	*box_read_bytes += 32;
	compressorname[31] = '\0';
	json_object_object_add(param->box.json,
			       "compressor_name",
			       json_object_new_string(compressorname));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	depth = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	json_object_object_add(
		param->box.json, "depth", json_object_new_int64(depth));

	return read_container_box(param, box_read_bytes);
}


static int read_hvcc_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t val16;
	uint8_t val8, nb_arrays;
	uint8_t version;
	int err = 0;

	MP4_READ_8(param->file.fd, version, *box_read_bytes);
	if (version != 1)
		ULOGE("hvcC configurationVersion mismatch: %u (expected 1)",
		      version);
	json_object_object_add(param->box.json,
			       "general_profile_idc",
			       json_object_new_int64(htons(version)));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	char general_profile_space = val8 >> 6;
	char general_tier_flag = (val8 >> 5) & 0x01;
	char general_profile_idc = val8 & 0x1F;
	json_object_object_add(
		param->box.json,
		"general_profile_space",
		json_object_new_int64(htons(general_profile_space)));
	json_object_object_add(param->box.json,
			       "general_tier_flag",
			       json_object_new_int64(htons(general_tier_flag)));
	json_object_object_add(
		param->box.json,
		"general_profile_idc",
		json_object_new_int64(htons(general_profile_idc)));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	uint32_t general_profile_compatibility_flags = htonl(val32);
	json_object_object_add(
		param->box.json,
		"general_profile_compatibility_flags",
		json_object_new_int64(general_profile_compatibility_flags));


	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val32 = htonl(val32);
	val16 = htons(val16);
	uint64_t general_constraints_indicator_flags =
		(((uint64_t)val32) << 16) + val16;
	json_object_object_add(
		param->box.json,
		"general_constraints_indicator_flags",
		json_object_new_int64(general_constraints_indicator_flags));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "general_level_idc",
			       json_object_new_int64(htons(val8)));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val16 = htons(val16);
	json_object_object_add(param->box.json,
			       "min_spatial_segmentation_idc",
			       json_object_new_int64(val16 & 0x0FFF));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "parallelism_type",
			       json_object_new_int64(val8 & 0x02));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "chroma_format",
			       json_object_new_int64(val8 & 0x02));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "bit_depth_luma",
			       json_object_new_int64((val8 & 0x03) + 8));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "bit_depth_chroma",
			       json_object_new_int64((val8 & 0x03) + 8));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "avg_framerate",
			       json_object_new_int64(htons(val16)));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "constant_framerate",
			       json_object_new_int64((val8 >> 6) & 0x03));
	json_object_object_add(param->box.json,
			       "num_temporal_layers",
			       json_object_new_int64((val8 >> 3) & 0x7));
	json_object_object_add(param->box.json,
			       "temporal_id_nested",
			       json_object_new_int64((val8 >> 2) & 0x01));
	json_object_object_add(param->box.json,
			       "length_size",
			       json_object_new_int64((val8 & 0x03) + 1));

	MP4_READ_8(param->file.fd, val8, *box_read_bytes);
	if (val8 > 16) {
		ULOGE("hvcC: invalid numOfArrays=%d", val8);
		return skip_rest_of_box(param, box_read_bytes);
	}
	nb_arrays = val8;
	json_object_object_add(param->box.json,
			       "array_size",
			       json_object_new_int64(nb_arrays));
	bool first_vps = true;
	bool first_pps = true;
	bool first_sps = true;
	for (int i = 0; i < nb_arrays; i++) {
		uint8_t array_completeness, nalu_type;
		uint16_t nb_nalus;

		json_object_object_add(
			param->box.json, "nalu_#", json_object_new_int64(i));

		MP4_READ_8(param->file.fd, val8, *box_read_bytes);
		array_completeness = (val8 >> 7) & 0x01;
		nalu_type = val8 & 0x3F;
		json_object_object_add(
			param->box.json,
			"array_completeness",
			json_object_new_int64(array_completeness));
		json_object_object_add(param->box.json,
				       "nalu_type",
				       json_object_new_int64(nalu_type));

		MP4_READ_16(param->file.fd, val16, *box_read_bytes);
		nb_nalus = htons(val16);
		if (nb_nalus > 16) {
			ULOGE("hvcC: invalid numNalus=%d", nb_nalus);
			return skip_rest_of_box(param, box_read_bytes);
		}
		json_object_object_add(param->box.json,
				       "nb_nalus",
				       json_object_new_int64(nb_nalus));

		for (int j = 0; j < nb_nalus; j++) {
			uint16_t nalu_length;

			MP4_READ_16(param->file.fd, val16, *box_read_bytes);
			nalu_length = htons(val16);

			if (nalu_length) {
				switch (nalu_type) {
				case MP4_H265_NALU_TYPE_VPS:
					if (!first_vps)
						break;
					first_vps = false;
					json_object_object_add(
						param->box.json,
						"hevc_vps_size",
						json_object_new_int64(
							nalu_length));
					err = json_add_hex_from_binary_data(
						nalu_length,
						"hevc_vps",
						param,
						box_read_bytes);
					if (err < 0) {
						ULOG_ERRNO(
							"json_add_hex_from_"
							"binary_data",
							-err);
						return err;
					}
					break;
				case MP4_H265_NALU_TYPE_SPS:
					if (!first_sps)
						break;
					first_sps = false;
					json_object_object_add(
						param->box.json,
						"hevc_sps_size",
						json_object_new_int64(
							nalu_length));
					err = json_add_hex_from_binary_data(
						nalu_length,
						"hevc_sps",
						param,
						box_read_bytes);
					if (err < 0) {
						ULOG_ERRNO(
							"json_add_hex_from_"
							"binary_data",
							-err);
						return err;
					}
					break;
				case MP4_H265_NALU_TYPE_PPS:
					if (!first_pps)
						break;
					first_pps = false;
					json_object_object_add(
						param->box.json,
						"hevc_pps_size",
						json_object_new_int64(
							nalu_length));
					err = json_add_hex_from_binary_data(
						nalu_length,
						"hevc_pps",
						param,
						box_read_bytes);
					if (err < 0) {
						ULOG_ERRNO(
							"json_add_hex_from_"
							"binary_data",
							-err);
						return err;
					}
					break;
				default:
					json_object_object_add(
						param->box.json,
						"ignoring_nalu",
						json_object_new_int64(
							nalu_type));
					off_t ret = lseek(param->file.fd,
							  nalu_length,
							  SEEK_CUR);
					if (ret == -1) {
						ULOG_ERRNO("lseek", errno);
						return skip_rest_of_box(
							param, box_read_bytes);
					}
					break;
				}
			}
			*box_read_bytes += nalu_length;
		}
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_avcc_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t val16;
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	val32 = ntohl(val32);
	uint8_t version = (val32 >> 24) & 0xFF;
	uint8_t profile = (val32 >> 16) & 0xFF;
	uint8_t profile_compat = (val32 >> 8) & 0xFF;
	uint8_t level = val32 & 0xFF;
	int err = 0;

	json_object_object_add(param->box.json,
			       "configuration_version",
			       json_object_new_int64(version));
	json_object_object_add(param->box.json,
			       "AVC_profile_indication",
			       json_object_new_int64(profile));
	json_object_object_add(param->box.json,
			       "profile_compatibility",
			       json_object_new_int64(profile_compat));
	json_object_object_add(param->box.json,
			       "AVC_level_indication",
			       json_object_new_int64(level));
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val16 = ntohs(val16);
	uint8_t lengthSize = ((val16 >> 8) & 0x3) + 1;
	uint8_t sps_count = val16 & 0x1F;
	json_object_object_add(param->box.json,
			       "nal_unit_size",
			       json_object_new_int64(lengthSize));
	json_object_object_add(
		param->box.json, "sps_count", json_object_new_int64(sps_count));
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val16 = ntohs(val16);
	json_object_object_add(param->box.json,
			       "sequence_parameter_length",
			       json_object_new_int64(val16));
	err = json_add_hex_from_binary_data(
		val16, "sequence_parameter", param, box_read_bytes);
	if (err < 0) {
		ULOG_ERRNO("json_add_hex_from_binary_data", -err);
		return err;
	}
	if (lseek(param->file.fd, 1, SEEK_CUR) == -1)
		return -errno;
	(*box_read_bytes)++;
	/* 'pictureParameterSetLength' */
	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	val16 = ntohs(val16);
	json_object_object_add(param->box.json,
			       "picture_parameter_length",
			       json_object_new_int64(val16));
	err = json_add_hex_from_binary_data(
		val16, "picture_parameter", param, box_read_bytes);
	if (err < 0) {
		ULOG_ERRNO("json_add_hex_from_binary_data", -err);
		return err;
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_ilst_meta_box(struct mp4_to_json_param *param,
			      off_t *box_read_bytes,
			      struct json_object *metadata)
{
	uint32_t val32;
	uint32_t index;
	uint32_t length;
	ssize_t count;
	char name[5];

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	length = ntohl(val32);
	if (length < 24) {
		ULOGE("invalid length: %" PRIu32 ", expected %d min",
		      length,
		      24);
		goto out;
	}
	length -= 24;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	index = ntohl(val32);
	if (index < 1)
		goto out;
	index -= 1;
	if (index > param->meta.count)
		goto out;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		metadata, "size", json_object_new_int(ntohl(val32)));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	(void)uint_to_str(ntohl(val32), name);
	json_object_object_add(
		metadata, "sub-box_type", json_object_new_string(name));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(metadata,
			       "sub-box_data_type",
			       json_object_new_int(ntohl(val32)));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		metadata, "sub-box_locale", json_object_new_int(ntohl(val32)));

	if (param->meta.values == NULL) {
		ULOGE("no meta values");
		return skip_rest_of_box(param, box_read_bytes);
	}
	param->meta.values[index] = calloc(length, sizeof(*param->meta.values));
	if (param->meta.values[index] == NULL) {
		ULOGE("calloc");
		return skip_rest_of_box(param, box_read_bytes);
	}
	count = read(param->file.fd, param->meta.values[index], length);
	if (count == -1 || count != (ssize_t)length) {
		ULOG_ERRNO("read", errno);
		goto out;
	}
	*box_read_bytes += length;

out:
	return 0;
}


static int read_ilst_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	struct json_object *metadata;

	if (param->box.track_type == MP4_TRACK_TYPE_METADATA) {
		json_arr = json_object_new_array();
		for (size_t i = 0; i < param->meta.count; i++) {
			metadata = json_object_new_object();
			read_ilst_meta_box(param, box_read_bytes, metadata);
			/* key is not present in ilst box */
			json_object_object_add(
				metadata,
				"(key)",
				json_object_new_string(param->meta.keys[i]));
			json_object_object_add(
				metadata,
				"value",
				json_object_new_string(param->meta.values[i]));
			json_object_array_add(json_arr, metadata);
		}
		json_object_object_add(param->box.json, "data_vals", json_arr);

		return 0;
	}

	return read_container_box(param, box_read_bytes);
}


static int read_xyz_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint16_t val16;
	ssize_t count;
	char *udta_location_value = NULL;
	uint16_t locationSize, languageCode;
	char name[5];

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	locationSize = ntohs(val16);
	json_object_object_add(param->box.json,
			       "location_size",
			       json_object_new_int64(locationSize));

	MP4_READ_16(param->file.fd, val16, *box_read_bytes);
	languageCode = ntohs(val16);
	json_object_object_add(param->box.json,
			       "language_code",
			       json_object_new_int64(languageCode));

	(void)uint_to_str(param->box.type, name);
	json_object_object_add(param->box.json,
			       "udta_location_key",
			       json_object_new_string(name));

	if (locationSize < 1 || locationSize >= UINT16_MAX) {
		ULOG_ERRNO("invalid location size", EINVAL);
		goto out;
	}
	udta_location_value = calloc(1, locationSize + 1);
	if (udta_location_value == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		goto out;
	}
	count = read(param->file.fd, udta_location_value, locationSize);
	if (count == -1 || count != (ssize_t)locationSize) {
		ULOG_ERRNO("read", errno);
		goto out;
	}
	*box_read_bytes += locationSize;
	udta_location_value[locationSize] = '\0';
	json_object_object_add(param->box.json,
			       "udta_location_value",
			       json_object_new_string(udta_location_value));
out:
	if (udta_location_value)
		free(udta_location_value);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_keys_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32, i;
	unsigned int metadata_count;
	char *metadata_key = NULL;
	uint32_t key_size;
	ssize_t count;
	struct json_object *metadata;
	int ret = 0;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	metadata_count = ntohl(val32);
	json_object_object_add(param->box.json,
			       "metadata_count",
			       json_object_new_int64(metadata_count));

	if (metadata_count > MAX_ALLOC_SIZE) {
		ULOGW("%s size too big, skipping rest of box", __func__);
		goto skip;
	}

	if (metadata_count == 0)
		goto skip;
	json_arr = json_object_new_array();

	clear_param_meta(param);

	param->meta.count = metadata_count;
	param->meta.keys = calloc(param->meta.count, sizeof(*param->meta.keys));
	if (param->meta.keys == NULL) {
		ULOGE("calloc");
		goto skip;
	}
	param->meta.values =
		calloc(param->meta.count, sizeof(*param->meta.values));
	if (param->meta.values == NULL) {
		ULOGE("calloc");
		goto skip;
	}
	for (i = 0; i < metadata_count; i++) {
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		key_size = ntohl(val32);
		if (key_size < 8) {
			ULOGE("invalid key size: %" PRIu32 ", expected %d min",
			      key_size,
			      8);
			ret = -EPROTO;
			goto out;
		}
		key_size -= 8;
		MP4_READ_32(param->file.fd, val32, *box_read_bytes);
		char key_name_space[5];
		(void)uint_to_str(ntohl(val32), key_name_space);
		if (key_size > MAX_ALLOC_SIZE) {
			ULOGW("%s size too big, skipping rest of box",
			      __func__);
			goto skip;
		}
		metadata_key = malloc(key_size + 1);
		if (metadata_key == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("malloc", -ret);
			goto out;
		}
		count = read(param->file.fd, metadata_key, key_size);
		if (count == -1 || count != (ssize_t)key_size) {
			ret = -errno;
			ULOG_ERRNO("read", -ret);
			goto out;
		}
		metadata = json_object_new_object();
		json_object_object_add(
			metadata, "key_size", json_object_new_int64(key_size));
		*box_read_bytes += key_size;
		metadata_key[key_size] = '\0';
		param->meta.keys[i] = strdup(metadata_key);
		json_object_object_add(metadata,
				       "key_value",
				       json_object_new_string(metadata_key));

		json_object_object_add(metadata,
				       "key_name_space",
				       json_object_new_string(key_name_space));
		json_object_array_add(json_arr, metadata);
		free(metadata_key);
		metadata_key = NULL;
	}

	json_object_object_add(param->box.json, "keys", json_arr);

skip:
	ret = skip_rest_of_box(param, box_read_bytes);
out:
	free(metadata_key);
	return ret;
}


static const char *mp4_metadata_class_to_str(int clazz)
{
	switch (clazz) {
	case MP4_METADATA_CLASS_UTF8:
		return "UTF8";
	case MP4_METADATA_CLASS_JPEG:
		return "JPEG";
	case MP4_METADATA_CLASS_PNG:
		return "PNG";
	case MP4_METADATA_CLASS_BMP:
		return "BMP";
	default:
		return "UNKNOWN";
	}
}


static int read_data_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	char *value = NULL;
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	uint32_t clazz = ntohl(val32);
	uint8_t version = (clazz >> 24) & 0xFF;
	clazz &= 0xFF;
	json_object_object_add(
		param->box.json, "version", json_object_new_int64(version));

	json_object_object_add(
		param->box.json,
		"clazz",
		json_object_new_string(mp4_metadata_class_to_str(clazz)));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	unsigned int valueLen = param->box.size - *box_read_bytes;

	if (clazz == MP4_METADATA_CLASS_UTF8) {
		if (valueLen > MAX_ALLOC_SIZE) {
			ULOGW("%s size too big, skipping rest of box",
			      __func__);
			return skip_rest_of_box(param, box_read_bytes);
		}
		value = malloc(valueLen + 1);
		ssize_t count = read(param->file.fd, value, valueLen);
		if (count == -1) {
			ULOG_ERRNO("read", errno);
			free(value);
			return -errno;
		}
		*box_read_bytes += valueLen;
		value[valueLen] = '\0';
		json_object_object_add(param->box.json,
				       "value",
				       json_object_new_string(value));
		free(value);
	} else {
		/* avoid reading an image */
		MP4_READ_SKIP(param->file.fd,
			      param->box.size - *box_read_bytes,
			      *box_read_bytes);
	}

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_mp4a_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t data_reference_index;

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	data_reference_index = (uint16_t)(ntohl(val32) & 0xFFFF);
	json_object_object_add(param->box.json,
			       "data_reference_index",
			       json_object_new_int64(data_reference_index));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(
		param->box.json,
		"audioChannelCount",
		json_object_new_int64((ntohl(val32) >> 16) & 0xFFFF));
	json_object_object_add(param->box.json,
			       "audio_sample_size",
			       json_object_new_int64(ntohl(val32) & 0xFFFF));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	json_object_object_add(param->box.json,
			       "audioSampleRate",
			       json_object_new_int64(ntohl(val32)));

	return read_container_box(param, box_read_bytes);
}


static int read_hmhd_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	uint32_t val32;
	uint16_t max_PDU_size;
	uint16_t avg_PDU_size;
	uint32_t max_bitrate;
	uint32_t avg_bitrate;

	read_version_flags(param, box_read_bytes);
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	max_PDU_size = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	avg_PDU_size = (uint16_t)(ntohl(val32) & 0xFFFF);
	json_object_object_add(param->box.json,
			       "max_PDU_size",
			       json_object_new_int64(max_PDU_size));
	json_object_object_add(param->box.json,
			       "avg_PDU_size",
			       json_object_new_int64(avg_PDU_size));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	max_bitrate = ntohl(val32);
	json_object_object_add(param->box.json,
			       "max_bitrate",
			       json_object_new_int64(max_bitrate));

	MP4_READ_32(param->file.fd, val32, *box_read_bytes);
	avg_bitrate = ntohl(val32);
	json_object_object_add(param->box.json,
			       "avg_bitrate",
			       json_object_new_int64(avg_bitrate));
	MP4_READ_32(param->file.fd, val32, *box_read_bytes);

	return skip_rest_of_box(param, box_read_bytes);
}


static int read_uuid_box(struct mp4_to_json_param *param, off_t *box_read_bytes)
{
	char *uuid = malloc(17);
	ssize_t count;

	count = read(param->file.fd, uuid, 17);
	if (count == -1) {
		ULOG_ERRNO("read", errno);
		free(uuid);
		return -errno;
	}
	uuid[16] = '\0';
	json_object_object_add(
		param->box.json, "UUID", json_object_new_string(uuid));

	*box_read_bytes += 17;

	free(uuid);
	return skip_rest_of_box(param, box_read_bytes);
}


static const struct {
	uint32_t type;
	int (*func)(struct mp4_to_json_param *param, off_t *box_read_bytes);
} box_type_map[] = {
	/* skipped boxes */
	{MP4_MDAT_BOX, &skip_rest_of_box},
	{MP4_DATA_REFERENCE_TYPE_URL, &read_version_flags_empty_box},
	/* free */
	{MP4_FREE_BOX, &skip_rest_of_box},
	/* container */
	{MP4_ROOT_BOX, &read_container_box},
	{MP4_MOVIE_BOX, &read_container_box},
	{MP4_TRACK_BOX, &read_container_box},
	{MP4_MEDIA_INFORMATION_BOX, &read_container_box},
	{MP4_DATA_INFORMATION_BOX, &read_container_box},
	{MP4_MEDIA_BOX, &read_container_box},
	{MP4_EDTS_BOX, &read_container_box},
	{MP4_SAMPLE_TABLE_BOX, &read_container_box},
	{MP4_USER_DATA_BOX, &read_container_box},
	{MP4_ILST_BOX, &read_ilst_box},
	{MP4_META_BOX, &read_meta_box},
	/* ilst child */
	{MP4_METADATA_TAG_TYPE_ARTIST, &read_container_box},
	{MP4_METADATA_TAG_TYPE_TITLE, &read_container_box},
	{MP4_METADATA_TAG_TYPE_DATE, &read_container_box},
	{MP4_METADATA_TAG_TYPE_MAKER, &read_container_box},
	{MP4_METADATA_TAG_TYPE_MODEL, &read_container_box},
	{MP4_METADATA_TAG_TYPE_VERSION, &read_container_box},
	{MP4_METADATA_TAG_TYPE_COMMENT, &read_container_box},
	{MP4_METADATA_TAG_TYPE_COPYRIGHT, &read_container_box},
	{MP4_METADATA_TAG_TYPE_ENCODER, &read_container_box},
	{MP4_LOCATION_BOX, &read_xyz_box},
	{MP4_METADATA_TAG_TYPE_COVER, &read_container_box},
	/* samples tables boxes */
	{MP4_SAMPLE_DESCRIPTION_BOX, &read_stsd_box},
	{MP4_TEXT_SAMPLE_ENTRY, &read_text_box},
	{MP4_DECODING_TIME_TO_SAMPLE_BOX, &read_stts_box},
	{MP4_SAMPLE_TO_CHUNK_BOX, &read_stsc_box},
	{MP4_CHUNK_OFFSET_BOX, &read_stco_box},
	{MP4_CHUNK_OFFSET_64_BOX, &read_co64_box},
	{MP4_SAMPLE_SIZE_BOX, &read_stsz_box},
	{MP4_SYNC_SAMPLE_BOX, &read_stss_box},
	/* encoding / decoding */
	{MP4_AVC1, &read_avc1_box},
	{MP4_HVC1, &read_hvc1_box},
	{MP4_HEVC_DECODER_CONFIG_BOX, &read_hvcc_box},
	{MP4_AVC_DECODER_CONFIG_BOX, &read_avcc_box},
	/* headers */
	{MP4_TRACK_HEADER_BOX, &read_tkhd_box},
	{MP4_MOVIE_HEADER_BOX, &read_mvhd_box},
	{MP4_MEDIA_HEADER_BOX, &read_mdhd_box},
	{MP4_VIDEO_MEDIA_HEADER_BOX, &read_vmhd_box},
	{MP4_SOUND_MEDIA_HEADER_BOX, &read_smhd_box},
	{MP4_HINT_MEDIA_HEADER_BOX, &read_hmhd_box},
	{MP4_NULL_MEDIA_HEADER_BOX, &read_version_flags_empty_box},
	{MP4_GENERIC_MEDIA_HEADER_BOX, &read_container_box},
	/* other */
	{MP4_UUID, &read_uuid_box},
	{MP4_FILE_TYPE_BOX, &read_ftyp_box},
	{MP4_DATA_REFERENCE_BOX, &read_dref_box},
	{MP4_ELST, &read_elst_box},
	{MP4_PASP, &read_pasp_box},
	{MP4_BTRT, &read_btrt_box},
	{MP4_TRACK_REFERENCE_BOX, &read_tref_box},
	{MP4_HANDLER_REFERENCE_BOX, &read_hdlr_box},
	{MP4_KEYS_BOX, &read_keys_box},
	{MP4_DATA_BOX, &read_data_box},
	{MP4_ISOM, &read_ilst_box_child},
	{MP4_ISO2, &read_ilst_box_child},
	{MP4_MP41, &read_ilst_box_child},
	{MP4_MHLR, &read_ilst_box_child},
	{MP4_REFERENCE_TYPE_DESCRIPTION, &read_ilst_box_child},
	{MP4_REFERENCE_TYPE_HINT_USED, &read_ilst_box_child},
	{MP4_REFERENCE_TYPE_CHAPTERS, &read_ilst_box_child},
	{MP4_XML_METADATA_SAMPLE_ENTRY, &read_ilst_box_child},
	{MP4_TEXT_METADATA_SAMPLE_ENTRY, &read_mett_box},
	{MP4_METADATA_NAMESPACE_MDTA, &read_ilst_box_child},
	{MP4_METADATA_HANDLER_TYPE_APPL, &read_ilst_box_child},
	{MP4_MP4A, &read_mp4a_box},
	{MP4_AUDIO_DECODER_CONFIG_BOX, &read_esds_box},
	{MP4_GENERIC_MEDIA_INFO_BOX, &read_gmin_box},
};


static int add_json_from_type(struct mp4_to_json_param *param,
			      off_t *box_read_bytes,
			      const char key[5])
{
	char box_name[5];
	for (unsigned int i = 0; i < ARRAY_SIZE(box_type_map); i++) {
		if (box_type_map[i].func == NULL)
			continue;
		(void)uint_to_str(box_type_map[i].type, box_name);
		if (param->box.type == box_type_map[i].type ||
		    strncmp(key, box_name, 5) == 0) {
			return box_type_map[i].func(param, box_read_bytes);
		}
	}

	ULOGW("box not recognized (%s)", key);

	return skip_rest_of_box(param, box_read_bytes);
}


static int add_box_to_json(struct mp4_to_json_param *param)
{
	uint32_t val32;
	uint32_t size;
	off_t box_read_bytes = 0;
	uint64_t largesize = 0;
	char key[5];
	int ret = 0;
	uint32_t type;
	struct json_object *mp4_json_box;
	int offset = (int)lseek(param->file.fd, 0, SEEK_CUR);

	if (offset < 0) {
		ULOG_ERRNO("lseek", -errno);
		return -errno;
	}
	mp4_json_box = json_object_new_object();
	param->box.json = mp4_json_box;
	MP4_READ_32(param->file.fd, val32, box_read_bytes);
	size = ntohl(val32);
	json_object_object_add(
		param->box.json, "size", json_object_new_int64(size));

	MP4_READ_32(param->file.fd, val32, box_read_bytes);
	type = ntohl(val32);
	param->box.type = type;
	(void)uint_to_str(type, key);
	if (size == 0) {
		param->mp4.last_box = true;
	} else if (size == 1) {
		MP4_READ_32(param->file.fd, val32, box_read_bytes);
		largesize = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(param->file.fd, val32, box_read_bytes);
		largesize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		json_object_object_add(param->box.json,
				       "largesize",
				       json_object_new_int64(largesize));
		param->box.size = largesize;
	} else
		param->box.size = size;

	if (param->verbose) {
		for (uint32_t i = 0; i < param->parent.level; i++)
			printf("|   ");
		printf("<Type: %s - size: %" PRIu32 " - offset: %d>\n",
		       key,
		       size,
		       offset);
	}

	ret = add_json_from_type(param, &box_read_bytes, key);
	if (ret < 0) {
		ULOG_ERRNO("add_json_from_type", -ret);
		goto out;
	}

	param->file.read_bytes += box_read_bytes;

	if (type == MP4_TRACK_BOX)
		json_object_array_add(param->mp4.tracks, mp4_json_box);
	else
		json_object_object_add(param->parent.json, key, mp4_json_box);

out:
	return ret;
}


int mp4_file_to_json(const char *filename,
		     bool verbose,
		     struct json_object **json_obj)
{
	int ret = 0;
	struct stat st;
	struct mp4_to_json_param *param;

	ULOG_ERRNO_RETURN_ERR_IF(filename == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(filename) == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(json_obj == NULL, EINVAL);

	json_arr = NULL;
	param = calloc(1, sizeof(*param));
	if (param == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	param->verbose = verbose;
	param->file.fd = open(filename, O_RDONLY);
	if (param->file.fd == -1) {
		ret = -errno;
		ULOG_ERRNO("open (%s)", errno, filename);
		goto out;
	}

	ret = stat(filename, &st);
	if (ret < 0) {
		ret = -errno;
		ULOG_ERRNO("stat (%s)", errno, filename);
		goto out;
	}
	param->file.size = st.st_size;
	param->mp4.tracks = json_object_new_array();

	param->parent.type = MP4_ROOT_BOX;

	param->mp4.root = json_object_new_object();
	param->parent.json = param->mp4.root;

	while (!param->mp4.last_box &&
	       param->file.read_bytes + 8 < param->file.size) {
		ret = add_box_to_json(param);
		if (ret < 0) {
			ULOG_ERRNO("add_box_to_json", -ret);
			goto out;
		}
	}

	json_object_object_add(param->mp4.root, "tracks", param->mp4.tracks);
	*json_obj = param->mp4.root;
	param->mp4.root = NULL;

out:
	if (param == NULL)
		return ret;
	if (param->mp4.root != NULL)
		json_object_put(param->mp4.root);
	if (param->file.fd != -1)
		close(param->file.fd);

	clear_param_meta(param);
	free(param);
	return ret;
}
