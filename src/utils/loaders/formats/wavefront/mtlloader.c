//
//  mtlloader.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 02/04/2019.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

// C-ray MTL file parser

#include "../../../../includes.h"
#include "mtlloader.h"

#include "../../../../datatypes/material.h"
#include "../../../logging.h"
#include "../../../string.h"
#include "../../../textbuffer.h"
#include "../../../fileio.h"
#include "../../../assert.h"
#include "../../textureloader.h"

static size_t countMaterials(textBuffer *buffer) {
	size_t mtlCount = 0;
	char *head = firstLine(buffer);
	while (head) {
		if (stringStartsWith("newmtl", head)) mtlCount++;
		head = nextLine(buffer);
	}
	logr(debug, "File contains %zu materials\n", mtlCount);
	head = firstLine(buffer);
	return mtlCount;
}

struct color parseColor(lineBuffer *line) {
	ASSERT(line->amountOf.tokens == 4);
	return (struct color){atof(nextToken(line)), atof(nextToken(line)), atof(nextToken(line)), 1.0f};
}

struct material *parseMTLFile(char *filePath, int *mtlCount) {
	size_t bytes = 0;
	char *rawText = loadFile(filePath, &bytes);
	if (!rawText) return NULL;
	logr(debug, "Loading MTL at %s\n", filePath);
	textBuffer *file = newTextBuffer(rawText);
	free(rawText);
	
	char *assetPath = getFilePath(filePath);
	
	size_t materialAmount = countMaterials(file);
	struct material *materials = calloc(materialAmount, sizeof(*materials));
	size_t currentMaterialIdx = 0;
	struct material *current = NULL;
	
	char *head = firstLine(file);
	lineBuffer *line = newLineBuffer();
	while (head) {
		fillLineBuffer(line, head, ' ');
		char *first = firstToken(line);
		if (first[0] == '#') {
			head = nextLine(file);
			continue;
		} else if (head[0] == '\0') {
			logr(debug, "empty line\n");
			head = nextLine(file);
			continue;
		} else if (stringEquals(first, "newmtl")) {
			current = &materials[currentMaterialIdx++];
			if (!peekNextToken(line)) {
				logr(warning, "newmtl without a name on line %zu\n", line->current.line);
				free(materials);
				return NULL;
			}
			current->name = stringCopy(peekNextToken(line));
		} else if (stringEquals(first, "Ka")) {
			current->ambient = parseColor(line);
		} else if (stringEquals(first, "Kd")) {
			current->diffuse = parseColor(line);
		} else if (stringEquals(first, "Ks")) {
			current->specular = parseColor(line);
		} else if (stringEquals(first, "Ke")) {
			current->emission = parseColor(line);
		} else if (stringEquals(first, "Ns")) {
			// Shinyness, unused
			head = nextLine(file);
			continue;
		} else if (stringEquals(first, "d")) {
			current->transparency = atof(nextToken(line));
		} else if (stringEquals(first, "r")) {
			current->reflectivity = atof(nextToken(line));
		} else if (stringEquals(first, "sharpness")) {
			current->glossiness = atof(nextToken(line));
		} else if (stringEquals(first, "Ni")) {
			current->IOR = atof(nextToken(line));
		} else if (stringEquals(first, "map_Kd")) {
			char *path = stringConcat(assetPath, nextToken(line));
			current->texture = loadTexture(path, NULL);
			free(path);
		} else if (stringEquals(first, "norm")) {
			char *path = stringConcat(assetPath, nextToken(line));
			current->normalMap = loadTexture(path, NULL);
			free(path);
		} else if (stringEquals(first, "map_Ns")) {
			char *path = stringConcat(assetPath, nextToken(line));
			current->specularMap = loadTexture(path, NULL);
			free(path);
		} else {
			char *fileName = getFileName(filePath);
			logr(debug, "Unknown statement \"%s\" in MTL \"%s\" on line %zu\n",
				first, fileName, file->current.line);
			free(fileName);
		}
		head = nextLine(file);
	}
	
	destroyLineBuffer(line);
	freeTextBuffer(file);
	free(assetPath);
	if (mtlCount) *mtlCount = (int)materialAmount;
	return materials;
}
