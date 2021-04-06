//
//  server.h
//  C-Ray
//
//  Created by Valtteri Koskivuori on 06/04/2021.
//  Copyright © 2021 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#ifndef WINDOWS
#include <arpa/inet.h>
#endif

struct renderer;

enum clientState {
	Disconnected,
	Connected,
	ConnectionFailed,
	Syncing,
	SyncFailed,
	Synced,
	Rendering,
	Finished
};

struct renderClient {
#ifndef WINDOWS
	struct sockaddr_in address;
#endif
	enum clientState state;
	int availableThreads;
	int socket;
	int id;
};

// Synchronise renderer state with clients, and return a list of clients
// ready to do some rendering
struct renderClient *syncWithClients(const struct renderer *r, size_t *count);

void *networkRenderThread(void *arg);
