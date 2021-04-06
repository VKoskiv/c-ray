//
//  protocol.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 21/03/2021.
//  Copyright © 2021 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#include "../../libraries/cJSON.h"
#include <stdbool.h>

#define C_RAY_DEFAULT_PORT 2222
#define PROTO_VERSION "0.1"

struct renderTile;
struct renderClient;
struct texture;
struct renderer;

struct command {
	char *name;
	int id;
};

int matchCommand(struct command *cmdlist, size_t commandCount, char *cmd);

// Consumes given json, no need to free it after.
bool sendJSON(int socket, cJSON *json);

cJSON *readJSON(int socket);

cJSON *errorResponse(char *error);

cJSON *goodbye(void);

cJSON *newAction(char *action);

cJSON *encodeTile(struct renderTile tile);

struct renderTile decodeTile(const cJSON *json);

cJSON *encodeTexture(const struct texture *t);

struct texture *decodeTexture(const cJSON *json);

bool containsError(const cJSON *json);

bool containsGoodbye(const cJSON *json);

void disconnectFromClient(struct renderClient *client);
