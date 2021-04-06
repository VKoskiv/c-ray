//
//  texture.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 09/04/2019.
//  Copyright © 2019-2020 Valtteri Koskivuori. All rights reserved.
//

#include "../../includes.h"

#include "texture.h"
#include "../../utils/logging.h"
#include "../../utils/assert.h"

//General-purpose setPixel function
void setPixel(struct texture *t, struct color c, size_t x, size_t y) {
	ASSERT(x < t->width); ASSERT(y < t->height);
	if (t->precision == char_p) {
		t->data.byte_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 0] = (unsigned char)min(c.red * 255.0f, 255.0f);
		t->data.byte_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 1] = (unsigned char)min(c.green * 255.0f, 255.0f);
		t->data.byte_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 2] = (unsigned char)min(c.blue * 255.0f, 255.0f);
		if (t->hasAlpha) t->data.byte_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 3] = (unsigned char)min(c.alpha * 255.0f, 255.0f);
	}
	else if (t->precision == float_p) {
		t->data.float_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 0] = c.red;
		t->data.float_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 1] = c.green;
		t->data.float_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 2] = c.blue;
		if (t->hasAlpha) t->data.float_p[(x + (t->height - (y + 1)) * t->width) * t->channels + 3] = c.alpha;
	}
}

static struct color textureGetPixelInternal(const struct texture *t, size_t x, size_t y) {
	struct color output = {0.0f, 0.0f, 0.0f, 0.0f};
	x = x % t->width;
	y = y % t->height;
	
	if (t->channels == 1) {
		if (t->precision == float_p) {
			output.red   = t->data.float_p[(x + ((t->height - 1) - y) * t->width) * t->channels];
			output.green = output.red;
			output.blue  = output.red;
			output.alpha = 1.0f;
		} else {
			output.red =   t->data.byte_p[(x + ((t->height - 1) - y) * t->width) * t->channels] / 255.0f;
			output.green = output.red;
			output.blue =  output.red;
			output.alpha = 1.0f;
		}
	} else {
		if (t->precision == float_p) {
			output.red   = t->data.float_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 0];
			output.green = t->data.float_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 1];
			output.blue  = t->data.float_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 2];
			output.alpha = t->hasAlpha ? t->data.float_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 3] : 1.0f;
		} else {
			output.red =   t->data.byte_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 0] / 255.0f;
			output.green = t->data.byte_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 1] / 255.0f;
			output.blue =  t->data.byte_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 2] / 255.0f;
			output.alpha = t->hasAlpha ? t->data.byte_p[(x + ((t->height - 1) - y) * t->width) * t->channels + 3] / 255.0f : 1.0f;
		}
	}
	return output;
}

//FIXME: This API is confusing. The semantic meaning of x and y change completely based on the filtered flag.
struct color textureGetPixel(const struct texture *t, float x, float y, bool filtered) {
	if (!filtered) return textureGetPixelInternal(t, (size_t)x, (size_t)y);
	x = x * t->width;
	y = y * t->height;
	float xcopy = x - 0.5f;
	float ycopy = y - 0.5f;
	int xint = (int)xcopy;
	int yint = (int)ycopy;
	struct color topleft = textureGetPixelInternal(t, xint, yint);
	struct color topright = textureGetPixelInternal(t, xint + 1, yint);
	struct color botleft = textureGetPixelInternal(t, xint, yint + 1);
	struct color botright = textureGetPixelInternal(t, xint + 1, yint + 1);
	return lerp(lerp(topleft, topright, xcopy - xint), lerp(botleft, botright, xcopy - xint), ycopy - yint);
}

struct texture *newTexture(enum precision p, size_t width, size_t height, size_t channels) {
	struct texture *t = calloc(1, sizeof(*t));
	t->width = width;
	t->height = height;
	t->precision = p;
	t->channels = channels;
	t->hasAlpha = false;
	t->data.byte_p = NULL;
	t->data.float_p = NULL;
	t->colorspace = linear;
	if (channels > 3) {
		t->hasAlpha = true;
	}
	
	switch (t->precision) {
		case char_p: {
			t->data.byte_p = calloc(channels * width * height, sizeof(*t->data.byte_p));
			if (!t->data.byte_p) {
				logr(warning, "Failed to allocate %zux%zu texture.\n", width, height);
				destroyTexture(t);
				return NULL;
			}
		}
			break;
		case float_p: {
			t->data.float_p = calloc(channels * width * height, sizeof(*t->data.float_p));
			if (!t->data.float_p) {
				logr(warning, "Failed to allocate %zux%zu texture.\n", width, height);
				destroyTexture(t);
				return NULL;
			}
		}
			break;
		default:
			break;
	}
	return t;
}

void textureFromSRGB(struct texture *t) {
	if (t->colorspace == sRGB) return;
	for (unsigned x = 0; x < t->width; ++x) {
		for (unsigned y = 0; y < t->height; ++y) {
			setPixel(t, fromSRGB(textureGetPixel(t, x, y, false)), x, y);
		}
	}
	t->colorspace = linear;
}

void textureToSRGB(struct texture *t) {
	if (t->colorspace == linear) return;
	for (unsigned x = 0; x < t->width; ++x) {
		for (unsigned y = 0; y < t->height; ++y) {
			setPixel(t, toSRGB(textureGetPixel(t, x, y, false)), x, y);
		}
	}
	t->colorspace = sRGB;
}

struct texture *flipHorizontal(struct texture *t) {
	struct texture *new = newTexture(t->precision, t->width, t->height, t->channels);
	new->colorspace = t->colorspace;
	
	for (size_t y = 0; y < new->height; ++y) {
		for (size_t x = 0; x < new->width; ++x) {
			setPixel(new, textureGetPixel(t, ((t->width - 1) - x), y, false), x, y);
		}
	}
	
	destroyTexture(t);
	return new;
}
void destroyTexture(struct texture *t) {
	if (t) {
		free(t->data.byte_p);
		free(t);
		t = NULL;
	}
}
