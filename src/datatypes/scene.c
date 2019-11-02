//
//  scene.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 28/02/2015.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "scene.h"

#include "camera.h"
#include "../acceleration/kdtree.h"
#include "../utils/filehandler.h"
#include "../utils/converter.h"
#include "../renderer/renderer.h"
#include "../libraries/cJSON.h"
#include "../utils/ui.h"
#include "../utils/logging.h"
#include "tile.h"
#include "../datatypes/vertexbuffer.h"
#include "../utils/loaders/objloader.h"
#include "../datatypes/texture.h"
#include "../utils/loaders/textureloader.h"
#include "../utils/multiplatform.h"
#include "../utils/timer.h"
#include "vec3.h"

/*struct renderer defaultSettings = (struct renderer){
	.prefs = {
		.fileMode = saveModeNormal,
		.tileOrder = renderOrderFromMiddle,
		.threadCount = 4,
		.sampleCount = 25,
		.bounces = 50,
		.tileWidth = 32,
		.tileHeight = 32,
		.antialiasing = true
	}
};*/

color *parseColor(const cJSON *data);

/**
 Extract the filename from a given file path

 @param input File path to be processed
 @return Filename string, including file type extension
 */
char *getFileName(char *input) {
	char *fn;
	
	/* handle trailing '/' e.g.
	 input == "/home/me/myprogram/" */
	if (input[(strlen(input) - 1)] == '/')
		input[(strlen(input) - 1)] = '\0';
	
	(fn = strrchr(input, '/')) ? ++fn : (fn = input);
	
	return fn;
}

void addMaterialToMesh(struct mesh *mesh, struct material * newMaterial);

struct mesh *lastMesh(struct renderer *r) {
	return &r->scene->meshes[r->scene->meshCount - 1];
}

struct sphere *lastSphere(struct renderer *r) {
	return &r->scene->spheres[r->scene->sphereCount - 1];
}

void loadMeshTextures(struct mesh *mesh) {
	//for (int i = 0; i < mesh->materialCount; i++) {
	//	//FIXME: do this check in materialFromOBJ and just check against hasTexture here
	//	if (mesh->materials[i].textureFilePath) {
	//		if (strcmp(mesh->materials[i].textureFilePath, "")) {
	//			//TODO: Set the shader for this obj to an obnoxious checker pattern if the texture wasn't found
	//			mesh->materials[i].texture = loadTexture(mesh->materials[i].textureFilePath);
	//			if (mesh->materials[i].texture) {
	//				mesh->materials[i].hasTexture = true;
	//			}
	//		} else {
	//			mesh->materials[i].hasTexture = false;
	//		}
	//	} else {
	//		mesh->materials[i].hasTexture = false;
	//	}
	//	
	//}
}

bool loadMeshNew(struct renderer *r, char *inputFilePath) {
	logr(info, "Loading mesh %s\n", inputFilePath);
	
	r->scene->meshes = realloc(r->scene->meshes, (r->scene->meshCount + 1) * sizeof(struct mesh));
	
	bool valid = false;
	
	struct mesh *newMesh = parseOBJFile(inputFilePath);
	if (newMesh != NULL) {
		r->scene->meshes[r->scene->meshCount] = *newMesh;
		free(newMesh);
		valid = true;
		loadMeshTextures(&r->scene->meshes[r->scene->meshCount]);
	}
	
	r->scene->meshCount++;
	return valid;
}

bool loadMesh(struct renderer *r, char *inputFilePath, int idx, int meshCount) {
	logr(info, "Loading mesh %i/%i\r", idx, meshCount);
	
	obj_scene_data data;
	if (parse_obj_scene(&data, inputFilePath) == 0) {
		printf("\n");
		logr(warning, "Mesh %s not found!\n", getFileName(inputFilePath));
		return false;
	}
	
	//Create mesh to keep track of meshes
	r->scene->meshes = realloc(r->scene->meshes, (r->scene->meshCount + 1) * sizeof(struct mesh));
	//Vertex data
	r->scene->meshes[r->scene->meshCount].firstVectorIndex = vertexCount;
	r->scene->meshes[r->scene->meshCount].vertexCount = data.vertex_count;
	//Normal data
	r->scene->meshes[r->scene->meshCount].firstNormalIndex = normalCount;
	r->scene->meshes[r->scene->meshCount].normalCount = data.vertex_normal_count;
	//Texture vector data
	r->scene->meshes[r->scene->meshCount].firstTextureIndex = textureCount;
	r->scene->meshes[r->scene->meshCount].textureCount = data.vertex_texture_count;
	//Poly data
	r->scene->meshes[r->scene->meshCount].firstPolyIndex = polyCount;
	r->scene->meshes[r->scene->meshCount].polyCount = data.face_count;
	//Transforms init
	r->scene->meshes[r->scene->meshCount].transformCount = 0;
	r->scene->meshes[r->scene->meshCount].transforms = malloc(sizeof(struct transform));
	
	//r->scene->meshes[r->scene->meshCount].materialCount = 0;
	//Set name
	copyString(getFileName(inputFilePath), &r->scene->meshes[r->scene->meshCount].name);
	
	//Update vector and poly counts
	vertexCount += data.vertex_count;
	normalCount += data.vertex_normal_count;
	textureCount += data.vertex_texture_count;
	polyCount += data.face_count;
	
	//Data loaded, now convert everything
	//Convert vectors
	vertexArray = realloc(vertexArray, vertexCount * sizeof(vec3));
	for (int i = 0; i < data.vertex_count; i++) {
		vertexArray[r->scene->meshes[r->scene->meshCount].firstVectorIndex + i] = vec3FromObj(data.vertex_list[i]);
	}
	
	//Convert normals
	normalArray = realloc(normalArray, normalCount * sizeof(vec3));
	for (int i = 0; i < data.vertex_normal_count; i++) {
		normalArray[r->scene->meshes[r->scene->meshCount].firstNormalIndex + i] = vec3FromObj(data.vertex_normal_list[i]);
	}
	//Convert texture vectors
	textureArray = realloc(textureArray, textureCount * sizeof(vec2));
	for (int i = 0; i < data.vertex_texture_count; i++) {
		textureArray[r->scene->meshes[r->scene->meshCount].firstTextureIndex + i] = vec2FromObj(data.vertex_texture_list[i]);
	}
	//Convert polygons
	polygonArray = realloc(polygonArray, polyCount * sizeof(struct poly));
	for (int i = 0; i < data.face_count; i++) {
		polygonArray[r->scene->meshes[r->scene->meshCount].firstPolyIndex + i] = polyFromObj(data.face_list[i],
																							r->scene->meshes[r->scene->meshCount].firstVectorIndex,
																							r->scene->meshes[r->scene->meshCount].firstNormalIndex,
																							r->scene->meshes[r->scene->meshCount].firstTextureIndex,
																							r->scene->meshes[r->scene->meshCount].firstPolyIndex + i);
	}
	
	//r->scene->meshes[r->scene->meshCount].mat = calloc(1, sizeof(IMaterial));
	//Parse materials
	if (data.material_count == 0) {
		//No material, set to something obscene to make it noticeable
		//r->scene->meshes[r->scene->meshCount].mat = calloc(1, sizeof(IMaterial));
		r->scene->meshes[r->scene->meshCount].material = newMaterial(MATERIAL_TYPE_WARNING);
		//assignBSDF(&r->scene->meshes[r->scene->meshCount].materials[0]);
		//r->scene->meshes[r->scene->meshCount].materialCount++;
	} else {
		//r->scene->meshes[r->scene->meshCount].mat = NewMaterial(MATERIAL_TYPE_DEFAULT);
		//Loop to add materials to mesh (We already set the material indices in polyFromObj)
		for (int i = 0; i < data.material_count; i++) {
			addMaterialToMesh(&r->scene->meshes[r->scene->meshCount], materialFromObj(data.material_list[i]));
		}
	}
	
	//Load textures for meshes
	loadMeshTextures(&r->scene->meshes[r->scene->meshCount]);
	
	//Delete OBJ data
	delete_obj_data(&data);
	
	//Mesh added, update count
	r->scene->meshCount++;
	return true;
}

//FIXME: change + 1 to ++scene->someCount and just pass the count to array access
//In the future, maybe just pass a list and size and copy at once to save time (large counts)
void addSphere(struct world *scene, struct sphere newSphere) {
	scene->spheres = realloc(scene->spheres, (scene->sphereCount + 1) * sizeof(struct sphere));
	scene->spheres[scene->sphereCount++] = newSphere;
}

void addMaterialToMesh(struct mesh *mesh, struct material *newMaterial) {
	//mesh->mat = realloc(mesh->materials, (mesh->materialCount + 1) * sizeof(IMaterial));
	mesh->material = newMaterial;
}

void transformMeshes(struct world *scene) {
	logr(info, "Running transforms...\n");
	for (int i = 0; i < scene->meshCount; ++i) {
		transformMesh(&scene->meshes[i]);
	}
}

void computeKDTrees(struct world *scene) {
	logr(info, "Computing KD-trees...\n");
	for (int i = 0; i < scene->meshCount; ++i) {
		int *polys = calloc(scene->meshes[i].polyCount, sizeof(int));
		for (int j = 0; j < scene->meshes[i].polyCount; j++) {
			polys[j] = scene->meshes[i].firstPolyIndex + j;
		}
		scene->meshes[i].tree = buildTree(polys, scene->meshes[i].polyCount, 0);
		
		// Optional tree checking
		/*int orphans = checkTree(scene->meshes[i].tree);
		if (orphans > 0) {
			int total = countNodes(scene->meshes[i].tree);
			logr(warning, "Found %i/%i orphan nodes in %s kdtree\n", orphans, total, scene->meshes[i].name);
		}*/
	}
}

void addCamTransform(struct camera *cam, struct transform transform) {
	if (cam->transformCount == 0) {
		cam->transforms = calloc(1, sizeof(struct transform));
	} else {
		cam->transforms = realloc(cam->transforms, (cam->transformCount + 1) * sizeof(struct transform));
	}
	
	cam->transforms[cam->transformCount] = transform;
	cam->transformCount++;
}

void addCamTransforms(struct camera *cam, struct transform *transforms, int count) {
	for (int i = 0; i < count; i++) {
		addCamTransform(cam, transforms[i]);
	}
}

void printSceneStats(struct world *scene, unsigned long long ms) {
	logr(info, "Scene parsing completed in %llums\n", ms);
	logr(info, "Totals: %iV, %iN, %iP, %iS\n",
		   vertexCount,
		   normalCount,
		   polyCount,
		   scene->sphereCount);
}

struct material *parseMaterial(const cJSON *data) {
	const cJSON* diffuseBSDF_JSON = cJSON_GetObjectItem(data, "diffuseBSDF");
	const cJSON* specularBSDF_JSON = cJSON_GetObjectItem(data, "specularBSDF");
	enum diffuseBSDF diffuseBSDF = DIFFUSE_BSDF_LAMBERT;
	enum specularBSDF specularBSDF = SPECULAR_BSDF_PHONG;

	cJSON *IOR = NULL;
	//cJSON *roughness = NULL;
	cJSON *colorJson = NULL;
	cJSON *intensity = NULL;
	
	bool validColor = false;
	bool validIOR = false;
	//bool validRoughness = false;
	bool validIntensity = false;
	
	float IORValue = 1.0;
	//float roughnessValue;
	color *colorValue = NULL;
	float intensityValue = 1.0;
	
	struct material *mat = newMaterial(MATERIAL_TYPE_DEFAULT);
	
	colorJson = cJSON_GetObjectItem(data, "color");
	colorValue = parseColor(colorJson);
	if (colorValue != NULL) {
		validColor = true;
	}
	
	intensity = cJSON_GetObjectItem(data, "intensity");
	if (cJSON_IsNumber(intensity)) {
		validIntensity = true;
		if (intensity->valuedouble >= 0) {
			intensityValue = intensity->valuedouble;
		} else {
			intensityValue = 1.0;
		}
	}
	
	IOR = cJSON_GetObjectItem(data, "IOR");
	if (cJSON_IsNumber(IOR)) {
		validIOR = true;
		if (IOR->valuedouble >= 0) {
			IORValue = IOR->valuedouble;
		} else {
			IORValue = 1.0;
		}
	}

	setMaterialFloat(mat, "ior", IORValue);

	if (cJSON_IsString(diffuseBSDF_JSON)) {
		diffuseBSDF = getDiffuseBSDF_FromStr(diffuseBSDF_JSON->valuestring);
	}
	else {
		logr(warning, "No diffuse BSDF given for material.");
		logr(error, "Material data: %s\n", cJSON_Print(data));
	}

	if (cJSON_IsString(specularBSDF_JSON)) {
		specularBSDF = getSpecularBSDF_FromStr(specularBSDF_JSON->valuestring);
	}
	else {
		logr(warning, "No specular BSDF given for material.");
		logr(error, "Material data: %s\n", cJSON_Print(data));
	}

	setMaterialColor(mat, "albedo", *colorValue);

	free(colorValue);
	//assignBSDF(mat);
	return mat;
}

struct transform parseTransform(const cJSON *data, char *targetName) {
	cJSON *type = cJSON_GetObjectItem(data, "type");
	if (!cJSON_IsString(type)) {
		logr(warning, "Failed to parse transform! No type found\n");
		logr(warning, "Transform data: %s\n", cJSON_Print(data));
	}
	
	cJSON *degrees = NULL;
	cJSON *radians = NULL;
	cJSON *scale = NULL;
	cJSON *X = NULL;
	cJSON *Y = NULL;
	cJSON *Z = NULL;
	
	bool validDegrees = false;
	bool validRadians = false;
	bool validScale = false;
	
	degrees = cJSON_GetObjectItem(data, "degrees");
	radians = cJSON_GetObjectItem(data, "radians");
	scale = cJSON_GetObjectItem(data, "scale");
	X = cJSON_GetObjectItem(data, "X");
	Y = cJSON_GetObjectItem(data, "Y");
	Z = cJSON_GetObjectItem(data, "Z");
	
	if (degrees != NULL && cJSON_IsNumber(degrees)) {
		validDegrees = true;
	}
	if (radians != NULL && cJSON_IsNumber(radians)) {
		validRadians = true;
	}
	if (scale != NULL && cJSON_IsNumber(scale)) {
		validScale = true;
	}
	
	//For translate, we want the default to be 0. For scaling, def should be 1
	float def = 0.0;
	if (strcmp(type->valuestring, "scale") == 0) {
		def = 1.0;
	}
	
	int validvec2s = 0; //Accept if we have at least one provided
	float Xval, Yval, Zval;
	if (X != NULL && cJSON_IsNumber(X)) {
		Xval = X->valuedouble;
		validvec2s++;
	} else {
		Xval = def;
	}
	if (Y != NULL && cJSON_IsNumber(Y)) {
		Yval = Y->valuedouble;
		validvec2s++;
	} else {
		Yval = def;
	}
	if (Z != NULL && cJSON_IsNumber(Z)) {
		Zval = Z->valuedouble;
		validvec2s++;
	} else {
		Zval = def;
	}
	
	if (strcmp(type->valuestring, "rotateX") == 0) {
		if (validDegrees) {
			return newTransformRotateX(degrees->valuedouble);
		} else if (validRadians) {
			return newTransformRotateX(fromRadians(radians->valuedouble));
		} else {
			logr(warning, "Found rotateX transform for object \"%s\" with no valid degrees or radians value given.\n", targetName);
		}
	} else if (strcmp(type->valuestring, "rotateY") == 0) {
		if (validDegrees) {
			return newTransformRotateY(degrees->valuedouble);
		} else if (validRadians) {
			return newTransformRotateY(fromRadians(radians->valuedouble));
		} else {
			logr(warning, "Found rotateY transform for object \"%s\" with no valid degrees or radians value given.\n", targetName);
		}
	} else if (strcmp(type->valuestring, "rotateZ") == 0) {
		if (validDegrees) {
			return newTransformRotateZ(degrees->valuedouble);
		} else if (validRadians) {
			return newTransformRotateZ(fromRadians(radians->valuedouble));
		} else {
			logr(warning, "Found rotateZ transform for object \"%s\" with no valid degrees or radians value given.\n", targetName);
		}
	} else if (strcmp(type->valuestring, "translate") == 0) {
		if (validvec2s > 0) {
			return newTransformTranslate(Xval, Yval, Zval);
		} else {
			logr(warning, "Found translate transform for object \"%s\" with less than 1 valid vec2inate given.\n", targetName);
		}
	} else if (strcmp(type->valuestring, "scale") == 0) {
		if (validvec2s > 0) {
			return newTransformScale(Xval, Yval, Zval);
		} else {
			logr(warning, "Found scale transform for object \"%s\" with less than 1 valid scale value given.\n", targetName);
		}
	} else if (strcmp(type->valuestring, "scaleUniform") == 0) {
		if (validScale) {
			return newTransformScaleUniform(scale->valuedouble);
		} else {
			logr(warning, "Found scaleUniform transform for object \"%s\" with no valid scale value given.\n", targetName);
		}
	} else {
		logr(warning, "Found an invalid transform \"%s\" for object \"%s\"\n", type->valuestring, targetName);
	}
	
	//Hack. This is essentially just a NOP transform that does nothing.
	return newTransformTranslate(0.0, 0.0, 0.0);
}

//Parse JSON array of transforms, and return a pointer to an array of corresponding transforms
struct transform *parseTransforms(const cJSON *data) {
	
	int transformCount = cJSON_GetArraySize(data);
	struct transform *transforms = calloc(transformCount, sizeof(struct transform));
	
	cJSON *transform = NULL;
	
	for (int i = 0; i < transformCount; i++) {
		transform = cJSON_GetArrayItem(data, i);
		transforms[i] = parseTransform(transform, "camera");
	}
	return transforms;
}

int parseRenderer(struct renderer *r, const cJSON *data) {
	const cJSON *threadCount = NULL;
	const cJSON *sampleCount = NULL;
	const cJSON *antialiasing = NULL;
	const cJSON *tileWidth = NULL;
	const cJSON *tileHeight = NULL;
	const cJSON *tileOrder = NULL;
	const cJSON *bounces = NULL;
	
	threadCount = cJSON_GetObjectItem(data, "threadCount");
	if (cJSON_IsNumber(threadCount)) {
		if (threadCount->valueint > 0) {
			r->prefs.threadCount = threadCount->valueint;
		} else {
			r->prefs.threadCount = getSysCores();
		}
	} else {
		logr(warning, "Invalid threadCount while parsing renderer\n");
		return -1;
	}
	
	sampleCount = cJSON_GetObjectItem(data, "sampleCount");
	if (cJSON_IsNumber(sampleCount)) {
		if (sampleCount->valueint >= 1) {
			r->prefs.sampleCount = sampleCount->valueint;
		} else {
			r->prefs.sampleCount = 1;
		}
	} else {
		logr(warning, "Invalid sampleCount while parsing renderer\n");
		return -1;
	}
	
	bounces = cJSON_GetObjectItem(data, "bounces");
	if (cJSON_IsNumber(bounces)) {
		if (bounces->valueint >= 0) {
			r->prefs.bounces = bounces->valueint;
		} else {
			r->prefs.bounces = 0;
		}
	}
	
	antialiasing = cJSON_GetObjectItem(data, "antialiasing");
	if (cJSON_IsBool(antialiasing)) {
		r->prefs.antialiasing = cJSON_IsTrue(antialiasing);
	} else {
		logr(warning, "Invalid antialiasing bool while parsing renderer\n");
		return -1;
	}
	
	tileWidth = cJSON_GetObjectItem(data, "tileWidth");
	if (cJSON_IsNumber(tileWidth)) {
		if (tileWidth->valueint >= 1) {
			r->prefs.tileWidth = tileWidth->valueint;
		} else {
			r->prefs.tileWidth = 1;
		}
	} else {
		logr(warning, "Invalid tileWidth while parsing renderer\n");
		return -1;
	}
	
	tileHeight = cJSON_GetObjectItem(data, "tileHeight");
	if (cJSON_IsNumber(tileHeight)) {
		if (tileHeight->valueint >= 1) {
			r->prefs.tileHeight = tileHeight->valueint;
		} else {
			r->prefs.tileHeight = 1;
		}
	} else {
		logr(warning, "Invalid tileHeight while parsing renderer\n");
		return -1;
	}
	
	tileOrder = cJSON_GetObjectItem(data, "tileOrder");
	if (cJSON_IsString(tileOrder)) {
		if (strcmp(tileOrder->valuestring, "normal") == 0) {
			r->prefs.tileOrder = renderOrderNormal;
		} else if (strcmp(tileOrder->valuestring, "random") == 0) {
			r->prefs.tileOrder = renderOrderRandom;
		} else if (strcmp(tileOrder->valuestring, "topToBottom") == 0) {
			r->prefs.tileOrder = renderOrderTopToBottom;
		} else if (strcmp(tileOrder->valuestring, "fromMiddle") == 0) {
			r->prefs.tileOrder = renderOrderFromMiddle;
		} else if (strcmp(tileOrder->valuestring, "toMiddle") == 0) {
			r->prefs.tileOrder = renderOrderToMiddle;
		} else {
			r->prefs.tileOrder = renderOrderNormal;
		}
	} else {
		logr(warning, "Invalid tileOrder while parsing renderer\n");
		return -1;
	}
	
	return 0;
}

int parseDisplay(struct renderer *r, const cJSON *data) {
	
#ifdef UI_ENABLED
	const cJSON *enabled = NULL;
	const cJSON *isFullscreen = NULL;
	const cJSON *isBorderless = NULL;
	const cJSON *windowScale = NULL;
	
	enabled = cJSON_GetObjectItem(data, "enabled");
	if (cJSON_IsBool(enabled)) {
		r->mainDisplay->enabled = cJSON_IsTrue(enabled);
	} else {
		r->mainDisplay->enabled = true;
	}
	
	isFullscreen = cJSON_GetObjectItem(data, "isFullscreen");
	if (cJSON_IsBool(isFullscreen)) {
		r->mainDisplay->isFullScreen = cJSON_IsTrue(isFullscreen);
	}
	
	isBorderless = cJSON_GetObjectItem(data, "isBorderless");
	if (cJSON_IsBool(isBorderless)) {
		r->mainDisplay->isBorderless = cJSON_IsTrue(isBorderless);
	}
	windowScale = cJSON_GetObjectItem(data, "windowScale");
	if (cJSON_IsNumber(windowScale)) {
		if (windowScale->valuedouble >= 0) {
			r->mainDisplay->windowScale = windowScale->valuedouble;
		} else {
			r->mainDisplay->windowScale = 0.5;
		}
	}
#endif
	return 0;
}

int parseCamera(struct renderer *r, const cJSON *data) {
	
	const cJSON *FOV = NULL;
	const cJSON *aperture = NULL;
	const cJSON *transforms = NULL;
	
	FOV = cJSON_GetObjectItem(data, "FOV");
	if (cJSON_IsNumber(FOV)) {
		if (FOV->valuedouble >= 0.0) {
			if (FOV->valuedouble > 180.0) {
				r->scene->camera->FOV = 180.0;
			} else {
				r->scene->camera->FOV = FOV->valuedouble;
			}
		} else {
			r->scene->camera->FOV = 80.0;
		}
	} else {
		logr(warning, "No FOV for camera found");
		return -1;
	}
	
	aperture = cJSON_GetObjectItem(data, "aperture");
	if (cJSON_IsNumber(aperture)) {
		if (aperture->valuedouble >= 0.0) {
			r->scene->camera->aperture = aperture->valuedouble;
		} else {
			r->scene->camera->aperture = 0.0;
		}
	} else {
		logr(warning, "No aperture for camera found");
		return -1;
	}
	
	transforms = cJSON_GetObjectItem(data, "transforms");
	if (cJSON_IsArray(transforms)) {
		int tformCount = cJSON_GetArraySize(transforms);
		addCamTransforms(r->scene->camera, parseTransforms(transforms), tformCount);
	} else {
		logr(warning, "No transforms for camera found");
		return -1;
	}
	
	return 0;
}

color *parseColor(const cJSON *data) {
	
	const cJSON *R = NULL;
	const cJSON *G = NULL;
	const cJSON *B = NULL;
	const cJSON *A = NULL;
	
	color *newColor = calloc(1, sizeof(color));
	
	R = cJSON_GetObjectItem(data, "r");
	if (R != NULL && cJSON_IsNumber(R)) {
		newColor->red = R->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	G = cJSON_GetObjectItem(data, "g");
	if (R != NULL && cJSON_IsNumber(G)) {
		newColor->green = G->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	B = cJSON_GetObjectItem(data, "b");
	if (R != NULL && cJSON_IsNumber(B)) {
		newColor->blue = B->valuedouble;
	} else {
		free(newColor);
		return NULL;
	}
	
	A = cJSON_GetObjectItem(data, "a");
	if (R != NULL && cJSON_IsNumber(A)) {
		newColor->alpha = A->valuedouble;
	} else {
		newColor->alpha = 0.0;
	}
	
	return newColor;
}

int parseAmbientColor(struct renderer *r, const cJSON *data) {
	const cJSON *down = NULL;
	const cJSON *up = NULL;
	const cJSON *hdr = NULL;
	const cJSON *offset = NULL;
	
	struct gradient *newGradient = calloc(1, sizeof(struct gradient));
	
	down = cJSON_GetObjectItem(data, "down");
	up = cJSON_GetObjectItem(data, "up");

	color* downColor = parseColor(down);
	color* upColor = parseColor(up);
	newGradient->down = *downColor;
	newGradient->up = *upColor;
	
	r->scene->ambientColor = newGradient;
	
	hdr = cJSON_GetObjectItem(data, "hdr");
	if (cJSON_IsString(hdr)) {
		r->scene->hdr = loadTexture(hdr->valuestring);
	}
	
	offset = cJSON_GetObjectItem(data, "offset");
	if (cJSON_IsNumber(offset)) {
		if (r->scene->hdr) {
			r->scene->hdr->offset = toRadians(offset->valuedouble)/4;
		}
	}
	
	return 0;
}

//FIXME: Only parse everything else if the mesh is found and is valid
void parseMesh(struct renderer *r, const cJSON *data, int idx, int meshCount) {
	const cJSON *fileName = cJSON_GetObjectItem(data, "fileName");

	const cJSON* diffuseBSDF_JSON = cJSON_GetObjectItem(data, "diffuseBSDF");
	const cJSON* specularBSDF_JSON = cJSON_GetObjectItem(data, "specularBSDF");
	enum diffuseBSDF diffuseBSDF = DIFFUSE_BSDF_LAMBERT;
	enum specularBSDF specularBSDF = SPECULAR_BSDF_PHONG;
	
	if (cJSON_IsString(diffuseBSDF_JSON)) {
		diffuseBSDF = getDiffuseBSDF_FromStr(diffuseBSDF_JSON->valuestring);
	} else {
		logr(warning, "Invalid diffuse BSDF while parsing mesh\n");
	}

	if (cJSON_IsString(specularBSDF_JSON)) {
		specularBSDF = getSpecularBSDF_FromStr(specularBSDF_JSON->valuestring);
	}
	else {
		logr(warning, "Invalid specular BSDF while parsing mesh\n");
	}

	cJSON* mat_json = cJSON_GetObjectItem(data, "material");

	color albedo;
	float roughness;
	float specularity;
	float metalness;
	float anisotropy;
	float ior;

	{
		cJSON* albedo_json = cJSON_GetObjectItem(mat_json, "albedo");
		cJSON* roughness_json = cJSON_GetObjectItem(mat_json, "roughness");
		cJSON* specularity_json = cJSON_GetObjectItem(mat_json, "specularity");
		cJSON* metalness_json = cJSON_GetObjectItem(mat_json, "metalness");
		cJSON* anisotropy_json = cJSON_GetObjectItem(mat_json, "anisotropy");
		cJSON* ior_json = cJSON_GetObjectItem(mat_json, "ior");

		cJSON* albedo_r_json = cJSON_GetObjectItem(albedo_json, "r");
		cJSON* albedo_g_json = cJSON_GetObjectItem(albedo_json, "g");
		cJSON* albedo_b_json = cJSON_GetObjectItem(albedo_json, "b");

		albedo = (color){ albedo_r_json->valuedouble, albedo_g_json->valuedouble, albedo_b_json->valuedouble, 1.0f };
		roughness = roughness_json->valuedouble;
		specularity = specularity_json->valuedouble;
		metalness = metalness_json->valuedouble;
		anisotropy = anisotropy_json->valuedouble;
		ior = ior_json->valuedouble;
	}

	bool meshValid = false;
	if (fileName != NULL && cJSON_IsString(fileName)) {
		if (loadMesh(r, fileName->valuestring, idx, meshCount)) {
			meshValid = true;
		} else {
			return;
		}
	}
	if (meshValid) {
		const cJSON *transforms = cJSON_GetObjectItem(data, "transforms");
		const cJSON *transform = NULL;
		//TODO: Use parseTransforms for this
		if (transforms != NULL && cJSON_IsArray(transforms)) {
			cJSON_ArrayForEach(transform, transforms) {
				addTransform(lastMesh(r), parseTransform(transform, lastMesh(r)->name));
			}
		}

		struct material *mat = lastMesh(r)->material;
		mat->type = MATERIAL_TYPE_DEFAULT;
		mat->diffuseBSDF = diffuseBSDF;
		mat->specularBSDF = specularBSDF;
		setMaterialColor(mat, "albedo", albedo);
		setMaterialFloat(mat, "roughness", roughness);
		setMaterialFloat(mat, "specularity", specularity);
		setMaterialFloat(mat, "metalness", metalness);
		setMaterialFloat(mat, "anisotropy", anisotropy);
		setMaterialFloat(mat, "ior", ior);
		
		//FIXME: this isn't right.
		/*
		for (int i = 0; i < lastMesh(r)->materialCount; i++) {
			IMaterial mat = lastMesh(r)->materials[i];
			mat->diffuse_bsdf_type = diffuse_bsdf_type;
			mat->specular_bsdf_type = specular_bsdf_type;
			MaterialSetVec3(mat, "albedo", albedo);
			MaterialSetFloat(mat, "roughness", roughness);
			MaterialSetFloat(mat, "specularity", specularity);
			MaterialSetFloat(mat, "metalness", metalness);
			MaterialSetFloat(mat, "anisotropy", anisotropy);
			MaterialSetFloat(mat, "ior", ior);
		}*/
	}
}

void parseMeshes(struct renderer *r, const cJSON *data) {
	const cJSON *mesh = NULL;
	int idx = 1;
	int meshCount = cJSON_GetArraySize(data);
	if (data != NULL && cJSON_IsArray(data)) {
		cJSON_ArrayForEach(mesh, data) {
			parseMesh(r, mesh, idx, meshCount);
			idx++;
		}
	}
	printf("\n");
}

vec3 parsevec3inate(const cJSON *data) {
	const cJSON *X = NULL;
	const cJSON *Y = NULL;
	const cJSON *Z = NULL;
	X = cJSON_GetObjectItem(data, "X");
	Y = cJSON_GetObjectItem(data, "Y");
	Z = cJSON_GetObjectItem(data, "Z");
	
	if (X != NULL && Y != NULL && Z != NULL) {
		if (cJSON_IsNumber(X) && cJSON_IsNumber(Y) && cJSON_IsNumber(Z)) {
			return vecWithPos(X->valuedouble, Y->valuedouble, Z->valuedouble);
		}
	}
	logr(warning, "Invalid vec3inate parsed! Returning 0,0,0\n");
	logr(warning, "Faulty JSON: %s\n", cJSON_Print(data));
	return (vec3){0.0,0.0,0.0};
}

void parseSphere(struct renderer *r, const cJSON *data) {
	const cJSON *pos = NULL;
	const cJSON *radius = NULL;
	
	struct sphere newSphere = defaultSphere(); // inits material

	const cJSON* materialType_JSON = cJSON_GetObjectItem(data, "materialType");
	const cJSON *diffuseBSDF_JSON = cJSON_GetObjectItem(data, "diffuseBSDF");
	const cJSON *specularBSDF_JSON = cJSON_GetObjectItem(data, "specularBSDF");
	enum diffuseBSDF diffuseBSDF = DIFFUSE_BSDF_LAMBERT;
	enum specularBSDF specularBSDF = SPECULAR_BSDF_PHONG;

	if (cJSON_IsString(diffuseBSDF_JSON)) {
		diffuseBSDF = getDiffuseBSDF_FromStr(diffuseBSDF_JSON->valuestring);
	}
	else {
		logr(warning, "Invalid diffuse BSDF while parsing mesh\n");
	}

	if (cJSON_IsString(specularBSDF_JSON)) {
		specularBSDF = getSpecularBSDF_FromStr(specularBSDF_JSON->valuestring);
	}
	else {
		logr(warning, "Invalid specular BSDF while parsing mesh\n");
	}

	if (cJSON_IsString(materialType_JSON))
	{
		newSphere.material->type = getMaterialType_FromStr(materialType_JSON->valuestring);
	}
	else
	{
		newSphere.material->type = MATERIAL_TYPE_WARNING;
	}

	cJSON* mat_json = cJSON_GetObjectItem(data, "material");

	vec3 albedo;
	float roughness;
	float specularity;
	float metalness;
	float anisotropy;
	float ior;

	{
		cJSON* albedo_json = cJSON_GetObjectItem(mat_json, "albedo");
		cJSON* roughness_json = cJSON_GetObjectItem(mat_json, "roughness");
		cJSON* specularity_json = cJSON_GetObjectItem(mat_json, "specularity");
		cJSON* metalness_json = cJSON_GetObjectItem(mat_json, "metalness");
		cJSON* anisotropy_json = cJSON_GetObjectItem(mat_json, "anisotropy");
		cJSON* ior_json = cJSON_GetObjectItem(mat_json, "ior");

		cJSON* albedo_r_json = cJSON_GetObjectItem(albedo_json, "r");
		cJSON* albedo_g_json = cJSON_GetObjectItem(albedo_json, "g");
		cJSON* albedo_b_json = cJSON_GetObjectItem(albedo_json, "b");

		albedo = (vec3){ albedo_r_json->valuedouble, albedo_g_json->valuedouble, albedo_b_json->valuedouble };
		roughness = roughness_json->valuedouble;
		specularity = specularity_json->valuedouble;
		metalness = metalness_json->valuedouble;
		anisotropy = anisotropy_json->valuedouble;
		ior = ior_json->valuedouble;
	}

	struct material *mat = newSphere.material;
	mat->diffuseBSDF = diffuseBSDF;
	mat->specularBSDF = specularBSDF;
	setMaterialVec3(mat, "albedo", albedo);
	setMaterialFloat(mat, "roughness", roughness);
	setMaterialFloat(mat, "specularity", specularity);
	setMaterialFloat(mat, "metalness", metalness);
	setMaterialFloat(mat, "anisotropy", anisotropy);
	setMaterialFloat(mat, "ior", ior);

	pos = cJSON_GetObjectItem(data, "pos");
	if (pos != NULL) {
		newSphere.pos = parsevec3inate(pos);
	} else {
		logr(warning, "No position specified for sphere\n");
	}
	
	radius = cJSON_GetObjectItem(data, "radius");
	if (radius != NULL && cJSON_IsNumber(radius)) {
		newSphere.radius = radius->valuedouble;
	} else {
		newSphere.radius = 10;
		logr(warning, "No radius specified for sphere, setting to %.0f\n", newSphere.radius);
	}
	
	//FIXME: Proper materials for spheres
	addSphere(r->scene, newSphere);
	//assignBSDF(&lastSphere(r)->material);
}

void parsePrimitive(struct renderer *r, const cJSON *data, int idx) {
	const cJSON *type = NULL;
	type = cJSON_GetObjectItem(data, "type");
	if (strcmp(type->valuestring, "sphere") == 0) {
		parseSphere(r, data);
	} else {
		logr(warning, "Unknown primitive type \"%s\" at index %i\n", type->valuestring, idx);
	}
}

void parsePrimitives(struct renderer *r, const cJSON *data) {
	const cJSON *primitive = NULL;
	if (data != NULL && cJSON_IsArray(data)) {
		int i = 0;
		cJSON_ArrayForEach(primitive, data) {
			parsePrimitive(r, primitive, i);
			i++;
		}
	}
}

int parseScene(struct renderer *r, const cJSON *data) {
	
	const cJSON *filePath = NULL;
	const cJSON *fileName = NULL;
	const cJSON *count = NULL;
	const cJSON *width = NULL;
	const cJSON *height = NULL;
	const cJSON *fileType = NULL;
	const cJSON *ambientColor = NULL;
	const cJSON *primitives = NULL;
	const cJSON *meshes = NULL;
	
	filePath = cJSON_GetObjectItem(data, "outputFilePath");
	if (cJSON_IsString(filePath)) {
		copyString(filePath->valuestring, &r->state.image->filePath);
	}
	
	fileName = cJSON_GetObjectItem(data, "outputFileName");
	if (cJSON_IsString(fileName)) {
		copyString(fileName->valuestring, &r->state.image->fileName);
	}
	
	count = cJSON_GetObjectItem(data, "count");
	if (cJSON_IsNumber(count)) {
		if (count->valueint >= 0) {
			r->state.image->count = count->valueint;
		} else {
			r->state.image->count = 0;
		}
	}
	
	//FIXME: This is super ugly
	width = cJSON_GetObjectItem(data, "width");
	if (cJSON_IsNumber(width)) {
		if (width->valueint >= 0) {
			*r->state.image->width = width->valueint;
#ifdef UI_ENABLED
			r->mainDisplay->width = width->valueint;
#endif
		} else {
			*r->state.image->width = 640;
#ifdef UI_ENABLED
			r->mainDisplay->width = 640;
#endif
		}
	}
	
	height = cJSON_GetObjectItem(data, "height");
	if (cJSON_IsNumber(height)) {
		if (height->valueint >= 0) {
			*r->state.image->height = height->valueint;
#ifdef UI_ENABLED
			r->mainDisplay->height = height->valueint;
#endif
		} else {
			*r->state.image->height = 640;
#ifdef UI_ENABLED
			r->mainDisplay->height = 640;
#endif
		}
	}
	
	fileType = cJSON_GetObjectItem(data, "fileType");
	if (cJSON_IsString(fileType)) {
		if (strcmp(fileType->valuestring, "png") == 0) {
			r->state.image->fileType = png;
		} else if (strcmp(fileType->valuestring, "bmp") == 0) {
			r->state.image->fileType = bmp;
		} else {
			r->state.image->fileType = png;
		}
	}
	
	ambientColor = cJSON_GetObjectItem(data, "ambientColor");
	if (cJSON_IsObject(ambientColor)) {
		if (parseAmbientColor(r, ambientColor) == -1) {
			return -1;
		}
	}
	
	primitives = cJSON_GetObjectItem(data, "primitives");
	if (cJSON_IsArray(primitives)) {
		parsePrimitives(r, primitives);
	}
	
	meshes = cJSON_GetObjectItem(data, "meshes");
	if (cJSON_IsArray(meshes)) {
		parseMeshes(r, meshes);
	}
	
	return 0;
}

//input is either a file path to load if fromStdin = false, or a data buffer if stdin = true
int parseJSON(struct renderer *r, char *input, bool fromStdin) {
	
	/*
	 Note: Since we are freeing the JSON data (and its' pointers) after parsing,
	 we need to *copy* all dynamically allocated strings with the copyString() function.
	 */
	
	char *buf = NULL;
	
	if (fromStdin) {
		buf = input;
	} else {
		buf = loadFile(input);
	}
	
	cJSON *json = cJSON_Parse(buf);
	if (json == NULL) {
		logr(warning, "Failed to parse JSON\n");
		const char *errptr = cJSON_GetErrorPtr();
		if (errptr != NULL) {
			free(buf);
			cJSON_Delete(json);
			logr(warning, "Error before: %s\n", errptr);
			return -2;
		}
	}
	
	const cJSON *renderer = NULL;
	const cJSON *display = NULL;
	const cJSON *camera = NULL;
	const cJSON *scene = NULL;
	
	renderer = cJSON_GetObjectItem(json, "renderer");
	if (renderer != NULL) {
		if (parseRenderer(r, renderer) == -1) {
			logr(warning, "Renderer parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No renderer found\n");
		free(buf);
		return -2;
	}
	
	display = cJSON_GetObjectItem(json, "display");
	if (display != NULL) {
		if (parseDisplay(r, display) == -1) {
			logr(warning, "Display parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No display found\n");
		free(buf);
		return -2;
	}
	
	camera = cJSON_GetObjectItem(json, "camera");
	if (camera != NULL) {
		if (parseCamera(r, camera) == -1) {
			logr(warning, "Camera parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No camera found\n");
		free(buf);
		return -2;
	}
	
	scene = cJSON_GetObjectItem(json, "scene");
	if (scene != NULL) {
		if (parseScene(r, scene) == -1) {
			logr(warning, "Scene parse failed!\n");
			free(buf);
			return -2;
		}
	} else {
		logr(warning, "No scene found\n");
		free(buf);
		return -2;
	}
	
	cJSON_Delete(json);
	free(buf);
	
	return 0;
}

//Load the scene, allocate buffers, etc
void loadScene(struct renderer *r, char *input, bool fromStdin) {
	
	struct timeval *timer = calloc(1, sizeof(struct timeval));
	startTimer(timer);
	
	//Build the scene
	switch (parseJSON(r, input, fromStdin)) {
		case -1:
			logr(error, "Scene builder failed due to previous error.\n");
			break;
		case 4:
			logr(error, "Scene debug mode enabled, won't render image.\n");
			break;
		case -2:
			logr(error, "JSON parser failed.\n");
			break;
		default:
			break;
	}
	
	transformCameraIntoView(r->scene->camera);
	transformMeshes(r->scene);
	computeKDTrees(r->scene);
	printSceneStats(r->scene, endTimer(timer));
	free(timer);
	
	//Alloc threadPaused booleans, one for each thread
	r->state.threadPaused = calloc(r->prefs.threadCount, sizeof(bool));
	//Alloc timers, one for each thread
	r->state.timers = calloc(r->prefs.threadCount, sizeof(struct timeval));
	//Alloc RNGs, one for each thread
	r->state.rngs = calloc(r->prefs.threadCount, sizeof(pcg32_random_t));
	
	//Seed each rng	
	for (int i = 0; i < r->prefs.threadCount; i++) {
		pcg32_srandom_r(&r->state.rngs[i], 3141592 + i, 0);
	}
	
	struct renderTile *tiles;
	//Quantize image into renderTiles
	r->state.tileCount = quantizeImage(&tiles, r->state.image, r->prefs.tileWidth, r->prefs.tileHeight);
	reorderTiles(&tiles, r->state.tileCount, r->prefs.tileOrder);
	assignTiles(tiles, &r->state.renderTiles, r->state.tileCount, r->prefs.threadCount, &r->state.tileAmounts);
	free(tiles);
	
	//Compute the focal length for the camera
	computeFocalLength(r->scene->camera, *r->state.image->width);
	
	//Allocate memory and create array of pixels for image data
	allocTextureBuffer(r->state.image, char_p, *r->state.image->width, *r->state.image->height, 3);
	if (!r->state.image->byte_data) {
		logr(error, "Failed to allocate memory for image data.");
	}
	
	//Set a dark gray background for the render preview
	for (int x = 0; x < *r->state.image->width; x++) {
		for (int y = 0; y < *r->state.image->height; y++) {
			blit(r->state.image, backgroundColor, x, y);
		}
	}
	
	//Allocate memory for render buffer
	//Render buffer is used to store accurate color values for the renderers' internal use
	r->state.renderBuffer = newTexture();
	allocTextureBuffer(r->state.renderBuffer, float_p, *r->state.image->width, *r->state.image->height, 3);
	
	//Allocate memory for render UI buffer
	//This buffer is used for storing UI stuff like currently rendering tile highlights
	r->state.uiBuffer = newTexture();
	allocTextureBuffer(r->state.uiBuffer, char_p, *r->state.image->width, *r->state.image->height, 4);
	
	//Alloc memory for pthread_create() args
	r->state.threadStates = calloc(r->prefs.threadCount, sizeof(struct threadState));
	if (r->state.threadStates == NULL) {
		logr(error, "Failed to allocate memory for threadInfo args.\n");
	}
	
	//Print a useful warning to user if the defined tile size results in less renderThreads
	if (r->state.tileCount < r->prefs.threadCount) {
		logr(warning, "WARNING: Rendering with a less than optimal thread count due to large tile size!\n");
		logr(warning, "Reducing thread count from %i to ", r->prefs.threadCount);
		r->prefs.threadCount = r->state.tileCount;
		printf("%i\n", r->prefs.threadCount);
	}
}

//Free scene data
void freeScene(struct world *scene) {
	if (scene->ambientColor) {
		free(scene->ambientColor);
	}
	if (scene->meshes) {
		for (int i = 0; i < scene->meshCount; i++) {
			freeMesh(&scene->meshes[i]);
		}
		free(scene->meshes);
	}
	if (scene->spheres) {
		free(scene->spheres);
	}
	if (scene->camera) {
		freeCamera(scene->camera);
		free(scene->camera);
	}
}
