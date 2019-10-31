//
//  mtlloader.c
//  C-ray
//
//  Created by Valtteri Koskivuori on 02/04/2019.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

// C-ray MTL file parser

#include "../../includes.h"
#include "mtlloader.h"

#include "../../datatypes/material.h"
#include "../../utils/logging.h"
#include "../../utils/filehandler.h" //for copyString FIXME
//#include "../../datatypes/scene.h" // for getFileName FIXME

// Parse a list of materials and return an array of materials.
// mtlCount is the amount of materials loaded.
struct material *parseMTLFile(char *filePath, int *mtlCount) {
	struct material **newMaterials = calloc(1, sizeof(struct material));
	
	int count = 1;
	int linenum = 0;
	char *token;
	char currLine[500];
	FILE *fileStream;
	struct material *currMat = NULL;
	bool matOpen = false;
	
	fileStream = fopen(filePath, "r");
	if (fileStream == 0) {
		logr(warning, "Material not found at %s\n", filePath);
		free(newMaterials);
		return NULL;
	}
	
	while (fgets(currLine, 500, fileStream)) {
		token = strtok(currLine, " \t\n\r");
		linenum++;
		
		if (token == NULL || stringEquals(token, "//") || stringEquals(token, "#")) {
			//Skip comments starting with // or #
			continue;
		} else if (stringEquals(token, "newmtl")) {
			//New material is created
			newMaterials = realloc(newMaterials, count * sizeof(struct material));
			currMat = newMaterials[count-1];
			currMat = newMaterial(MATERIAL_TYPE_DEFAULT);
			//newMaterials[count-1].name = calloc(CRAY_MATERIAL_NAME_SIZE, sizeof(char));
			//currMat->textureFilePath = calloc(CRAY_MESH_FILENAME_LENGTH, sizeof(char));
			//strncpy(newMaterials[count-1].name, strtok(NULL, " \t"), CRAY_MATERIAL_NAME_SIZE);
			count++;
			matOpen = true;
		} else if (stringEquals(token, "Ka") && matOpen) {
			//Ambient color
			//currMat->ambient.red = atof(strtok(NULL, " \t"));
			//currMat->ambient.green = atof(strtok(NULL, " \t"));
			//currMat->ambient.blue = atof(strtok(NULL, " \t"));
		} else if (stringEquals(token, "Kd") && matOpen) {
			//Diffuse color
			setMaterialVec3(currMat, "albedo", (vec3) { atof(strtok(NULL, " \t")), atof(strtok(NULL, " \t")), atof(strtok(NULL, " \t")) });
		} else if (stringEquals(token, "Ks") && matOpen) {
			//Specular color
			//currMat->specular.red = atof(strtok(NULL, " \t"));
			//currMat->specular.green = atof(strtok(NULL, " \t"));
			//currMat->specular.blue = atof(strtok(NULL, " \t"));
		} else if (stringEquals(token, "Ke") && matOpen) {
			//Emissive color
			//currMat->emission.red = atof(strtok(NULL, " \t"));
			//currMat->emission.green = atof(strtok(NULL, " \t"));
			//currMat->emission.blue = atof(strtok(NULL, " \t"));
		} else if (stringEquals(token, "Ns") && matOpen) {
			//Shinyness
			//UNUSED
		} else if (stringEquals(token, "d") && matOpen) {
			//Transparency
			//currMat->transparency = atof(strtok(NULL, " \t"));
		} else if (stringEquals(token, "r") && matOpen) {
			//Reflectivity
			//currMat->reflectivity = atof(strtok(NULL, " \t"));
		} else if (stringEquals(token, "sharpness") && matOpen) {
			//Glossiness
			setMaterialFloat(currMat, "roughness", 1.0 - atof(strtok(NULL, " \t")));
		} else if (stringEquals(token, "Ni") && matOpen) {
			//IOR
			setMaterialFloat(currMat, "ior", atof(strtok(NULL, " \t")));
		} else if (stringEquals(token, "illum") && matOpen) {
			//Illumination type
			//UNUSED
		} else if (stringEquals(token, "map_Kd") && matOpen) {
			//Diffuse texture map
			//strncpy(currMat->textureFilePath, strtok(NULL, " \t"), CRAY_MESH_FILENAME_LENGTH);
		} else if ((stringEquals(token, "map_bump") || stringEquals(token, "bump")) && matOpen) {
			//Bump map
			//TODO
		} else if (stringEquals(token, "map_d") && matOpen) {
			//Alpha channel? Not needed I think
		} else {
			logr(warning, "Unrecognized command '%s' in mtl file %s on line %i\n", token, filePath, linenum);
		}
	}
	
	fclose(fileStream);
	
	*mtlCount = count-1;
	
	return newMaterials;
}
