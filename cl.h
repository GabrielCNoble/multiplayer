#ifndef CL_H
#define CL_H

#include "g.h"

void cl_Init();

void cl_RunClient();

uint32_t cl_Connect(char *client_name, char *server_address);

void cl_Disconnect();

void cl_SyncClients();

void cl_SyncFrames();

void cl_SendPlayerFrame(struct g_player_t *player);

struct g_player_t *cl_FindPlayerByClient(struct sv_client_t *client);


#endif // CL_H
