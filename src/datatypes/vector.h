//
//  vector.h
//  C-ray
//
//  Created by Valtteri Koskivuori on 28/02/2015.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include <math.h>
#include "../renderer/samplers/sampler.h"
#include "../utils/assert.h"

struct vector {
	float x, y, z;
};

typedef struct vector vector;

struct base {
	vector i, j, k;
};

struct coord {
	float x, y;
};

struct intCoord {
	int x, y;
};

/**
 Create a vector with given position values and return it.

 @param x X component
 @param y Y component
 @param z Z component
 @return Vector with given values
 */
static inline struct vector vecWithPos(float x, float y, float z) {
	return (struct vector){x, y, z};
}

//For defaults
static inline struct vector vecZero() {
	return (struct vector){0.0f, 0.0f, 0.0f};
}

/**
 Add two vectors and return the resulting vector

 @param v1 Vector 1
 @param v2 Vector 2
 @return Resulting vector
 */
static inline struct vector vecAdd(struct vector v1, struct vector v2) {
	return (struct vector){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

/**
 Subtract a vector from another and return the resulting vector

 @param v1 Vector to be subtracted from
 @param v2 Vector to be subtracted
 @return Resulting vector
 */
static inline struct vector vecSub(const struct vector v1, const struct vector v2) {
	return (struct vector){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

static inline struct vector vecMul(struct vector v1, struct vector v2) {
	return (struct vector){v1.x * v2.x, v1.y * v2.y, v1.z * v2.z};
}

/**
 Return the dot product of two vectors

 @param v1 Vector 1
 @param v2 Vector 2
 @return Resulting scalar
 */
static inline float vecDot(const struct vector v1, const struct vector v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/**
 Multiply a vector by a given scalar and return the resulting vector

 @param c Scalar to multiply the vector by
 @param v Vector to be multiplied
 @return Multiplied vector
 */
static inline struct vector vecScale(const struct vector v, const float c) {
	return (struct vector){v.x * c, v.y * c, v.z * c};
}

static inline struct coord coordScale(const float c, const struct coord crd) {
	return (struct coord){crd.x * c, crd.y * c};
}

static inline struct coord addCoords(const struct coord c1, const struct coord c2) {
	return (struct coord){c1.x + c2.x, c1.y + c2.y};
}

/**
 Calculate cross product and return the resulting vector

 @param v1 Vector 1
 @param v2 Vector 2
 @return Cross product of given vectors
 */
static inline struct vector vecCross(struct vector v1, struct vector v2) {
	return (struct vector){ ((v1.y * v2.z) - (v1.z * v2.y)),
							((v1.z * v2.x) - (v1.x * v2.z)),
							((v1.x * v2.y) - (v1.y * v2.x))
	};
}

/**
 Return a vector containing the smallest components of given vectors

 @param v1 Vector 1
 @param v2 Vector 2
 @return Smallest vector
 */
static inline struct vector vecMin(struct vector v1, struct vector v2) {
	return (struct vector){min(v1.x, v2.x), min(v1.y, v2.y), min(v1.z, v2.z)};
}

/**
 Return a vector containing the largest components of given vectors

 @param v1 Vector 1
 @param v2 Vector 2
 @return Largest vector
 */
static inline struct vector vecMax(struct vector v1, struct vector v2) {
	return (struct vector){max(v1.x, v2.x), max(v1.y, v2.y), max(v1.z, v2.z)};
}

//calculate length^2 of vector
static inline float vecLengthSquared(struct vector v) {
    return vecDot(v, v);
}

/**
 Compute the length of a vector

 @param v Vector to compute the length for
 @return Length of given vector
 */
static inline float vecLength(struct vector v) {
	return sqrtf(vecLengthSquared(v));
}

/**
 Normalize a given vector

 @param v Vector to normalize
 @todo Consider having this one void and as a reference type
 @return normalized vector
 */
static inline struct vector vecNormalize(struct vector v) {
	float length = vecLength(v);
	return (struct vector){v.x / length, v.y / length, v.z / length};
}

/**
 Get the mid-point for three given vectors

 @param v1 Vector 1
 @param v2 Vector 2
 @param v3 Vector 3
 @return Mid-point of given vectors
 */
static inline struct vector getMidPoint(struct vector v1, struct vector v2, struct vector v3) {
	return vecScale(vecAdd(vecAdd(v1, v2), v3), 1.0f/3.0f);
}

static inline float rndFloatRange(float min, float max, sampler *sampler) {
	return ((getDimension(sampler)) * (max - min)) + min;
}

struct vector getRandomVecOnRadius(struct vector center, float radius, pcg32_random_t *rng);

struct vector getRandomVecOnPlane(struct vector center, float radius, pcg32_random_t *rng);

static inline struct coord randomCoordOnUnitDisc(sampler *sampler) {
	float r = sqrtf(getDimension(sampler));
	float theta = rndFloatRange(0.0f, 2.0f * PI, sampler);
	return (struct coord){r * cosf(theta), r * sinf(theta)};
}

float rndFloat(pcg32_random_t *rng);

static inline struct vector vecNegate(struct vector v) {
	return (struct vector){-v.x, -v.y, -v.z};
}

/**
Returns the reflected ray vector from a surface

@param I Incident vector normalized
@param N Normal vector normalized
@return Vector of the reflected ray vector from a surface
*/
static inline struct vector vecReflect(const struct vector I, const struct vector N) {
	return vecSub(I, vecScale(N, vecDot(N, I) * 2.0f));
}

static inline float wrapMax(float x, float max) {
	return fmodf(max + fmodf(x, max), max);
}

static inline float wrapMinMax(float x, float min, float max) {
	return min + wrapMax(x - min, max - min);
}

//Compute two orthonormal vectors for this unit vector
//PBRT
static inline struct base baseWithVec(struct vector i) {
	ASSERT(vecLength(i) == 1.0f);
	struct base newBase;
	newBase.i = i;
	if (fabsf(i.x) > fabsf(i.y)) {
		float len = sqrtf(i.x * i.x + i.z * i.z);
		newBase.j = (vector){-i.z / len, 0.0f / len, i.x / len};
	} else {
		float len = sqrtf(i.y * i.y + i.z * i.z);
		newBase.j = (vector){ 0.0f / len, i.z / len, -i.y / len};
	}
	ASSERT(vecDot(newBase.i, newBase.j) == 0.0f);
	newBase.k = vecCross(newBase.i, newBase.j);
	return newBase;
}

