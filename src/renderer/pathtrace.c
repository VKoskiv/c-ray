//
//  pathtrace.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 27/04/2017.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "pathtrace.h"

#include "../datatypes/scene.h"
#include "../datatypes/camera.h"
#include "../acceleration/bbox.h"
#include "../acceleration/kdtree.h"
#include "../datatypes/texture.h"

bool GetHit(struct intersection* rec, struct lightRay *incidentRay, struct world *scene);
vec3 GetBackground(struct lightRay *incidentRay, struct world *scene);

vec3 RandomUnitSphere(pcg32_random_t* rng) {
	vec3 vec = (vec3){ 0.0f, 0.0f, 0.0f };
	do {
		vec = vecMultiplyConst(vecWithPos(rndFloat(0, 1, rng), rndFloat(0, 1, rng), rndFloat(0, 1, rng)), 2.0);
		vec = vecSubtract(vec, vecWithPos(1.0f, 1.0f, 1.0f));
	} while (vecLengthSquared(vec) >= 1.0f);
	return vec;
}


vec3 pathTrace(struct lightRay *incidentRay, struct world *scene, int maxDepth, pcg32_random_t *rng, bool *hasHitObject) {

	vec3 color = VEC3_ZERO;
	vec3 falloff = VEC3_ONE;

	for (int i = 0; i < maxDepth; ++i)
	{
		struct intersection rec;

		if (GetHit(&rec, incidentRay, scene))
		{
			vec3 wo, wi;
			if (hasHitObject) *hasHitObject = true;
			wi = vec3_negate(incidentRay->direction);

			incidentRay->start = rec.hitPoint;
			incidentRay->direction = vec3_normalize(vec3_sub(RandomUnitSphere(rng), rec.surfaceNormal));

			wo = incidentRay->direction;

			// TODO(saidwho12): transform wi and wo to tangent space using TBN inverse
			
			if (rec.end->type == MATERIAL_TYPE_EMISSIVE)
			{
				color = vec3_mul(falloff, GetAlbedo(rec.end));
				break;
			}
			if (rec.end->type == MATERIAL_TYPE_DEFAULT)
			{
				vec3 diffuse = LightingFuncDiffuse(rec.end, wo, wi);
				vec3 specular = LightingFuncSpecular(rec.end, wo, wi);

				float specularity = MaterialGetFloat(rec.end, "specularity");
				float metalness = MaterialGetFloat(rec.end, "metalness");

				falloff = vec3_mul(falloff, vec3_mix(diffuse, specular, specularity * metalness));
			}
		}
		else
		{
			color = vec3_mul(vec3_muls(GetBackground(incidentRay, scene), 4.0), falloff);
			break;
		}
	}

	return color;
}

//vec3 pathTrace(struct lightRay* incidentRay, struct world* scene, int depth, int maxDepth, pcg32_random_t* rng, bool* hasHitObject) {
//	struct intersection isect = getClosestIsect(incidentRay, scene);
//	if (isect.didIntersect) {
//		if (hasHitObject) *hasHitObject = true;
//		struct lightRay scattered;
//		vec3 attenuation;
//
//		IMaterial mat = isect.end;
//		vec3 emitted = (mat->type == MATERIAL_TYPE_EMISSIVE && MaterialValueAt(mat, "albedo")) ? MaterialGetVec3(mat, "albedo") : VEC3_ZERO;
//
//		if (depth < maxDepth && LightingFunc(&isect, &attenuation, &scattered, rng)) {
//			float probability = 1;
//			if (depth >= 2) {
//				probability = max(attenuation.r, max(attenuation.g, attenuation.b));
//				if (rndFloat(0, 1, rng) > probability) {
//					return emitted;
//				}
//			}
//			vec3 newColor = pathTrace(&scattered, scene, depth + 1, maxDepth, rng, hasHitObject);
//			return vec3_muls(vec3_add(emitted, vec3_mul(attenuation, newColor)), 1.0 / probability);
//		}
//		else {
//			return emitted;
//		}
//	}
//	else {
//		return getBackground(incidentRay, scene);
//	}
//}

/**
 Calculate the closest intersection point, and other relevant information based on a given lightRay and scene
 See the intersection struct for documentation of what this function calculates.

 @param incidentRay Given light ray (set up in renderThread())
 @param scene  Given scene to cast that ray into
 @return intersection struct with the appropriate values set
 */
bool GetHit(struct intersection *rec, struct lightRay *incidentRay, struct world *scene) {
	rec->distance = 20000.0;
	rec->ray = *incidentRay;
	rec->start = incidentRay->currentMedium;
	rec->didIntersect = false;
	for (int i = 0; i < scene->sphereCount; i++) {
		if (rayIntersectsWithSphere(&scene->spheres[i], incidentRay, rec)) {
			rec->end = scene->spheres[i].material;
			rec->didIntersect = true;
		}
	}
	for (int o = 0; o < scene->meshCount; o++) {
		if (rayIntersectsWithNode(scene->meshes[o].tree, incidentRay, rec)) {
			rec->end = scene->meshes[o].materials[polygonArray[rec->polyIndex].materialIndex];
			rec->didIntersect = true;
		}
	}
	return rec->didIntersect;
}

float wrapMax(float x, float max) {
    return fmod(max + fmod(x, max), max);
}

float wrapMinMax(float x, float min, float max) {
    return min + wrapMax(x - min, max - min);
}

vec3 getHDRI(struct lightRay *incidentRay, struct world *scene) {
	//Unit direction vector
	vec3 ud = vecNormalize(incidentRay->direction);
	
	//To polar from cartesian
	float r = 1.0f; //Normalized above
	float phi = (atan2f(ud.z, ud.x)/4) + scene->hdr->offset;
	float theta = acosf((-ud.y/r));
	
	float u = theta / PI;
	float v = (phi / (PI/2));
	
	u = wrapMinMax(u, 0, 1);
	v = wrapMinMax(v, 0, 1);
	
	float x =  (v * *scene->hdr->width);
	float y = (u * *scene->hdr->height);
	
	vec3 newColor = textureGetPixelFiltered(scene->hdr, x, y);
	
	return newColor;
}

//Linearly interpolate based on the Y component
vec3 getAmbientColor(struct lightRay *incidentRay, struct gradient *color) {
	vec3 unitDirection = vecNormalize(incidentRay->direction);
	float t = 0.5 * (unitDirection.y + 1.0);
	return vec3_add(vec3_muls((*color).down, 1.0 - t), vec3_muls((*color).up, t));
}

vec3 GetBackground(struct lightRay *incidentRay, struct world *scene) {
	return scene->hdr ? getHDRI(incidentRay, scene) : getAmbientColor(incidentRay, scene->ambientColor);
}
