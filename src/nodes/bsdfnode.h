//
//  bsdfnode.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 29/11/2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "../datatypes/lightRay.h"
#include "../utils/mempool.h"
#include "valuenode.h"
#include "vectornode.h"
#include "colornode.h"
#include "../datatypes/hitrecord.h"
#include "nodebase.h"

struct bsdfSample {
	struct vector out;
	float pdf;
	struct color color;
};

//TODO: Expand and refactor to match a standard bsdf signature with eval, sample and pdf
struct bsdf {
	struct nodeBase base;
	struct bsdfSample (*sample)(const struct bsdf *bsdf, sampler *sampler, const struct hitRecord *record);
};

#include "shaders/diffuse.h"
#include "shaders/glass.h"
#include "shaders/metal.h"
#include "shaders/mix.h"
#include "shaders/plastic.h"
#include "shaders/transparent.h"

struct bsdf *warningBsdf(struct world *world);
