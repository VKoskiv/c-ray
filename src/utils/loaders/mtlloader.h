//
//  mtlloader.h
//  C-ray
//
//  Created by Valtteri Koskivuori on 02/04/2019.
//  Copyright © 2015-2019 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "../../datatypes/material.h"

struct material *parseMTLFile(char *filePath, int *mtlCount);
