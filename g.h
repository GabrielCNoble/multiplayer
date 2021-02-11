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

enum G_PLAYER_FACING_DIR
{
    G_PLAYER_FACING_DIR_LEFT = 0,
    G_PLAYER_FACING_DIR_RIGHT,
};

enum G_PLAYER_INPUT
{
    G_PLAYER_INPUT_MOVE_LEFT = 1,
    G_PLAYER_INPUT_MOVE_RIGHT = 1 << 1,
    G_PLAYER_INPUT_JUMP
};

struct g_box_t
{
    vec2_t position;
    vec2_t size;
    vec2_t rot;
};

struct g_player_t
{
    struct g_player_t *next;
    struct g_player_t *prev;
    struct sv_client_t *client;
    uint32_t type;
    uint32_t facing_dir;
    uint32_t input_mask;
    vec2_t position;
    vec2_t velocity;
};

struct g_tile_t
{
    struct g_tile_t *next;
    struct g_tile_t *prev;
    vec2_t position;
    vec2_t size;
    float orientation;
};

struct g_dbvh_node_t
{
    vec2_t min;
    vec2_t max;
    struct g_dbvh_node_t *parent;
    struct g_dbvh_node_t *left;
    struct g_dbvh_node_t *right;
    struct g_tile_t *tile;
};

struct g_tile_list_t
{
    struct g_tile_t **tiles;
    uint32_t tile_count;
    uint32_t max_tiles;
};

void g_Init();

struct g_player_t *g_CreatePlayer(uint32_t type, vec2_t *position);

void g_DestroyPlayer(struct g_player_t *player);

struct g_tile_t *g_CreateTile(vec2_t *position, vec2_t *size, float orientation);

void g_DestroyTile(struct g_tile_t *tile);

void g_PlayerInput(float delta_time);

void g_UpdatePlayers(float delta_time);

void g_UpdateCamera(float delta_time);

void g_BuildDbvh();

struct g_tile_list_t *g_BoxOnDbvh(vec2_t *box_min, vec2_t *box_max);

uint32_t g_CollidePlayer(struct g_player_t *player);

void g_BoxEdge(struct g_box_t *box, vec2_t *direction, vec2_t *v0, vec2_t *v1, vec2_t *normal);

void g_BeginDraw();

void g_EndDraw();

void g_DrawPlayers();

void g_DrawTiles();

void g_DrawDbvh();


#endif // PLAYER_H
