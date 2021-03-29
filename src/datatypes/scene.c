//
//  scene.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 28/02/2015.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "scene.h"

#include "../utils/timer.h"
#include "../utils/loaders/sceneloader.h"
#include "../utils/logging.h"
#include "image/imagefile.h"
#include "../renderer/renderer.h"
#include "image/texture.h"
#include "camera.h"
#include "vertexbuffer.h"
#include "../accelerators/bvh.h"
#include "tile.h"
#include "mesh.h"
#include "poly.h"
#include "../utils/platform/thread.h"
#include "../utils/ui.h"
#include "../datatypes/instance.h"
#include "../datatypes/bbox.h"
#include "../utils/mempool.h"
#include "../utils/hashtable.h"
#include "../nodes/bsdfnode.h"
#include "../utils/hashtable.h"
#include "../utils/string.h"
#include "../utils/args.h"
#include "../utils/fileio.h"
#include "../utils/base64.h"
#include "../utils/textbuffer.h"

struct bvhBuildTask {
	struct bvh *bvh;
	struct poly *polygons;
	int polyCount;
};

void *bvhBuildThread(void *arg) {
	struct bvhBuildTask *task = (struct bvhBuildTask*)threadUserData(arg);
	task->bvh = buildBottomLevelBvh(task->polygons, task->polyCount);
	return NULL;
}

static void computeAccels(struct mesh *meshes, int meshCount) {
	logr(info, "Computing BVHs: ");
	struct timeval timer = {0};
	startTimer(&timer);
	struct bvhBuildTask *tasks = calloc(meshCount, sizeof(*tasks));
	struct crThread *buildThreads = calloc(meshCount, sizeof(*buildThreads));
	for (int t = 0; t < meshCount; ++t) {
		tasks[t] = (struct bvhBuildTask){
			.polygons = meshes[t].polygons,
			.polyCount = meshes[t].polyCount
		};
		buildThreads[t] = (struct crThread){
			.threadFunc = bvhBuildThread,
			.userData = &tasks[t]
		};
		if (threadStart(&buildThreads[t])) {
			logr(error, "Failed to create a bvhBuildTask\n");
		}
	}
	
	for (int t = 0; t < meshCount; ++t) {
		threadWait(&buildThreads[t]);
		meshes[t].bvh = tasks[t].bvh;
	}
	printSmartTime(getMs(timer));
	free(tasks);
	free(buildThreads);
	logr(plain, "\n");
}

struct bvh *computeTopLevelBvh(struct instance *instances, int instanceCount) {
	logr(info, "Computing top-level BVH: ");
	struct timeval timer = {0};
	startTimer(&timer);
	struct bvh *new = buildTopLevelBvh(instances, instanceCount);
	printSmartTime(getMs(timer));
	logr(plain, "\n");
	return new;
}

static void printSceneStats(struct world *scene, unsigned long long ms) {
	logr(info, "Scene construction completed in ");
	printSmartTime(ms);
	unsigned polys = 0;
	for (int i = 0; i < scene->instanceCount; ++i) {
		if (isMesh(&scene->instances[i])) polys += ((struct mesh*)scene->instances[i].object)->polyCount;
	}
	logr(plain, "\n");
	logr(info, "Totals: %iV, %iN, %iT, %iP, %iS, %iM\n",
		   vertexCount,
		   normalCount,
		   textureCount,
		   polys,
		   scene->sphereCount,
		   scene->meshCount);
}

#include "../utils/filecache.h"
//Split scene loading and prefs?
//Load the scene, allocate buffers, etc
//FIXME: Rename this func and take parseJSON out to a separate call.
int loadScene(struct renderer *r, char *input) {
	
	struct timeval timer = {0};
	startTimer(&timer);
	
	r->scene = calloc(1, sizeof(*r->scene));
	
	r->scene->nodePool = newBlock(NULL, 1024);
	r->scene->nodeTable = newHashtable(compareNodes, &r->scene->nodePool);
	
	//Load configuration and assets
	switch (parseJSON(r, input)) {
		case -1:
			logr(warning, "Scene builder failed due to previous error.\n");
			return -1;
		case 4:
			logr(warning, "Scene debug mode enabled, won't render image.\n");
			return -1;
		case -2:
			//JSON parser failed
			return -1;
		default:
			break;
	}
	
	if (isSet("use_clustering")) {
		// Stash a cached of scene data here
		cJSON *cache = cJSON_Parse(input);
		// Embed files into the json as base64
		r->sceneCache = cJSON_PrintUnformatted(cache);
	}
	
	// Do some pre-render preparations
	// Compute BVH acceleration structures for all objects in the scene
	computeAccels(r->scene->meshes, r->scene->meshCount);
	// And then compute a single top-level BVH that contains all the objects
	r->scene->topLevel = computeTopLevelBvh(r->scene->instances, r->scene->instanceCount);
	printSceneStats(r->scene, getMs(timer));
	
	//Quantize image into renderTiles
	r->state.tileCount = quantizeImage(&r->state.renderTiles,
									   r->prefs.imageWidth,
									   r->prefs.imageHeight,
									   r->prefs.tileWidth,
									   r->prefs.tileHeight,
									   r->prefs.tileOrder);
	
	// Some of this stuff seems like it should be in newRenderer(), but notice
	// how they depend on r->prefs, which is populated by parseJSON
	
	//Allocate memory for render buffer
	//Render buffer is used to store accurate color values for the renderers' internal use
	r->state.renderBuffer = newTexture(float_p, r->prefs.imageWidth, r->prefs.imageHeight, 3);
	
	//Allocate memory for render UI buffer
	//This buffer is used for storing UI stuff like currently rendering tile highlights
	r->state.uiBuffer = newTexture(char_p, r->prefs.imageWidth, r->prefs.imageHeight, 4);
	
	//Print a useful warning to user if the defined tile size results in less renderThreads
	if (r->state.tileCount < r->prefs.threadCount) {
		logr(warning, "WARNING: Rendering with a less than optimal thread count due to large tile size!\n");
		logr(warning, "Reducing thread count from %i to %i\n", r->prefs.threadCount, r->state.tileCount);
		r->prefs.threadCount = r->state.tileCount;
	}
	return 0;
}

//Free scene data
void destroyScene(struct world *scene) {
	if (scene) {
		destroyCamera(scene->camera);
		for (int i = 0; i < scene->meshCount; ++i) {
			destroyMesh(&scene->meshes[i]);
		}
		destroyBvh(scene->topLevel);
		destroyHashtable(scene->nodeTable);
		destroyBlocks(scene->nodePool);
		free(scene->instances);
		free(scene->meshes);
		free(scene->spheres);
		free(scene);
	}
}
