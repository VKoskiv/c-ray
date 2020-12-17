//
//  metal.c
//  C-Ray
//
//  Created by Valtteri Koskivuori on 30/11/2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#include "../../includes.h"
#include "../../datatypes/color.h"
#include "../../renderer/samplers/sampler.h"
#include "../../datatypes/vector.h"
#include "../../datatypes/material.h"
#include "../../utils/hashtable.h"
#include "../../datatypes/scene.h"
#include "../bsdfnode.h"

#include "metal.h"

struct metalBsdf {
	struct bsdfNode bsdf;
	struct colorNode *color;
	struct colorNode *roughness;
};

struct bsdfSample sampleMetal(const struct bsdfNode *bsdf, sampler *sampler, const struct hitRecord *record) {
	struct metalBsdf *metalBsdf = (struct metalBsdf*)bsdf;
	
	const struct vector normalizedDir = vecNormalize(record->incident.direction);
	struct vector reflected = reflectVec(&normalizedDir, &record->surfaceNormal);
	float roughness = metalBsdf->roughness->eval(metalBsdf->roughness, record).red;
	if (roughness > 0.0f) {
		const struct vector fuzz = vecScale(randomOnUnitSphere(sampler), roughness);
		reflected = vecAdd(reflected, fuzz);
	}
	
	return (struct bsdfSample){
		.out = reflected,
		.color = metalBsdf->color->eval(metalBsdf->color, record)
	};
}

static bool compare(const void *A, const void *B) {
	const struct metalBsdf *this = A;
	const struct metalBsdf *other = B;
	return this->color == other->color && this->roughness == other->roughness;
}

static uint32_t hash(const void *p) {
	const struct metalBsdf *this = p;
	uint32_t h = hashInit();
	h = hashBytes(h, &this->color, sizeof(this->color));
	h = hashBytes(h, &this->roughness, sizeof(this->roughness));
	return h;
}

struct bsdfNode *newMetal(struct world *world, struct colorNode *color, struct colorNode *roughness) {
	HASH_CONS(world->nodeTable, &world->nodePool, hash, struct metalBsdf, {
		.color = color ? color : newConstantTexture(world, blackColor),
		.roughness = roughness ? roughness : newConstantTexture(world, blackColor),
		.bsdf = {
			.sample = sampleMetal,
			.base = { .compare = compare }
		}
	});
}
