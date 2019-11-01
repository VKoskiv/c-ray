//
//  texture.h
//  C-ray
//
//  Created by Valtteri on 09/04/2019.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

enum fileType {
	bmp,
	png,
	hdr,
	buffer
};

struct dimensions {
	int height;
	int width;
};

enum colorspace {
	linear,
	sRGB
};

enum precision {
	char_p,
	float_p
};

struct texture {
	bool hasAlpha;
	enum fileType fileType;
	enum colorspace colorspace;
	enum precision precision;
	char *filePath;
	char *fileName;
	int count;
	unsigned char *byte_data; //For 24/32bit
	float *float_data; //For hdr
	int *channels; //For hdr
	float offset; //radians, for hdr
	unsigned int *width;
	unsigned int *height;
};

struct color;

void blit(struct texture *t, color c, unsigned int x, unsigned int y);
void blitfloat(float *buf, int width, int height, color*c, unsigned int x, unsigned int y);
color textureGetPixel(struct texture *t, int x, int y);
color textureGetPixelFiltered(struct texture *t, float x, float y);

struct texture *newTexture(void);
void allocTextureBuffer(struct texture *t, enum precision p, int width, int height, int channels);

void textureFromSRGB(struct texture *t);
void textureToSRGB(struct texture *t);

void freeTexture(struct texture *tex);
