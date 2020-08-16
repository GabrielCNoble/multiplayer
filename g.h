#ifndef G_H
#define G_H

#include <stdint.h>
#include "math.h"
#include "sv.h"

enum G_PLAYER_TYPE
{
    G_PLAYER_TYPE_CONTROLLED = 0,
    G_PLAYER_TYPE_REMOTE,
    G_PLAYER_TYPE_NPC,
    G_PLAYER_TYPE_LAST
};

struct g_player_t
{
    struct g_player_t *next;
    struct sv_client_t *client;
    uint32_t type;
    vec2_t position;
    vec2_t velocity;
};

void g_Init();

struct g_player_t *g_CreatePlayer(uint32_t type, vec2_t *position);

void g_UpdatePlayers();

void g_BeginDraw();

void g_EndDraw();

void g_DrawPlayers();


#endif // PLAYER_H
