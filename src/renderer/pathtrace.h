//
//  pathtrace.h
//  C-ray
//
//  Created by Valtteri Koskivuori on 27/04/2017.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "../datatypes/vector.h"
#include "../datatypes/poly.h"
#include "../datatypes/lightRay.h"
#include "../datatypes/material.h"

struct world;

//FIXME: These datatypes should be hidden inside the implementation!

/**
 Shading/intersection information, used to perform shading and rendering logic.
 @note uv, mtlIndex and polyIndex are only set if the ray hits a polygon (mesh)
 @todo Consider moving start, end materials to lightRay instead
 */
struct hitRecord {
	struct lightRay incident;		//Light ray that encountered this intersection
	struct material material;	    //Material of the intersected object
	struct vector hitPoint;			//Hit point vector in 3D space
	struct vector surfaceNormal;	//Surface normal at that point of intersection
	struct coord uv;				//UV barycentric coordinates for intersection point
	float distance;					//Distance to intersection point
	struct poly *polygon;			//ptr to polygon that was encountered
	int instIndex;                  //Instance index, negative if no intersection
};


/// Recursive path tracer.
/// @param incidentRay View ray to be casted into the scene
/// @param scene Scene to cast the ray into
/// @param maxDepth Maximum depth of recursion
/// @param rng A random number generator. One per execution thread.
struct color pathTrace(const struct lightRay *incidentRay, const struct world *scene, int maxDepth, sampler *sampler);
