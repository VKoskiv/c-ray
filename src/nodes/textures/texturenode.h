//
//  texturenode.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 30/11/2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "../nodebase.h"

enum textureType {
	Diffuse,
	Normal,
	Specular
};

struct colorNode {
	struct nodeBase base;
	struct color (*eval)(const struct colorNode *node, const struct hitRecord *record);
};

#include "checker.h"
#include "constant.h"
#include "image.h"
#include "grayscale.h"

struct colorNode *unknownTextureNode(struct world *w);
