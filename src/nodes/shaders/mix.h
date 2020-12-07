//
//  mix.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 30/11/2020.
//  Copyright © 2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

struct bsdf *newMix(struct world *world, struct bsdf *A, struct bsdf *B, struct textureNode *lerp);

struct bsdf *newMixConstant(struct world *world, struct bsdf *A, struct bsdf *B, float mix);
