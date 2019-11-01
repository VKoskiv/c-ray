//
//  renderer.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 19/02/2017.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "renderer.h"

#include "../datatypes/camera.h"
#include "../datatypes/scene.h"
#include "pathtrace.h"
#include "../utils/logging.h"
#include "../utils/ui.h"
#include "../datatypes/tile.h"
#include "../utils/timer.h"
#include "../datatypes/texture.h"
#include "../utils/loaders/textureloader.h"

color getPixel(struct renderer *r, int x, int y);

/// @todo Use defaultSettings state struct for this.
/// @todo Clean this up, it's ugly.
void render(struct renderer *r) {
	logr(info, "Starting C-ray renderer for frame %i\n", r->state.image->count);
	
	logr(info, "Rendering at %i x %i\n", *r->state.image->width,*r->state.image->height);
	logr(info, "Rendering %i samples with %i bounces.\n", r->prefs.sampleCount, r->prefs.bounces);
	logr(info, "Rendering with %d thread", r->prefs.threadCount);
	printf(r->prefs.threadCount > 1 ? "s.\n" : ".\n");
	
	logr(info, "Pathtracing...\n");
	
	//Create threads
	int t;
	
	r->state.isRendering = true;
	r->state.renderAborted = false;
#ifndef WINDOWS
	pthread_attr_init(&r->state.renderThreadAttributes);
	pthread_attr_setdetachstate(&r->state.renderThreadAttributes, PTHREAD_CREATE_JOINABLE);
#endif
	//Main loop (input)
	bool threadsHaveStarted = false;
	while (r->state.isRendering) {
#ifdef UI_ENABLED
		getKeyboardInput(r);
		drawWindow(r);
#endif
		
		if (!threadsHaveStarted) {
			threadsHaveStarted = true;
			//Create render threads
			for (t = 0; t < r->prefs.threadCount; t++) {
				r->state.threadStates[t].thread_num = t;
				r->state.threadStates[t].threadComplete = false;
				r->state.threadStates[t].r = r;
				r->state.activeThreads++;
#ifdef WINDOWS
				DWORD threadId;
				r->state.threadStates[t].thread_handle = CreateThread(NULL, 0, renderThread, &r->state.threadStates[t], 0, &threadId);
				if (r->state.threadStates[t].thread_handle == NULL) {
					logr(error, "Failed to create thread.\n");
					exit(-1);
				}
				r->state.threadStates[t].thread_id = threadId;
#else
				if (pthread_create(&r->state.threadStates[t].thread_id, &r->state.renderThreadAttributes, renderThread, &r->state.threadStates[t])) {
					logr(error, "Failed to create a thread.\n");
				}
#endif
			}
			
			r->state.threadStates->threadComplete = false;
			
#ifndef WINDOWS
			if (pthread_attr_destroy(&r->state.renderThreadAttributes)) {
				logr(warning, "Failed to destroy pthread.\n");
			}
#endif
		}
		
		//Wait for render threads to finish (Render finished)
		for (t = 0; t < r->prefs.threadCount; t++) {
			if (r->state.threadStates[t].threadComplete && r->state.threadStates[t].thread_num != -1) {
				r->state.activeThreads--;
				r->state.threadStates[t].thread_num = -1;
			}
			if (r->state.activeThreads == 0 || r->state.renderAborted) {
				r->state.isRendering = false;
			}
		}
		if (r->state.threadPaused[0]) {
			sleepMSec(800);
		} else {
			sleepMSec(40);
		}
	}
	
	//Make sure render threads are finished before continuing
	for (t = 0; t < r->prefs.threadCount; t++) {
#ifdef WINDOWS
		WaitForSingleObjectEx(r->state.threadStates[t].thread_handle, INFINITE, FALSE);
#else
		if (pthread_join(r->state.threadStates[t].thread_id, NULL)) {
			logr(warning, "Thread %t frozen.", t);
		}
#endif
	}
}

/**
 A render thread
 
 @param arg Thread information (see threadInfo struct)
 @return Exits when thread is done
 */
#ifdef WINDOWS
DWORD WINAPI renderThread(LPVOID arg) {
#else
void *renderThread(void *arg) {
#endif
	//First time setup for each thread
	struct lightRay incidentRay;
	struct threadState *tinfo = (struct threadState*)arg;
	
	struct renderer *r = tinfo->r;
	pcg32_random_t *rng = &tinfo->r->state.rngs[tinfo->thread_num];
	
	int currentTileIndex = 0;
	struct renderTile *tiles = r->state.renderTiles[tinfo->thread_num];
	struct renderTile tile = tiles[currentTileIndex];
	tiles[currentTileIndex].isRendering = true;
	tinfo->currentTileNum = tile.tileNum;
	tinfo->currentTileIdx = currentTileIndex;
	
	bool hasHitObject = false;
	
	while (currentTileIndex < r->state.tileAmounts[tinfo->thread_num] && r->state.isRendering) {
		unsigned long long sleepMs = 0;
		startTimer(&r->state.timers[tinfo->thread_num]);
		hasHitObject = false;
		
		while (tile.completedSamples < r->prefs.sampleCount+1 && r->state.isRendering) {
			for (int y = (int)tile.end.y; y > (int)tile.begin.y; y--) {
				for (int x = (int)tile.begin.x; x < (int)tile.end.x; x++) {
					
					float fracX = (float)x;
					float fracY = (float)y;
					
					//A cheap 'antialiasing' of sorts. The more samples, the better this works
					float jitter = 0.25;
					if (r->prefs.antialiasing) {
						fracX = rndFloat(fracX - jitter, fracX + jitter, rng);
						fracY = rndFloat(fracY - jitter, fracY + jitter, rng);
					}
					
					//Set up the light ray to be casted. direction is pointing towards the X,Y coordinate on the
					//imaginary plane in front of the origin. startPos is just the camera position.
					vec3 direction = {(fracX - 0.5 * *r->state.image->width)
												/ r->scene->camera->focalLength,
											   (fracY - 0.5 * *r->state.image->height)
												/ r->scene->camera->focalLength,
												1.0};
					
					//Normalize direction
					direction = vec3_normalize(direction);

					vec3 startPos = r->scene->camera->pos;
					vec3 left = r->scene->camera->left;
					vec3 up = r->scene->camera->up;
					
					//Run camera tranforms on direction vector
					transformCameraView(r->scene->camera, &direction);
					
					//Now handle aperture
					//FIXME: This is a 'square' aperture
					float aperture = r->scene->camera->aperture;
					if (aperture <= 0.0) {
						incidentRay.start = startPos;
					} else {
						float randY = rndFloat(-aperture, aperture, rng);
						float randX = rndFloat(-aperture, aperture, rng);
						vec3 randomStart = vec3_add(vec3_add(startPos, vec3_muls(up, randY)), vec3_muls(left, randX));
						
						incidentRay.start = randomStart;
					}
					
					incidentRay.direction = direction;
					incidentRay.rayType = rayTypeIncident;
					incidentRay.remainingInteractions = r->prefs.bounces;

					incidentRay.currentMedium = newMaterial(MATERIAL_TYPE_DEFAULT);
					setMaterialFloat(incidentRay.currentMedium, "ior", AIR_IOR);
					
					//For multi-sample rendering, we keep a running average of color values for each pixel
					//The next block of code does this
					
					//Get previous color value from render buffer

					color output = textureGetPixel(r->state.renderBuffer, x, y);
					
					//Get new sample (path tracing is initiated here)
					vec3 sample = pathTrace(&incidentRay, r->scene, r->prefs.bounces, rng, &hasHitObject);

					freeMaterial(incidentRay.currentMedium);
					
					//And process the running average
					output.r = output.r * (tile.completedSamples - 1);
					output.g = output.g * (tile.completedSamples - 1);
					output.b = output.b * (tile.completedSamples - 1);
					
					output = color_add(output, (color) { sample.r, sample.g, sample.b, 1.0f });
					
					output.r = output.r / tile.completedSamples;
					output.g = output.g / tile.completedSamples;
					output.b = output.b / tile.completedSamples;
					
					//Store internal render buffer (float precision)
					blit(r->state.renderBuffer, output, x, y);
					
					//Gamma correction
					output = toSRGB(output);
					
					//And store the image data
					blit(r->state.image, output, x, y);
				}
			}
			tile.completedSamples++;
			tinfo->completedSamples = tile.completedSamples;
			if (tile.completedSamples > 25 && !hasHitObject) break; //Abort if we didn't hit anything within 25 samples
			//Pause rendering when bool is set
			while (r->state.threadPaused[tinfo->thread_num] && !r->state.renderAborted) {
				sleepMSec(100);
				sleepMs += 100;
			}
		}
		tiles[currentTileIndex++].isRendering = false;
		//Tile has finished rendering, get a new one and start rendering it.
		tinfo->currentTileNum = -1;
		tinfo->currentTileIdx = -1;
		tinfo->completedSamples = 0;
		unsigned long long samples = tile.completedSamples * (tile.width * tile.height);
		tile = tiles[currentTileIndex];
		tiles[currentTileIndex].isRendering = true;
		tinfo->finishedTileCount++;
		tinfo->currentTileNum = tile.tileNum;
		tinfo->currentTileIdx = currentTileIndex;
		unsigned long long duration = endTimer(&r->state.timers[tinfo->thread_num]);
		if (sleepMs > 0) {
			duration -= sleepMs;
		}
		printStats(r, duration, samples, tinfo->thread_num);
	}
	//No more tiles to render, exit thread. (render done)
	tinfo->threadComplete = true;
	tinfo->currentTileNum = -1;
	tinfo->currentTileIdx = -1;
#ifdef WINDOWS
	return 0;
#else
	pthread_exit((void*) arg);
#endif
}
	
struct renderer *newRenderer() {
	struct renderer *r = calloc(1, sizeof(struct renderer));
	r->state.avgTileTime = (time_t)1;
	r->state.timeSampleCount = 1;
	r->prefs.fileMode = saveModeNormal;
	r->state.image = newTexture();
	
	r->scene = calloc(1, sizeof(struct world));
	r->scene->camera = calloc(1, sizeof(struct camera));
	r->scene->ambientColor = calloc(1, sizeof(struct gradient));
	r->scene->hdr = NULL; //Optional, to be loaded later
	r->scene->meshes = calloc(1, sizeof(struct mesh));
	r->scene->spheres = calloc(1, sizeof(struct sphere));
	
#ifdef UI_ENABLED
	r->mainDisplay = calloc(1, sizeof(struct display));
	r->mainDisplay->window = NULL;
	r->mainDisplay->renderer = NULL;
	r->mainDisplay->texture = NULL;
	r->mainDisplay->overlayTexture = NULL;
#else
	logr(warning, "Render preview is disabled. (No SDL2)\n");
#endif
	
	//Mutex
#ifdef _WIN32
	r->state.statsMutex = CreateMutex(NULL, FALSE, NULL);
#else
	r->state.statsMutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif

	return r;
}
	
//TODO: Refactor this to retrieve pixel from a given buffer, so we can reuse it for texture maps
color getPixel(struct renderer *r, int x, int y) {
	color output = {0.0f, 0.0f, 0.0f, 0.0f};
	output.r = r->state.image->byte_data[(x + (*r->state.image->height - y) * *r->state.image->width)*3 + 0];
	output.g = r->state.image->byte_data[(x + (*r->state.image->height - y) * *r->state.image->width)*3 + 1];
	output.b = r->state.image->byte_data[(x + (*r->state.image->height - y) * *r->state.image->width)*3 + 2];
	output.a = 1.0;
	return output;
}
	
void freeRenderer(struct renderer *r) {
	if (r->scene) {
		freeScene(r->scene);
		free(r->scene);
	}
	if (r->state.image) {
		freeTexture(r->state.image);
		free(r->state.image);
	}
	if (r->state.renderTiles) {
		free(r->state.renderTiles);
	}
	if (r->state.renderBuffer) {
		freeTexture(r->state.renderBuffer);
		free(r->state.renderBuffer);
	}
	if (r->state.uiBuffer) {
		freeTexture(r->state.uiBuffer);
		free(r->state.uiBuffer);
	}
	if (r->state.threadPaused) {
		free(r->state.threadPaused);
	}
	if (r->state.threadStates) {
		free(r->state.threadStates);
	}
#ifdef UI_ENABLED
	if (r->mainDisplay) {
		freeDisplay(r->mainDisplay);
	}
#endif
	if (r->state.timers) {
		free(r->state.timers);
	}
	
	free(r);
}
