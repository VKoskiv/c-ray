//
//  material.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 20/05/2017.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "material.h"

#include "../renderer/pathtrace.h"
#include "../datatypes/vertexbuffer.h"
#include "../datatypes/texture.h"

// TODO: Make this dynamic
static const uint64_t MATERIAL_TABLE_SIZE = 32;
struct material *newMaterial(enum materialType type) {
	struct material *self = malloc(sizeof(struct material));
	self->size = MATERIAL_TABLE_SIZE;
	self->data = malloc(sizeof(struct materialBucket) * MATERIAL_TABLE_SIZE);
	for (uint64_t i = 0; i < MATERIAL_TABLE_SIZE; ++i)
	{
		self->data[i].is_used = false;
		self->data[i].value = NULL;
		self->data[i].key = NULL;
	}

	self->type = type;
	return self;
}

void freeMaterial(struct material *self) {
	if (self == NULL) return;
	for (uint64_t i = 0; i < MATERIAL_TABLE_SIZE; ++i)
	{
		if(self->data[i].is_used) free(self->data[i].value);
	}
	free(self->data);
	free(self);
}

struct materialBucket *getMaterialBucketPtr(struct material *self, const char* key) {
	struct materialBucket* p_bucket;
	uint64_t index = hashDataToU64((uint8_t*)key, strlen(key)) % self->size;

	for (; index < self->size; ++index)
	{
		p_bucket = &self->data[index];

		if (p_bucket->is_used && p_bucket->key != NULL)
		{
			if (!strcmp(key, p_bucket->key)) return p_bucket;
		}
		else
		{
			return p_bucket;
		}
	}

	return NULL;
}

void setMaterialVec3(struct material *self, const char* key, vec3 value) {
	struct materialBucket *p_bucket = getMaterialBucketPtr(self, key);
	p_bucket->value = malloc(sizeof(vec3));
	p_bucket->is_used = true;
	*(vec3*)p_bucket->value = value;
}

void setMaterialFloat(struct material *self, const char *key, float value) {
	struct materialBucket* p_bucket = getMaterialBucketPtr(self, key);
	p_bucket->value = malloc(sizeof(float));
	p_bucket->is_used = true;
	*(float*)p_bucket->value = value;
}

float getMaterialFloat(struct material *self, const char *key) {
	return *(float*)getMaterialBucketPtr(self, key)->value;
}

vec3 getMaterialVec3(struct material *self, const char *key) {
	struct materialBucket* p_bucket = getMaterialBucketPtr(self, key);
	if (p_bucket->is_used)
		return *(vec3*)p_bucket->value;
	else
		return (vec3) { 0.0f, 0.0f, 0.0f };
}

void setMaterialColor(struct material* self, const char* key, color value) {
	struct materialBucket* p_bucket = getMaterialBucketPtr(self, key);
	p_bucket->value = malloc(sizeof(color));
	p_bucket->is_used = true;
	*(color*)p_bucket->value = value;
}

color getMaterialColor(struct material* self, const char* key) {
	struct materialBucket* p_bucket = getMaterialBucketPtr(self, key);
	if (p_bucket->is_used)
		return *(color*)p_bucket->value;
	else
		return (color) { 1.0f, 0.0f, 1.0f, 1.0f };
}

bool doesMaterialValueExist(struct material *self, const char *key) {
	return getMaterialBucketPtr(self, key)->is_used;
}

/*
vec3 colorForUV(struct intersection* isect) {
	struct color output = { 0.0,0.0,0.0,0.0 };
	struct material mtl = isect->end;
	struct poly p = polygonArray[isect->polyIndex];

	//Texture width and height for this material
	float width = *mtl.texture->width;
	float heigh = *mtl.texture->height;

	//barycentric coordinates for this polygon
	float u = isect->uv.x;
	float v = isect->uv.y;
	float w = 1.0 - u - v;

	//Weighted texture coordinates
	struct coord ucomponent = coordScale(u, textureArray[p.textureIndex[1]]);
	struct coord vcomponent = coordScale(v, textureArray[p.textureIndex[2]]);
	struct coord wcomponent = coordScale(w, textureArray[p.textureIndex[0]]);

	// textureXY = u * v1tex + v * v2tex + w * v3tex
	struct coord textureXY = addCoords(addCoords(ucomponent, vcomponent), wcomponent);

	float x = (textureXY.x * (width));
	float y = (textureXY.y * (heigh));

	//Get the color value at these XY coordinates
	output = textureGetPixelFiltered(mtl.texture, x, y);

	//Since the texture is probably srgb, transform it back to linear colorspace for rendering
	//FIXME: Maybe ask lodepng if we actually need to do this transform
	output = fromSRGB(output);

	return output;
}
*/
vec3 getAlbedo(struct material *p_mat) {
	if (doesMaterialValueExist(p_mat, "albedo"))
	{
		color albedoColor = getMaterialColor(p_mat, "albedo");
		return (vec3) { albedoColor.r, albedoColor.g, albedoColor.b };
	}
	else return (vec3) { 1.0f, 0.0f, 1.0f };
}


float square(float x) { return x * x; }
float getTheta(vec3 w) { return acos(w.z / vec3_length(w)); }
float getPhi(vec3 w) { return atan(w.y / w.x); }
float getCosPhi(vec3 w) { return cos(getPhi(w)); }
float getCosTheta(vec3 w) { return cos(getTheta(w)); }
float getCos2Theta(vec3 w) { return square(getCosTheta(w)); }
float getAbsTanTheta(vec3 w) { return fabs(tan(getTheta(w))); }
float getAbsCosTheta(vec3 w) { return fabs(getCosTheta(w)); }
float rsqrt(float x)
{
	return 1.0f / sqrt(x);
}

// Start defining BSDF functions

vec3 diffuseLambert(struct material *p_mat, vec3 wo, vec3 wi) {
	vec3 albedo = getAlbedo(p_mat);
	float dotNL = getCosTheta(wi);
	return vec3_muls(albedo, max(dotNL, 0.0) * INV_PI);
}

float EricHeitz2018GGXG1Lambda(vec3 V, float alpha_x, float alpha_y) {
	float Vx2 = square(V.x);
	float Vy2 = square(V.y);
	float Vz2 = square(V.z);
	float ax2 = square(alpha_x);
	float ay2 = square(alpha_y);
	return (-1.0f + sqrtf(1.0f + (Vx2 * ax2 + Vy2 * ay2) / Vz2)) / 2.0f;
}

float EricHeitz2018GGXG1(vec3 V, float alpha_x, float alpha_y) {
	return 1.0f / (1.0f + EricHeitz2018GGXG1Lambda(V, alpha_x, alpha_y));
}

// wm: microfacet normal in frame
float EricHeitz2018GGXD(vec3 N, float alpha_x, float alpha_y) {
	float Nx2 = square(N.x);
	float Ny2 = square(N.y);
	float Nz2 = square(N.z);
	float ax2 = square(alpha_x);
	float ay2 = square(alpha_y);
	return 1.0f / (M_PI * alpha_x * alpha_y * square(Nx2 / ax2 + Ny2 / ay2 + Nz2));
}


float EricHeitz2018GGXG2(vec3 V, vec3 L, float alpha_x, float alpha_y) {
	return EricHeitz2018GGXG1(V, alpha_x, alpha_y) * EricHeitz2018GGXG1(L, alpha_x, alpha_y);
}

vec3 SchlickFresnel(float NoX, vec3 F0) {
	return vec3_add(F0, vec3_muls(vec3_sub(VEC3_ONE, F0), pow(1.0f - NoX, 5.0f)));
}

// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers U(0, 1)
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 EricHeitz2018GGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2) {
	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = vec3_normalize((vec3) { alpha_x* Ve.x, alpha_y* Ve.y, Ve.z });
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0.0f ? vec3_muls((vec3) { -Vh.y, Vh.x, 0.0f }, rsqrt(lensq)) : (vec3) { 1.0f, 0.0f, 0.0f };
	vec3 T2 = vec3_cross(Vh, T1);
	// Section 4.2: parameterization of the projected area
	float r = sqrt(U1);
	float phi = 2.0f * M_PI * U2;
	float t1 = r * cosf(phi);
	float t2 = r * sinf(phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = (1.0f - s) * sqrtf(1.0f - t1 * t1) + s * t2;
	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = vec3_add(vec3_add(vec3_muls(T1, t1), vec3_muls(T2, t2)), vec3_muls(Vh, sqrtf(fmaxf(0.0f, 1.0f - t1 * t1 - t2 * t2))));
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = vec3_normalize((vec3) { alpha_x* Nh.x, alpha_y* Nh.y, fmaxf(0.0f, Nh.z) });
	return Ne;
}

float EricHeitz2018GGXDV(vec3 N, vec3 V, float alpha_x, float alpha_y) {
	float dotVZ = getCosTheta(V);
	float dotNV = vec3_dot(N, V);
	float G = EricHeitz2018GGXG1(V, alpha_x, alpha_y);
	float D = EricHeitz2018GGXD(N, alpha_x, alpha_y);
	return (G * fmaxf(dotNV, 0.0f) * D) / dotVZ;
}

float EricHeitz2018GGXPDF(vec3 V, vec3 Ni, vec3 Li, float alpha_x, float alpha_y) {
	float DV = EricHeitz2018GGXDV(Ni, V, alpha_x, alpha_y);
	float dotNiV = vec3_dot(V, Ni);
	return DV / (4.0f * dotNiV);
}
/*
float EricHeitz2018GGX_Rho(vec3 V, vec3 L, float alpha_x, float alpha_y, float ior)
{
	vec3 H = vec3_normalize(vec3_add(L, V));
	float cosThetaV = CosTheta(V);
	float cosThetaL = CosTheta(L);
	float VoH = vec3_dot(V, H);
	float F0 = abs((1.0f - ior) / (1.0f + ior));

	float D = EricHeitz2018GGXD(H, alpha_x, alpha_y);
	float F = SchlickFresnel(fmaxf(VoH, 0.0f), F0);
	float G = EricHeitz2018GGXG2(V, L, alpha_x, alpha_y);
	return (D * F * G) / (4.0f * cosThetaV * cosThetaL);
}
*/

vec3 specularEricHeitz2018GGX(struct material* p_mat, vec3 V, vec3* p_Li, pcg32_random_t* p_rng) {
	vec3 albedo = getAlbedo(p_mat);
	float roughness = getMaterialFloat(p_mat, "roughness");
	float anisotropy = getMaterialFloat(p_mat, "anisotropy");
	float metalness = getMaterialFloat(p_mat, "metalness");
	float ior = getMaterialFloat(p_mat, "ior");

	float alpha = roughness * roughness;
	float aspect = sqrt(1.0f - 0.9f * anisotropy);
	float alpha_x = alpha * aspect;
	float alpha_y = alpha / aspect;

	float U1 = rndFloat(0.0f, 1.0f, p_rng);
	float U2 = rndFloat(0.0f, 1.0f, p_rng);
	vec3 N = EricHeitz2018GGXVNDF(V, alpha_x, alpha_y, U1, U2);
	vec3 L = reflect(vec3_negate(V), N); // Li

	vec3 H = vec3_normalize(vec3_add(L, V));
	float dotVL = vec3_dot(V, L);

	float F0_ = abs((1.0f - ior) / (1.0f + ior));
	F0_ = F0_* F0_;
	vec3 F0 = vec3_mix((vec3) {F0_, F0_, F0_}, albedo, metalness);

	vec3 F = SchlickFresnel(fmaxf(dotVL, 0.0f), F0);
	float G2 = EricHeitz2018GGXG2(V, L, alpha_x, alpha_y);
	float G1 = EricHeitz2018GGXG1(V, alpha_x, alpha_y);

	vec3 I = vec3_divs(vec3_muls(F, G2), fmaxf(G1, 0.01f));

	*p_Li = L;

	return I;
}

vec3 diffuseEarlHammonGGX(struct material *p_mat, vec3 wo, vec3 wi) {
	float dotNV = getCosTheta(wo);
	float dotNL = getCosTheta(wi);

	// No light contribution if light or view isn't visible from surface
	if (dotNV <= 0.0f || dotNL <= 0.0f) return VEC3_ZERO;

	vec3 albedo = getAlbedo(p_mat);
	float roughness = getMaterialFloat(p_mat, "roughness");

	vec3 wm = vec3_normalize(vec3_add(wo, wi));

	float dotNH = getCosTheta(wm);
	float dotVL = vec3_dot(wo, wi);

	float alpha = roughness * roughness;

	float facing = 0.5f + 0.5f * dotVL;
	float roughy = facing * (0.9f - 0.4f * facing) * (0.5f + dotNH) / dotNH;
	float smoothy = 1.05f * (1.0f - powf(1.0f - dotNL, 5.0f)) * (1.0f - powf(1.0f - dotNV, 5.0f));
	float single = INV_PI * (smoothy * (1.0f - alpha) + alpha * roughy);
	float multi = alpha * 0.1159f;

	return vec3_mul(albedo, vec3_adds(vec3_muls(albedo, multi), single));
}

vec3 lightingFuncDiffuse(struct material *p_mat, vec3 wo, vec3 wi) {
	switch (p_mat->diffuseBSDF)
	{
		case DIFFUSE_BSDF_LAMBERT:
		{
			return diffuseLambert(p_mat, wo, wi);
		}

		case DIFFUSE_BSDF_EARL_HAMMON_GGX:
		{
			return diffuseEarlHammonGGX(p_mat, wo, wi);
		}
	}

	return (vec3) { 1.0f, 0.0f, 1.0f };
}

vec3 lightingFuncSpecular(struct material *p_mat, vec3 V, vec3* p_Li, pcg32_random_t* p_rng) {
	switch (p_mat->specularBSDF)
	{
		case SPECULAR_BSDF_ERIC_HEITZ_GGX_2018:
		{
			return specularEricHeitz2018GGX(p_mat, V, p_Li, p_rng);
		}
	}

	return (vec3) { 1.0f, 0.0f, 1.0f };
}

//bool LightingFunc(struct intersection* isect, vec3* attenuation, struct lightRay* scattered, pcg32_random_t* rng)
//{
//	if (isect->end->type == MATERIAL_TYPE_EMISSIVE) return false;
//
//	switch (isect->end->diffuse_bsdf_type)
//	{
//	case BSDF_TYPE_LAMBERT_DIFFUSE:
//	{
//		vec3 temp = vecAdd(isect->hitPoint, isect->surfaceNormal);
//		vec3 rand = RandomUnitSphere(rng);
//		vec3 target = vecAdd(temp, rand);
//		vec3 target2 = vecNormalize(vecSubtract(isect->hitPoint, target));
//		*scattered = ((struct lightRay) { isect->hitPoint, target2, rayTypeScattered, isect->end, 0 });
//		*attenuation = GetAlbedo(isect->end);
//
//		return true;
//	}
//	}
//
//	return false;
//}


//
//
//
////FIXME: Temporary, eventually support full OBJ spec
//struct material newMaterial(struct color diffuse, float reflectivity) {
//	struct material newMaterial = {0};
//	newMaterial.reflectivity = reflectivity;
//	newMaterial.diffuse = diffuse;
//	return newMaterial;
//}
//
//
//struct material newMaterialFull(struct color ambient,
//								struct color diffuse,
//								struct color specular,
//								float reflectivity,
//								float refractivity,
//								float IOR,
//								float transparency,
//								float sharpness,
//								float glossiness) {
//	struct material mat;
//	
//	mat.ambient = ambient;
//	mat.diffuse = diffuse;
//	mat.specular = specular;
//	mat.reflectivity = reflectivity;
//	mat.refractivity = refractivity;
//	mat.IOR = IOR;
//	mat.transparency = transparency;
//	mat.sharpness = sharpness;
//	mat.glossiness = glossiness;
//	
//	return mat;
//}
//
//struct material emptyMaterial() {
//	return (struct material){0};
//}
//
//struct material defaultMaterial() {
//	struct material newMat = emptyMaterial();
//	newMat.diffuse = grayColor;
//	newMat.reflectivity = 1.0;
//	newMat.type = lambertian;
//	newMat.IOR = 1.0;
//	return newMat;
//}
//
////To showcase missing .MTL file, for example
//struct material warningMaterial() {
//	struct material newMat = emptyMaterial();
//	newMat.type = lambertian;
//	newMat.diffuse = (struct color){1.0, 0.0, 0.5, 0.0};
//	return newMat;
//}
//
////Find material with a given name and return a pointer to it
//struct material *materialForName(struct material *materials, int count, char *name) {
//	for (int i = 0; i < count; i++) {
//		if (strcmp(materials[i].name, name) == 0) {
//			return &materials[i];
//		}
//	}
//	return NULL;
//}
//
//void assignBSDF(struct material *mat) {
//	//TODO: Add the BSDF weighting here
//	switch (mat->type) {
//		case lambertian:
//			mat->bsdf = lambertianBSDF;
//			break;
//		case metal:
//			mat->bsdf = metallicBSDF;
//			break;
//		case emission:
//			mat->bsdf = emissiveBSDF;
//			break;
//		case glass:
//			mat->bsdf = dialectricBSDF;
//			break;
//		default:
//			mat->bsdf = lambertianBSDF;
//			break;
//	}
//}
//
////Transform the intersection coordinates to the texture coordinate space
////And grab the color at that point. Texture mapping.
//struct color colorForUV(struct intersection *isect) {
//	struct color output = {0.0,0.0,0.0,0.0};
//	struct material mtl = isect->end;
//	struct poly p = polygonArray[isect->polyIndex];
//	
//	//Texture width and height for this material
//	float width = *mtl.texture->width;
//	float heigh = *mtl.texture->height;
//	
//	//barycentric coordinates for this polygon
//	float u = isect->uv.x;
//	float v = isect->uv.y;
//	float w = 1.0 - u - v;
//	
//	//Weighted texture coordinates
//	struct coord ucomponent = coordScale(u, textureArray[p.textureIndex[1]]);
//	struct coord vcomponent = coordScale(v, textureArray[p.textureIndex[2]]);
//	struct coord wcomponent = coordScale(w, textureArray[p.textureIndex[0]]);
//	
//	// textureXY = u * v1tex + v * v2tex + w * v3tex
//	struct coord textureXY = addCoords(addCoords(ucomponent, vcomponent), wcomponent);
//	
//	float x = (textureXY.x*(width));
//	float y = (textureXY.y*(heigh));
//	
//	//Get the color value at these XY coordinates
//	output = textureGetPixelFiltered(mtl.texture, x, y);
//	
//	//Since the texture is probably srgb, transform it back to linear colorspace for rendering
//	//FIXME: Maybe ask lodepng if we actually need to do this transform
//	output = fromSRGB(output);
//	
//	return output;
//}
//
//struct color gradient(struct intersection *isect) {
//	//barycentric coordinates for this polygon
//	float u = isect->uv.x;
//	float v = isect->uv.y;
//	float w = 1.0 - u - v;
//	
//	return colorWithValues(u, v, w, 1.0);
//}
//
////FIXME: Make this configurable
////This is a checkerboard pattern mapped to the surface coordinate space
//struct color mappedCheckerBoard(struct intersection *isect, float coef) {
//	struct poly p = polygonArray[isect->polyIndex];
//	
//	//barycentric coordinates for this polygon
//	float u = isect->uv.x;
//	float v = isect->uv.y;
//	float w = 1.0 - u - v; //1.0 - u - v
//	
//	//Weighted coordinates
//	struct coord ucomponent = coordScale(u, textureArray[p.textureIndex[1]]);
//	struct coord vcomponent = coordScale(v, textureArray[p.textureIndex[2]]);
//	struct coord wcomponent = coordScale(w,	textureArray[p.textureIndex[0]]);
//	
//	// textureXY = u * v1tex + v * v2tex + w * v3tex
//	struct coord surfaceXY = addCoords(addCoords(ucomponent, vcomponent), wcomponent);
//	
//	float sines = sin(coef*surfaceXY.x) * sin(coef*surfaceXY.y);
//	
//	if (sines < 0) {
//		return (struct color){0.4, 0.4, 0.4, 0.0};
//	} else {
//		return (struct color){1.0, 1.0, 1.0, 0.0};
//	}
//}
//
////FIXME: Make this configurable
////This is a spatial checkerboard, mapped to the world coordinate space (always axis aligned)
//struct color checkerBoard(struct intersection *isect, float coef) {
//	float sines = sin(coef*isect->hitPoint.x) * sin(coef*isect->hitPoint.y) * sin(coef*isect->hitPoint.z);
//	if (sines < 0) {
//		return (struct color){0.4, 0.4, 0.4, 0.0};
//	} else {
//		return (struct color){1.0, 1.0, 1.0, 0.0};
//	}
//}
//
///**
// Compute reflection vector from a given vector and surface normal
// 
// @param vec Incident ray to reflect
// @param normal Surface normal at point of reflection
// @return Reflected vector
// */
//vec3 reflectVec(const vec3 *incident, const vec3 *normal) {
//	float reflect = 2.0 * vecDot(*incident, *normal);
//	return vecSubtract(*incident, vecScale(reflect, *normal));
//}
//
//vec3 randomInUnitSphere(pcg32_random_t *rng) {
//	vec3 vec = (vec3){0.0, 0.0, 0.0};
//	do {
//		vec = vecMultiplyConst(vecWithPos(rndFloat(0, 1, rng), rndFloat(0, 1, rng), rndFloat(0, 1, rng)), 2.0);
//		vec = vecSubtract(vec, vecWithPos(1.0, 1.0, 1.0));
//	} while (vecLengthSquared(vec) >= 1.0);
//	return vec;
//}
//
//vec3 randomOnUnitSphere(pcg32_random_t *rng) {
//	vec3 vec = (vec3){0.0, 0.0, 0.0};
//	do {
//		vec = vecMultiplyConst(vecWithPos(rndFloat(0, 1, rng), rndFloat(0, 1, rng), rndFloat(0, 1, rng)), 2.0);
//		vec = vecSubtract(vec, vecWithPos(1.0, 1.0, 1.0));
//	} while (vecLengthSquared(vec) >= 1.0);
//	return vecNormalize(vec);
//}
//
//bool emissiveBSDF(struct intersection *isect, struct lightRay *ray, struct color *attenuation, struct lightRay *scattered, pcg32_random_t *rng) {
//	return false;
//}
//
//bool weightedBSDF(struct intersection *isect, struct lightRay *ray, struct color *attenuation, struct lightRay *scattered, pcg32_random_t *rng) {
//	
//	/*
//	 This will be the internal shader weighting solver that runs a random distribution and chooses from the available
//	 discrete shaders.
//	 */
//	
//	return false;
//}
//
////TODO: Make this a function ptr in the material?
//struct color diffuseColor(struct intersection *isect) {
//	if (isect->end.hasTexture) {
//		return colorForUV(isect);
//	} else {
//		return isect->end.diffuse;
//	}
//}
//
//bool lambertianBSDF(struct intersection *isect, struct lightRay *ray, struct color *attenuation, struct lightRay *scattered, pcg32_random_t *rng) {
//	vec3 temp = vecAdd(isect->hitPoint, isect->surfaceNormal);
//	vec3 rand = randomInUnitSphere(rng);
//	vec3 target = vecAdd(temp, rand);
//	vec3 target2 = vecSubtract(isect->hitPoint, target);
//	*scattered = ((struct lightRay){isect->hitPoint, target2, rayTypeScattered, isect->end, 0});
//	*attenuation = diffuseColor(isect);
//	return true;
//}
//
//bool metallicBSDF(struct intersection *isect, struct lightRay *ray, struct color *attenuation, struct lightRay *scattered, pcg32_random_t *rng) {
//	vec3 normalizedDir = vecNormalize(isect->ray.direction);
//	vec3 reflected = reflectVec(&normalizedDir, &isect->surfaceNormal);
//	//Roughness
//	if (isect->end.roughness > 0.0) {
//		vec3 fuzz = vecMultiplyConst(randomInUnitSphere(rng), isect->end.roughness);
//		reflected = vecAdd(reflected, fuzz);
//	}
//	
//	*scattered = newRay(isect->hitPoint, reflected, rayTypeReflected);
//	*attenuation = diffuseColor(isect);
//	return (vecDot(scattered->direction, isect->surfaceNormal) > 0);
//}
//
//bool refract(vec3 in, vec3 normal, float niOverNt, vec3 *refracted) {
//	vec3 uv = vecNormalize(in);
//	float dt = vecDot(uv, normal);
//	float discriminant = 1.0 - niOverNt * niOverNt * (1 - dt * dt);
//	if (discriminant > 0) {
//		vec3 A = vecMultiplyConst(normal, dt);
//		vec3 B = vecSubtract(uv, A);
//		vec3 C = vecMultiplyConst(B, niOverNt);
//		vec3 D = vecMultiplyConst(normal, sqrt(discriminant));
//		*refracted = vecSubtract(C, D);
//		return true;
//	} else {
//		return false;
//	}
//}
//
//float shlick(float cosine, float IOR) {
//	float r0 = (1 - IOR) / (1 + IOR);
//	r0 = r0*r0;
//	return r0 + (1 - r0) * pow((1 - cosine), 5);
//}
//
//// Only works on spheres for now. Reflections work but refractions don't
//bool dialectricBSDF(struct intersection *isect, struct lightRay *ray, struct color *attenuation, struct lightRay *scattered, pcg32_random_t *rng) {
//	vec3 outwardNormal;
//	vec3 reflected = reflectVec(&isect->ray.direction, &isect->surfaceNormal);
//	float niOverNt;
//	*attenuation = diffuseColor(isect);
//	vec3 refracted;
//	float reflectionProbability;
//	float cosine;
//	
//	if (vecDot(isect->ray.direction, isect->surfaceNormal) > 0) {
//		outwardNormal = vecNegate(isect->surfaceNormal);
//		niOverNt = isect->end.IOR;
//		cosine = isect->end.IOR * vecDot(isect->ray.direction, isect->surfaceNormal) / vecLength(isect->ray.direction);
//	} else {
//		outwardNormal = isect->surfaceNormal;
//		niOverNt = 1.0 / isect->end.IOR;
//		cosine = -(vecDot(isect->ray.direction, isect->surfaceNormal) / vecLength(isect->ray.direction));
//	}
//	
//	if (refract(isect->ray.direction, outwardNormal, niOverNt, &refracted)) {
//		reflectionProbability = shlick(cosine, isect->end.IOR);
//	} else {
//		*scattered = newRay(isect->hitPoint, reflected, rayTypeReflected);
//		reflectionProbability = 1.0;
//	}
//	
//	//Roughness
//	if (isect->end.roughness > 0.0) {
//		vec3 fuzz = vecMultiplyConst(randomInUnitSphere(rng), isect->end.roughness);
//		reflected = vecAdd(reflected, fuzz);
//		refracted = vecAdd(refracted, fuzz);
//	}
//	
//	if (rndFloat(0, 1, rng) < reflectionProbability) {
//		*scattered = newRay(isect->hitPoint, reflected, rayTypeReflected);
//	} else {
//		*scattered = newRay(isect->hitPoint, refracted, rayTypeRefracted);
//	}
//	return true;
//}
//
//void freeMaterial(struct material *mat) {
//	if (mat->textureFilePath) {
//		free(mat->textureFilePath);
//	}
//	if (mat->name) {
//		free(mat->name);
//	}
//}
