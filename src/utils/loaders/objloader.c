//
//  objloader.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 02/04/2019.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#include "../../includes.h"
#include "objloader.h"

#include "../../datatypes/mesh.h"
#include "../../datatypes/vector.h"
#include "../../datatypes/poly.h"
#include "../../datatypes/material.h"
#include "../../utils/logging.h"
#include "../../utils/string.h"
#include "../../datatypes/vertexbuffer.h"
#include "mtlloader.h"
#include "../fileio.h"
#include "../../utils/assert.h"
#include "../../utils/textbuffer.h"
#include "../../datatypes/vertexbuffer.h"

//TODO: REMOVE
#include "../../utils/platform/terminal.h"

#define ws " \t\n\r"

static int parseIndices(int *vertexIndex, int *normalIndex, int *textureIndex) {
	
	char *tempstr;
	char *token;
	int vertexCount = 0;
	
	while ((token = strtok(NULL, ws)) != NULL) {
		
		//texture and normal indices are optional
		if (textureIndex != NULL) {
			textureIndex[vertexCount] = 0;
		}
		if (normalIndex != NULL) {
			normalIndex[vertexCount] = 0;
		}
		
		vertexIndex[vertexCount] = atoi(token);
		
		if (stringContains(token, "//")) {
			//Only normal and vertex
			tempstr = strchr(token, '/');
			tempstr++;
			normalIndex[vertexCount] = atoi(++tempstr);
		} else if (stringContains(token, "/")) {
			tempstr = strchr(token, '/');
			textureIndex[vertexCount] = atoi(++tempstr);
			if (stringContains(tempstr, "/")) {
				tempstr = strchr(tempstr, '/');
				normalIndex[vertexCount] = atoi(++tempstr);
			}
		}
		
		vertexCount++;
	}
	
	return vertexCount;
}

static struct poly parsePolyOld(struct mesh mesh) {
	struct poly p;
	p.vertexCount = parseIndices(p.vertexIndex, p.normalIndex, p.textureIndex);
	
	/*convertIndices(vertexCount, p.vertexIndex);
	convertIndices(normalCount, p.normalIndex);
	convertIndices(textureCount, p.textureIndex);*/
	
	for (int i = 0; i < p.vertexCount; ++i) {
		if (p.normalIndex[i] == 0) {
			p.hasNormals = false;
		}
		
		p.vertexIndex[i] = mesh.firstVectorIndex + (p.vertexIndex[i] - 1);
		p.normalIndex[i] = mesh.firstNormalIndex + (p.normalIndex[i] - 1);
		p.textureIndex[i] = mesh.firstTextureIndex + (p.textureIndex[i] - 1);
	}
	
	return p;
}

static int findMaterialIndex(struct mesh *mesh, char *mtlName) {
	for (int i = 0; i < mesh->materialCount; ++i) {
		if (stringEquals(mesh->materials[i].name, mtlName)) {
			return i;
		}
	}
	return 0;
}

static struct mesh parseOBJMesh(textBuffer *segment) {
	(void)segment;
	return (struct mesh){0};
}

// Count lines starting with thing
static size_t count(textBuffer *buffer, const char *thing) {
	size_t thingCount = 0;
	char *head = firstLine(buffer);
	lineBuffer line = {0};
	while (head) {
		fillLineBuffer(&line, head, " ");
		char *first = firstToken(&line);
		if (stringEquals(first, thing)) thingCount++;
		head = nextLine(buffer);
	}
	logr(debug, "File contains %zu of %s\n", thingCount, thing);
	head = firstLine(buffer);
	return thingCount;
}

enum currentMode {
	None,
	Mesh
};

/*
 o = new object starting, so for us that's a new 'segment'
 mtllib = new material set. Usually one per file, is it always that?
 v = new vector
 vn = new normal
 f = new face
 usemtl = switch to that material
 
 
 */

/*
struct mesh {
	//Vertices
	int vertexCount;
	int firstVectorIndex;
	//Normals
	int normalCount;
	int firstNormalIndex;
	//Texture coordinates
	int textureCount;
	int firstTextureIndex;
	//Faces
	struct poly *polygons;
	int polyCount;
	//Materials
	int materialCount;
	struct material *materials;
	struct bvh *bvh;
	float rayOffset;
	char *name;
};*/

static struct vector parseVertex(lineBuffer *line) {
	ASSERT(line->amountOf.tokens == 4);
	return (struct vector){atof(nextToken(line)), atof(nextToken(line)), atof(nextToken(line))};
}

static struct coord parseCoord(lineBuffer *line) {
	ASSERT(line->amountOf.tokens == 3);
	return (struct coord){atof(nextToken(line)), atof(nextToken(line))};
}

static struct poly parsePolygon(lineBuffer *line) {
	ASSERT(line->amountOf.tokens == 4);
	struct poly p = {0};
	p.vertexCount = MAX_CRAY_VERTEX_COUNT;
	for (int i = 0; i < p.vertexCount; ++i) {
		// Order goes v/vt/vn
		lineBuffer batch = {0};
		fillLineBuffer(&batch, nextToken(line), "/");
		p.vertexIndex[i] = atoi(firstToken(&batch));
		p.textureIndex[i] = nextToken(&batch) ? atoi(currentToken(&batch)) : 0; // Optional
		p.normalIndex[i] = atoi(nextToken(&batch));
	}
	// Wavefront indices always begin from 1, and we zero above^
	p.hasNormals = p.normalIndex[0] == 0;
	return p;
}

struct mesh *parseWavefront(const char *filePath, size_t *finalMeshCount) {
	logr(debug, "Loading OBJ at %s\n", filePath);
	size_t bytes = 0;
	char *rawText = loadFile(filePath, &bytes);
	if (!rawText) return NULL;
	textBuffer *file = newTextBuffer(rawText);
	
	//Start processing line-by-line, state machine style.
	size_t meshCount = count(file, "o");
	size_t currentMesh = 0;
	size_t valid_meshes = 0;
	
	// Allocate local buffers (memcpy these to global buffers if parsing succeeds)
	size_t fileVertices = count(file, "v");
	size_t currentVertex = 0;
	struct vector *vertices = malloc(fileVertices * sizeof(*vertices));
	size_t fileTexCoords = count(file, "vt");
	size_t currentTextureCoord = 0;
	struct coord *texCoords = malloc(fileTexCoords * sizeof(*texCoords));
	size_t fileNormals = count(file, "vn");
	size_t currentNormal = 0;
	struct vector *normals = malloc(fileNormals * sizeof(*normals));
	size_t filePolys = count(file, "f");
	size_t currentPoly = 0;
	struct poly *polygons = malloc(filePolys * sizeof(*polygons));
	
	struct material *materialSet = NULL;
	int materialCount = 0;
	int currentMaterial = 0;
	
	struct mesh *meshes = calloc(meshCount, sizeof(*meshes));
	struct mesh *currentMeshPtr = NULL;
	
	char *head = firstLine(file);
	lineBuffer line = {0};
	while (head) {
		fillLineBuffer(&line, head, " ");
		char *first = firstToken(&line);
		if (first[0] == '#') {
			head = nextLine(file);
			continue;
		} else if (first[0] == 'o') {
			currentMeshPtr = &meshes[currentMesh++];
			currentMeshPtr->name = stringCopy(peekNextToken(&line));
			valid_meshes++;
		} else if (first[0] == 'g') {
			// New object group, ignore for now.
		} else if (stringEquals(first, "v")) {
			vertices[currentVertex++] = parseVertex(&line);
			currentMeshPtr->vertexCount++;
		} else if (stringEquals(first, "vt")) {
			texCoords[currentTextureCoord++] = parseCoord(&line);
			currentMeshPtr->textureCount++;
		} else if (stringEquals(first, "vn")) {
			normals[currentNormal++] = parseVertex(&line);
			currentMeshPtr->normalCount++;
		} else if (first[0] == 'f') {
			struct poly p = parsePolygon(&line);
			p.materialIndex = currentMaterial;
			polygons[currentPoly++] = p;
		} else if (stringEquals(first, "newmtl")) {
			char *mtlFilePath = stringConcat(getFilePath(filePath), peekNextToken(&line));
			materialSet = parseMTLFile(mtlFilePath, &materialCount);
			free(mtlFilePath);
		}
		head = nextLine(file);
	}
	
	if (finalMeshCount) *finalMeshCount = valid_meshes;
	freeTextBuffer(file);
	free(rawText);

	/*for (size_t i = 0; i < meshCount; ++i) {
		meshes[i].materials = materialSet;
		meshes[i].materialCount = materialCount;
	}*/
	currentMeshPtr->materials = calloc(1, sizeof(struct material));
	currentMeshPtr->materials[0] = warningMaterial();//materialSet;
	currentMeshPtr->materialCount = 1;//materialCount;
	currentMeshPtr->polygons = polygons;
	currentMeshPtr->polyCount = (int)filePolys;
	currentMeshPtr->firstVectorIndex = vertexCount;
	currentMeshPtr->firstNormalIndex = normalCount;
	currentMeshPtr->firstTextureIndex = textureCount;
	
	vertexCount += fileVertices;
	normalCount += fileNormals;
	textureCount+= fileTexCoords;
	
	g_vertices = realloc(g_vertices, vertexCount * sizeof(struct vector));
	for (size_t i = 0; i < fileVertices; ++i) {
		g_vertices[currentMeshPtr->firstVectorIndex + i] = vertices[i];
	}
	//memcpy(g_vertices + vertexCount, vertices, fileVertices * sizeof(*vertices));
	
	g_normals = realloc(g_normals, normalCount * sizeof(struct vector));
	for (size_t i = 0; i < fileNormals; ++i) {
		g_normals[currentMeshPtr->firstNormalIndex + i] = normals[i];
	}
	//memcpy(g_normals + normalCount, normals, fileNormals * sizeof(*normals));
	
	g_textureCoords = realloc(g_textureCoords, textureCount * sizeof(struct coord));
	for (size_t i = 0; i < fileTexCoords; ++i) {
		g_textureCoords[currentMeshPtr->firstTextureIndex + i] = texCoords[i];
	}
	//memcpy(g_textureCoords + textureCount, texCoords, fileTexCoords * sizeof(*texCoords));
	
	//exit(0);
	return meshes;
}

struct mesh *parseOBJFileNah(char *filePath, size_t *meshCountOut) {
	logr(debug, "Loading OBJ file\n");
	char *rawText = loadFile(filePath, NULL);
	textBuffer *file = newTextBuffer(rawText);
	
	//Figure out how many meshes this file contains
	size_t meshCount = count(file, "o");
	if (!meshCount) return NULL;

	//Get the offsets
	char *head = firstLine(file);
	lineBuffer line = {0};
	size_t *meshOffsets = malloc(meshCount * sizeof(*meshOffsets));
	int i = 0;
	while (head) {
		fillLineBuffer(&line, head, " ");
		char *first = firstToken(&line);
		if (first[0] == 'o') meshOffsets[i++] = file->current.line;
		head = nextLine(file);
	}
	logr(debug, "They start on lines:\n");
	
	for (size_t l = 0; l < meshCount; ++l) {
		logr(debug, "%zu\n", meshOffsets[l]);
	}
	head = firstLine(file);
	
	struct mesh *meshes = calloc(meshCount, sizeof(*meshes));
	
	for (size_t m = 0; m < meshCount; ++m) {
		size_t start = meshOffsets[m];
		size_t end = meshCount == 1 ? file->amountOf.lines : meshOffsets[m + 1] - meshOffsets[m];
		textBuffer *segment = newTextView(file, start, end);
		meshes[m] = parseOBJMesh(segment);
		freeTextBuffer(segment);
	}
	
	free(meshOffsets);
	freeLineBuffer(&line);
	freeTextBuffer(file);
	restoreTerminal();
	if (meshCountOut) *meshCountOut = meshCount;
	exit(0);
	return NULL;
}

//Parse a Wavefront OBJ file and return a generic mesh.
//Note: This will also alter the global vertex arrays
//We will add vertices to them as the OBJ is loaded.
struct mesh *parseOBJFilea(char *filePath) {
	struct mesh *newMesh = calloc(1, sizeof(*newMesh));
	
	int linenum = 0;
	char *token;
	char currLine[500];
	FILE *fileStream;
	
	fileStream = fopen(filePath, "r");
	if (fileStream == 0) {
		logr(warning, "OBJ not found at %s\n", filePath);
		free(newMesh);
		return NULL;
	}
	
	//First, find vertex counts
	int vCount = 0;
	int nCount = 0;
	int tCount = 0;
	int pCount = 0;
	
	while (fgets(currLine, 500, fileStream)) {
		token = strtok(currLine, ws);
		//skip comments
		if(token == NULL || stringEquals(token, "#")) {
			continue;
		} else if (stringEquals(token, "v")) {
			vCount++;
		} else if (stringEquals(token, "vn")) {
			nCount++;
		} else if (stringEquals(token, "vt")) {
			tCount++;
		} else if (stringEquals(token, "f")) {
			pCount++;
		}
	}
	fseek(fileStream, 0, SEEK_SET);
	
	newMesh->firstVectorIndex = vertexCount;
	newMesh->firstNormalIndex = normalCount;
	newMesh->firstTextureIndex = textureCount;
	
	newMesh->vertexCount = vCount;
	newMesh->normalCount = nCount;
	newMesh->textureCount = tCount;
	newMesh->polyCount = pCount;
	
	vertexCount += vCount;
	g_vertices = realloc(g_vertices, vertexCount * sizeof(struct vector));
	normalCount += nCount;
	g_normals = realloc(g_normals, normalCount * sizeof(struct vector));
	textureCount += tCount;
	g_textureCoords = realloc(g_textureCoords, textureCount * sizeof(struct coord));
	
	newMesh->polygons = malloc(pCount * sizeof(struct poly));
	
	//newMesh->name;
	//newMesh->materials;
	//newMesh->materialCount;
	
	int currMatIdx = 0;
	int currVecIdx = 0;
	int currNorIdx = 0;
	int currTexIdx = 0;
	int currPolIdx = 0;
	
	while (fgets(currLine, 500, fileStream)) {
		token = strtok(currLine, ws);
		linenum++;
		
		if (token == NULL || stringEquals(token, "#")) {
			//skip comments
			continue;
		} else if (stringEquals(token, "v")) {
			//Vertex
			//g_vertices[newMesh->firstVectorIndex + currVecIdx] = parseVertex();
			currVecIdx++;
		} else if (stringEquals(token, "vn")) {
			//Normal
			//g_normals[newMesh->firstNormalIndex + currNorIdx] = parseVertex();
			currNorIdx++;
		} else if (stringEquals(token, "vt")) {
			//Texture coord
			//g_textureCoords[newMesh->firstTextureIndex + currTexIdx] = parseCoord();
			currTexIdx++;
		} else if (stringEquals(token, "f")) {
			//Polygon
			//struct poly p = parsePoly(*newMesh);
			//p.materialIndex = currMatIdx;
			//newMesh->polygons[currPolIdx] = p;
			currPolIdx++;
		} else if (stringEquals(token, "usemtl")) {
			//current material index
			currMatIdx = findMaterialIndex(newMesh, strtok(NULL, ws));
		} else if (stringEquals(token, "mtllib")) {
			//new material
			int mtlCount = 0;
			//I am so very sorry for this. I didn't mean to. Just wanted to get it working.
			char *fullPath = (char*)calloc(1024, sizeof(char));
			sprintf(fullPath, "%s", filePath);
			char key = '\0';
			int i = (int)strcspn(filePath, &key);
			fullPath[i-3] = 'm';
			fullPath[i-2] = 't';
			fullPath[i-1] = 'l';
			
			struct material *newMats = parseMTLFile(fullPath, &mtlCount);
			
			if (newMats != NULL) {
				newMesh->materialCount = mtlCount;
				newMesh->materials = newMats;
			} else {
				newMesh->materials = calloc(1, sizeof(*newMesh->materials));
				newMesh->materials[0] = warningMaterial();
				newMesh->materialCount = 1;
			}
			free(fullPath);
		} else if (stringEquals(token, "o")) {
			//object name
			newMesh->name = stringCopy(strtok(NULL, ws));
		} else if (stringEquals(token, "s")) {
			//smoothShading
			//TODO
		} else {
			logr(warning, "Unrecognized command '%s' in OBJ file %s on line %i\n", token, filePath, linenum);
		}
	}
	
	fclose(fileStream);
	
	return newMesh;
}
