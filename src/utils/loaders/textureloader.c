//
//  textureloader.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 02/04/2019.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#include "../../includes.h"
#include "textureloader.h"
#include "../fileio.h"
#include "../logging.h"
#include "../../datatypes/image/texture.h"
#include "../../datatypes/color.h"
#include "../../utils/assert.h"

#define STBI_NO_PSD
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "../../libraries/stb_image.h"

static struct texture *loadEnvMap(unsigned char *buf, size_t buflen, const char *path) {
	ASSERT(buf);
	logr(info, "Loading HDR...");
	struct texture *tex = newTexture(none, 0, 0, 0);
	tex->data.float_p = stbi_loadf_from_memory(buf, (int)buflen, (int *)&tex->width, (int *)&tex->height, (int *)&tex->channels, 0);
	tex->precision = float_p;
	if (!tex->data.float_p) {
		destroyTexture(tex);
		logr(warning, "Error while decoding HDR from %s - Corrupted?\n", path);
		return NULL;
	}
	char *fsbuf = humanFileSize(buflen);
	printf(" %s\n", fsbuf);
	free(fsbuf);
	return tex;
}

struct texture *loadTexture(char *filePath) {
	size_t len = 0;
	//Handle the trailing newline here
	filePath[strcspn(filePath, "\n")] = 0;
	unsigned char *file = (unsigned char*)loadFile(filePath, &len);
	if (!file) return NULL;
	struct texture *new = NULL;
	if (stbi_is_hdr(filePath)) {
		new = loadEnvMap(file, len, filePath);
	} else {
		new = loadTextureFromBuffer(file, (unsigned int)len);
	}
	free(file);
	if (!new) {
		logr(warning, "^That happened while decoding texture \"%s\" - Corrupted?\n", filePath);
		destroyTexture(new);
		return NULL;
	}
	
	return new;
}

struct texture *loadTextureFromBuffer(const unsigned char *buffer, const unsigned int buflen) {
	struct texture *new = newTexture(none, 0, 0, 0);
	new->data.byte_p = stbi_load_from_memory(buffer, buflen, (int *)&new->width, (int *)&new->height, (int *)&new->channels, 0);
	if (!new->data.byte_p) {
		logr(warning, "Failed to decode texture from memory buffer of size %u", buflen);
		destroyTexture(new);
		return NULL;
	}
	if (new->channels > 3) {
		new->hasAlpha = true;
	}
	new->precision = char_p;
	return new;
}
