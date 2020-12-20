//
//  vectornode.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 16/12/2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "nodebase.h"

struct vectorValue {
	struct vector v;
	struct coord c;
};

struct vectorNode {
	struct nodeBase base;
	struct vectorValue (*eval)(const struct vectorNode *node, const struct hitRecord *record);
};

#include "input/normal.h"

const struct vectorNode *newConstantVector(const struct world *world, struct vector vector);
