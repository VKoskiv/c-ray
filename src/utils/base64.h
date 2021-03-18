//
//  base64.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 18/03/2021.
//  Copyright © 2021 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include <stddef.h>

char *b64encode(const void *data, const size_t length);
char *b64decode(const void *data, const size_t length);
