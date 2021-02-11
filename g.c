#include "g.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_render.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

struct g_player_t *g_players = NULL;
struct g_player_t *g_last_player = NULL;
struct g_player_t *g_main_player = NULL;


struct g_dbvh_node_t *g_dbvh = NULL;
struct g_tile_t *g_tiles = NULL;
struct g_tile_t *g_last_tile = NULL;
uint32_t g_tile_count = 0;

struct g_tile_list_t g_tile_list;

vec2_t g_camera_pos;

SDL_Event g_event;
SDL_Window *g_window;
SDL_Renderer *g_renderer;
uint32_t g_window_width;
uint32_t g_window_height;

//SDL_Texture *g_solid_white_texture;
//SDL_Texture *g_solid_red_texture;

SDL_Texture *g_default_texture;

#define G_FRICTION_COEF 5.5
#define G_PLAYER_MASS 50.0
#define G_GRAVITY 9.8
#define G_PLAYER_SIZE ((vec2_t){15.0, 25.0})
#define G_POINT_TO_SDL_SPACE(v) (vec2_t){(v)->x + (g_window_width / 2), (g_window_height / 2) - (v)->y}

extern struct sv_sync_frames_t *cl_sync_frames;

void g_Init()
{
    g_window_width = 800;
    g_window_height = 600;
    g_window = SDL_CreateWindow("game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g_window_width, g_window_height, 0);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    g_tile_list.max_tiles = 512;
    g_tile_list.tile_count = 0;
    g_tile_list.tiles = calloc(g_tile_list.max_tiles, sizeof(struct g_tile_t *));
    
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    
    g_CreatePlayer(G_PLAYER_TYPE_CONTROLLED, &(vec2_t){40.0, 80.0});
    g_CreateTile(&(vec2_t){0.0, 0.0}, &(vec2_t){80.0, 20.0}, 0.0);
//    g_CreateTile(&(vec2_t){-40.0, 0.0}, &(vec2_t){80.0, 20.0}, -0.08);

    g_BuildDbvh();
    g_camera_pos = (vec2_t){0.0, 0.0};
    SDL_RaiseWindow(g_window);
    
    char *pixel_data;
    uint32_t row_pitch;
    
    g_default_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 2, 2);
    SDL_LockTexture(g_default_texture, NULL, &pixel_data, &row_pitch);
    pixel_data[0 ] = 0xff;
    pixel_data[1 ] = 0x0f;
    pixel_data[2 ] = 0x0f;
    pixel_data[3 ] = 0x0f;
    
    pixel_data[4 ] = 0xff;
    pixel_data[5 ] = 0xe0;
    pixel_data[6 ] = 0xe0;
    pixel_data[7 ] = 0xe0;
    
    pixel_data[8 ] = 0xff;
    pixel_data[9 ] = 0xe0;
    pixel_data[10] = 0xe0;
    pixel_data[11] = 0xe0;
    
    pixel_data[12] = 0xff;
    pixel_data[13] = 0xe0;
    pixel_data[14] = 0xe0;
    pixel_data[15] = 0xe0;
    
    SDL_UnlockTexture(g_default_texture);
    
//    g_solid_red_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
//    SDL_LockTexture(g_solid_red_texture, NULL, &pixel_data, &row_pitch);
//    pixel_data[0] = 0xff;
//    pixel_data[1] = 0;
//    pixel_data[2] = 0;
//    pixel_data[3] = 0xff;
//    SDL_UnlockTexture(g_solid_red_texture);
}

struct g_player_t *g_CreatePlayer(uint32_t type, vec2_t *position)
{
    struct g_player_t *player = NULL;
    
    if(type == G_PLAYER_TYPE_CONTROLLED && g_main_player)
    {
        return g_main_player;
    }
    
    player = calloc(1, sizeof(struct g_player_t));

    player->position = *position;
    player->type = type;
    player->velocity = (vec2_t){0.0, 0.0};
    
    if(!g_players)
    {
        g_players = player;
    }
    else
    {
        g_last_player->next = player;
        player->prev = g_last_player;
    }
    
    g_last_player = player;
    
    if(type == G_PLAYER_TYPE_CONTROLLED)
    {
        g_main_player = player;
    }
    
    return player;
}

void g_DestroyPlayer(struct g_player_t *player)
{
    if(player)
    {
        if(player->next)
        {
            player->next->prev = player->prev;
        }
        else
        {
            g_last_player = g_last_player->prev;
        }
        
        if(player->prev)
        {
            player->prev->next = player->next;
        }
        else
        {
            g_players = g_players->next;
        }
        
        free(player);
    }
}

struct g_tile_t *g_CreateTile(vec2_t *position, vec2_t *size, float orientation)
{
    struct g_tile_t *tile;
    
    tile = calloc(1, sizeof(struct g_tile_t));
    tile->orientation = orientation;
    tile->position = *position;
    tile->size = *size;
    
    if(!g_tiles)
    {
        g_tiles = tile;
    }
    else
    {
        g_last_tile->next = tile;
        tile->prev = g_last_tile;
    }
    
    g_last_tile = tile;
    g_tile_count++;
    return tile;
}

void g_DestroyTile(struct g_tile_t *tile)
{
    
}

#define G_PLAYER_MAX_HORIZONTAL_VELOCITY 5.0
#define G_PLAYER_HORIZONTAL_VELOCITY_INCREMENT 20.2

void g_PlayerInput(float delta_time)
{
    SDL_PollEvent(&g_event);
    const uint8_t *keyboard_state = SDL_GetKeyboardState(NULL);
    
    if(g_main_player)
    {
        g_main_player->input_mask = 0;
        
        if(keyboard_state[SDL_SCANCODE_LEFT])
        {
            g_main_player->velocity.x -= G_PLAYER_HORIZONTAL_VELOCITY_INCREMENT * delta_time;
            g_main_player->facing_dir = G_PLAYER_FACING_DIR_LEFT;
            g_main_player->input_mask |= G_PLAYER_INPUT_MOVE_LEFT;
        }
        
        if(keyboard_state[SDL_SCANCODE_RIGHT])
        {
            g_main_player->velocity.x += G_PLAYER_HORIZONTAL_VELOCITY_INCREMENT * delta_time;
            g_main_player->facing_dir = G_PLAYER_FACING_DIR_RIGHT;
            g_main_player->input_mask |= G_PLAYER_INPUT_MOVE_RIGHT;
        }
        
//        if(keyboard_state[SDL_SCANCODE_UP])
//        {
//            g_main_player->velocity.y += G_PLAYER_HORIZONTAL_VELOCITY_INCREMENT * delta_time;
//        }
//        
//        if(keyboard_state[SDL_SCANCODE_DOWN])
//        {
//            g_main_player->velocity.y -= G_PLAYER_HORIZONTAL_VELOCITY_INCREMENT * delta_time;
//        }
                
        if(g_main_player->velocity.x > G_PLAYER_MAX_HORIZONTAL_VELOCITY)
        {
            g_main_player->velocity.x = G_PLAYER_MAX_HORIZONTAL_VELOCITY;
        }
        else if (g_main_player->velocity.x < -G_PLAYER_MAX_HORIZONTAL_VELOCITY)
        {
            g_main_player->velocity.x = -G_PLAYER_MAX_HORIZONTAL_VELOCITY;
        }
    }
}

void g_UpdatePlayers(float delta_time)
{
    struct g_player_t *player = g_players;
    struct sv_player_frame_t *player_frame;
    vec2_t friction;
    const uint8_t *keyboard_state;
//    struct g_box_t box;
//    vec2_t direction;
//    vec2_t v0;
//    vec2_t v1;
//    vec2_t p0;
//    vec2_t p1;
    while(player)
    {
        switch(player->type)
        {
            case G_PLAYER_TYPE_CONTROLLED:
                player->velocity.y -= G_GRAVITY * delta_time;
//                vec2_t_add(&player->position, &player->position, &player->velocity);
                
                if(!(player->input_mask & (G_PLAYER_INPUT_MOVE_LEFT | G_PLAYER_INPUT_MOVE_RIGHT)))
                {
                    player->velocity.x -= player->velocity.x * G_FRICTION_COEF * delta_time;
//                    player->velocity.y -= player->velocity.y * G_FRICTION_COEF * delta_time;
                }
                
                if(g_CollidePlayer(player))
                {
                    vec2_t_add(&player->position, &player->position, &player->velocity);
                }
                
//                vec2_t_sub(&direction, &player->position, &g_tiles->position);
                
//                box.position = g_tiles->position;
//                box.size = g_tiles->size;
//                box.rot = (vec2_t){cos(g_tiles->orientation * 3.14159265), sin(g_tiles->orientation * 3.14159265)};
//                g_BoxEdge(&box, &direction, &v0, &v1);
            break;
            
            case G_PLAYER_TYPE_REMOTE:
                player_frame = NULL;
                for(uint32_t frame_index = 0; frame_index < cl_sync_frames->frame_count; frame_index++)
                {
                    if(cl_sync_frames->frames[frame_index].client == player->client)
                    {
                        player_frame = cl_sync_frames->frames + frame_index;
                        break;
                    }
                }
                
                if(player_frame)
                {
                    player->position = player_frame->frame.position;
                }   
            break;
        }
        
        player = player->next;
    }
}

void g_UpdateCamera(float delta_time)
{
    vec2_t ideal_camera_player_pos;
    vec2_t camera_player_pos_delta;
    vec2_t camera_player_pos;
    if(g_main_player)
    {
        switch(g_main_player->facing_dir)
        {
            case G_PLAYER_FACING_DIR_LEFT:
                vec2_t_add(&ideal_camera_player_pos, &ideal_camera_player_pos, &(vec2_t){0.0, 0.0});
            break;
            
            case G_PLAYER_FACING_DIR_RIGHT:
                vec2_t_add(&ideal_camera_player_pos, &ideal_camera_player_pos, &(vec2_t){-0.0, 0.0});
            break;
        }
        
        vec2_t_sub(&camera_player_pos, &g_main_player->position, &g_camera_pos);
        vec2_t_sub(&camera_player_pos_delta, &ideal_camera_player_pos, &camera_player_pos);
//        vec2_t_fmadd(&g_camera_pos, &g_camera_pos, &camera_player_pos_delta, -delta_time * 5.0);        
    }
}

struct g_dbvh_node_t *g_BuildDbvhRecursive(struct g_dbvh_node_t *nodes)
{
    struct g_dbvh_node_t *node_a = nodes;
    struct g_dbvh_node_t *parent_nodes = NULL;
    
    if(nodes && nodes->parent)
    {
        while(node_a)
        {
            struct g_dbvh_node_t *smallest_pair = NULL;
            struct g_dbvh_node_t *prev_smallest_pair = NULL;
            float smallest_area = FLT_MAX;
            struct g_dbvh_node_t *node_b = node_a->parent;
            struct g_dbvh_node_t *prev_b = NULL;
            
            vec2_t node_min = node_a->min;
            vec2_t node_max = node_a->max;
            
            while(node_b)
            {
                vec2_t min = node_a->min;
                vec2_t max = node_a->max;
                
                if(node_b->min.x < min.x)
                {
                    min.x = node_b->min.x;
                }
                
                if(node_b->min.y < min.y)
                {
                    min.y = node_b->min.y;
                }
                
                if(node_b->max.x > max.x)
                {
                    max.x = node_b->max.x;
                }
                
                if(node_b->max.y > max.y)
                {
                    max.y = node_b->max.y;
                }
                
                float area = (max.x - min.x) * (max.y - min.y);
                
                if(area < smallest_area)
                {
                    smallest_area = area;
                    smallest_pair = node_b;
                    prev_smallest_pair = prev_b;
                    node_min = min;
                    node_max = max;
                }
                prev_b = node_b;
                node_b = node_b->parent;
            }
            
            if(smallest_pair)
            {
                struct g_dbvh_node_t *node = calloc(1, sizeof(struct g_dbvh_node_t));
                node->left = node_a;
                node->right = smallest_pair;
                node->min = node_min;
                node->max = node_max;
                
                if(prev_smallest_pair)
                {
                    prev_smallest_pair->parent = smallest_pair->parent;
                }
                else
                {
                    node_a = smallest_pair;
                }
                
                node_a = node_a->parent;
                
                node->left->parent = node;
                node->right->parent = node;
                node->parent = parent_nodes;
                parent_nodes = node;
            }
            else
            {
                struct g_dbvh_node_t *next_node = node_a->parent;
                node_a->parent = parent_nodes;
                parent_nodes = node_a;
                node_a = next_node;
            }
        }
        
        return g_BuildDbvhRecursive(parent_nodes);
    }
    
    return nodes;
}

void g_BuildDbvh()
{
    struct g_tile_t *tile_a;
    struct g_tile_t **tiles;
    uint32_t tile_count = 0;
    struct g_dbvh_node_t *nodes = NULL;
    vec2_t top;
    vec2_t right;
    vec2_t corners[4];
    tile_a = g_tiles;
    
    while(tile_a)
    {
        struct g_dbvh_node_t *node = calloc(1, sizeof(struct g_dbvh_node_t));
        node->tile = tile_a;
        
        vec2_t orientation = (vec2_t){cos(tile_a->orientation * 3.14159265), sin(tile_a->orientation * 3.14159265)};
        
        vec2_t top = (vec2_t){-tile_a->size.y * orientation.y, tile_a->size.y * orientation.x};
        vec2_t right = (vec2_t){tile_a->size.x * orientation.x, tile_a->size.x * orientation.y};
        
        corners[0] = (vec2_t){top.x + right.x, top.y + right.y};
        corners[1] = (vec2_t){top.x - right.x, top.y + right.y};
        corners[2] = (vec2_t){top.x - right.x, top.y - right.y};
        corners[3] = (vec2_t){top.x + right.x, top.y - right.y};
        
        node->min = (vec2_t){0.0, 0.0};
        node->max = (vec2_t){0.0, 0.0};
        
        for(uint32_t corner_index = 0; corner_index < 4; corner_index++)
        {
            if(corners[corner_index].x < node->min.x)
            {
                node->min.x = corners[corner_index].x;
            }
            
            if(corners[corner_index].y < node->min.y)
            {
                node->min.y = corners[corner_index].y;
            }
            
            if(corners[corner_index].x > node->max.x)
            {
                node->max.x = corners[corner_index].x;
            }
            
            if(corners[corner_index].y > node->max.y)
            {
                node->max.y = corners[corner_index].y;
            }
        }
        
        vec2_t_sub(&node->min, &node->min, &tile_a->position);
        vec2_t_sub(&node->max, &node->max, &tile_a->position);
        
        node->parent = nodes;
        nodes = node;
        
        tile_a = tile_a->next;
    }
    
    g_dbvh = g_BuildDbvhRecursive(nodes);
}

void g_BoxOnDbvhRecursive(struct g_dbvh_node_t *node, vec2_t *box_min, vec2_t *box_max)
{
    if(node)
    {
        if(box_max->x > node->min.x && box_min->x < node->max.x)
        {
            if(box_max->y > node->min.y && box_min->y < node->max.y)
            {
                if(node->left == NULL && node->right == NULL)
                {
                    g_tile_list.tiles[g_tile_list.tile_count] = node->tile;
                    g_tile_list.tile_count++;
                }
                else
                {
                    g_BoxOnDbvhRecursive(node->left, box_min, box_max);
                    g_BoxOnDbvhRecursive(node->right, box_min, box_max);
                }
            }
        }
    }
}

struct g_tile_list_t *g_BoxOnDbvh(vec2_t *box_min, vec2_t *box_max)
{
    g_tile_list.tile_count = 0;
    g_BoxOnDbvhRecursive(g_dbvh, box_min, box_max);
    return &g_tile_list;
}

uint32_t g_CollidePlayer(struct g_player_t *player)
{
    struct g_tile_list_t *tile_list;
    
    vec2_t box_min;
    vec2_t box_max;
    vec2_t player_size = G_PLAYER_SIZE;
    
    vec2_t edges[4];
    vec2_t normals[2];
    vec2_t positions[2];
    vec2_t sum_edge[2];
    vec2_t direction;
    
    float smallest_t = 1.0;
    vec2_t smallest_normal;
    
    vec2_t_fmadd(&box_min, &player->position, &G_PLAYER_SIZE, -0.5);
    vec2_t_fmadd(&box_max, &player->position, &G_PLAYER_SIZE, 0.5);    
    
    if(player->velocity.x > 0.0)
    {
        box_max.x += player->velocity.x; 
    }
    else
    {
        box_min.x -= player->velocity.x;
    }
    
    
    if(player->velocity.y > 0.0)
    {
        box_max.y += player->velocity.y; 
    }
    else
    {
        box_min.y -= player->velocity.y;
    }
    
    tile_list = g_BoxOnDbvh(&box_min, &box_max);
    
    if(tile_list->tile_count)
    {
        for(uint32_t tile_index = 0; tile_index < tile_list->tile_count; tile_index++)
        {
            struct g_tile_t *tile = tile_list->tiles[tile_index];
            struct g_box_t box;
            
            vec2_t_sub(&direction, &player->position, &tile->position);
            
            positions[0] = tile->position;
            box.position = tile->position;
            box.size = tile->size;
            box.rot = (vec2_t){cos(tile->orientation * 3.14159265), sin(tile->orientation * 3.14159265)};
            /* find the closest edge on the tile to the player */
            g_BoxEdge(&box, &direction, &edges[0], &edges[1], &normals[0]);
            
            positions[1] = player->position;
            box.position = player->position;
            box.size = G_PLAYER_SIZE;
            box.rot = (vec2_t){1.0, 0.0};
            /* find the closest edge on the player to the tile */
            g_BoxEdge(&box, &direction, &edges[2], &edges[3], &normals[1]);
            
            
            /* start by using the tile edge's as base, and the player 
            edge verts to offset the edge (this is essentially a minkownsky
            sum) */
            vec2_t *base_edge = &edges[0];
            vec2_t *add_vert = &edges[2];
            float smallest_edge_t = 1.0;
            vec2_t smallest_edge_normal;
            
            for(uint32_t edge_index = 0; edge_index < 2; edge_index++)
            {
                for(uint32_t vert_index = 0; vert_index < 2; vert_index++)
                {
                    /* offset the edges */
                    vec2_t_add(&sum_edge[0], &base_edge[0], &add_vert[vert_index]);
                    vec2_t_add(&sum_edge[1], &base_edge[1], &add_vert[vert_index]);
                    /* translate them */
                    vec2_t_add(&sum_edge[0], &sum_edge[0], &positions[edge_index]);
                    vec2_t_add(&sum_edge[1], &sum_edge[1], &positions[edge_index]);
                    
                    vec2_t edge_vec;
                    vec2_t_sub(&edge_vec, &sum_edge[1], &sum_edge[0]);
                    float edge_len = sqrt(vec2_t_dot(&edge_vec, &edge_vec));
                    
                    
                    /* this collision detection is player-centric. To be more generic
                    we'd need to consider both velocities here */
                    vec2_t move_pos = player->position;
                    vec2_t move_edge_vec;
                    /* vec from the movement's first end point (player origin) 
                    to the first vertex of the edge */
                    vec2_t_sub(&move_edge_vec, &sum_edge[0], &move_pos);            
                    float d0 = vec2_t_dot(&move_edge_vec, &normals[edge_index]);
                    vec2_t_add(&move_pos, &move_pos, &player->velocity);
                    /* vec from the movement's second end point to the first 
                    vertex of the edge */
                    vec2_t_sub(&move_edge_vec, &sum_edge[0], &move_pos);
                    float d1 = vec2_t_dot(&move_edge_vec, &normals[edge_index]);
                    
                    if(d0 * d1 <= 0.0)
                    {
                        /* movement end points are on opposite sides of the edge,
                        so it intersects the support line of the edge */
                        float denum = (d0 - d1);
                        
                        if(denum)
                        {
                            float t = d0 / denum;
//                            printf("%f\n", t);
                            
                            if(t >= 0.0 && t < 1.0 && t < smallest_edge_t)
                            {
                                /* the movement ray intersects the support line of the
                                edge, so test to see if the final movement point actually
                                lies inside the edge */
                                    
                                vec2_t position;
                                vec2_t_fmadd(&position, &player->position, &player->velocity, t);
                                
                                vec2_t pos_edge_vec;
                                vec2_t_sub(&pos_edge_vec, &position, &sum_edge[0]);
                                
                                float s = vec2_t_dot(&pos_edge_vec, &edge_vec) / edge_len;
                                
                                if(s >= 0.0 && s <= edge_len)
                                {
                                    /* final movement point is inside the edge */
                                    smallest_edge_t = t;
                                    smallest_edge_normal = normals[edge_index];
                                }
                            }
                        }
                    }
                }
                
                /* swap the edges around. Now, the base edge is the player's, and 
                the verts of the tile are used as offset */
                base_edge = &edges[2];
                add_vert = &edges[0];
            }
            
            if(smallest_edge_t < smallest_t)
            {
                smallest_t = smallest_edge_t;
                smallest_normal = smallest_edge_normal;
            }
        }
        
        if(smallest_t == 1.0)
        {
            return 1;
        }
        
        vec2_t_fmadd(&player->position, &player->position, &player->velocity, smallest_t);   
        vec2_t normalized_velocity;
        vec2_t_normalize(&normalized_velocity, &player->velocity);
        /* small nudge back */
//        vec2_t_fmadd(&player->position, &player->position, &normalized_velocity, -0.05);
        
        /* we hit an edge, so we gotta clip the velocity alongside it */            
        float normal_velocity = vec2_t_dot(&player->velocity, &smallest_normal);
        if(normal_velocity < 0.0)
        {
            vec2_t_fmadd(&player->velocity, &player->velocity, &smallest_normal, -normal_velocity);
        }
        
        return 1;
    }
    
    return 1;
}

void g_BoxEdge(struct g_box_t *box, vec2_t *direction, vec2_t *v0, vec2_t *v1, vec2_t *normal)
{
    vec2_t local_direction;
    vec2_t size;
    
    /* transform the direction into the local box space */
    local_direction.x = (direction->x * box->rot.x + direction->y * box->rot.y) * (1.0 / box->size.x);
    local_direction.y = (-direction->x * box->rot.y + direction->y * box->rot.x) * (1.0 / box->size.y);
    
    size.x = box->size.x * 0.5;
    size.y = box->size.y * 0.5;
    
    if(fabsf(local_direction.x) > fabsf(local_direction.y))
    {
        /* mostly horizontal direction, which means we'll be selecting
        the left or right side of the box */
        
        if(local_direction.x < 0.0)
        {
            /* left side */
            *v0 = (vec2_t){-size.x, size.y};
            *v1 = (vec2_t){-size.x,-size.y};
            *normal = (vec2_t){-1.0, 0.0};
//            printf("left\n");
        }
        else
        {
            /* right side */
            *v0 = (vec2_t){ size.x,-size.y};
            *v1 = (vec2_t){ size.x, size.y};
            *normal = (vec2_t){ 1.0, 0.0};
//            printf("right\n");
        }
    }
    else
    {
        /* the direction is mostly vertical, or is perfectly diagonal. This means
        we'll probably get some biasing towards the up/bottom sides. */
        
        if(local_direction.y > 0.0)
        {
            /* top side */
            *v0 = (vec2_t){ size.x, size.y};
            *v1 = (vec2_t){-size.x, size.y};
            *normal = (vec2_t){ 0.0, 1.0};
//            printf("top\n");
        }
        else
        {
            /* bottom side */
            *v0 = (vec2_t){-size.x,-size.y};
            *v1 = (vec2_t){ size.x,-size.y};
            *normal = (vec2_t){ 0.0,-1.0};
//            printf("bottom\n");
        }
    }
    /* transform the local vertices back to world space */
    *v0 = (vec2_t){v0->x * box->rot.x - v0->y * box->rot.y, v0->x * box->rot.y + v0->y * box->rot.x};
    *v1 = (vec2_t){v1->x * box->rot.x - v1->y * box->rot.y, v1->x * box->rot.y + v1->y * box->rot.x};
    *normal = (vec2_t){normal->x * box->rot.x - normal->y * box->rot.y, normal->x * box->rot.y + normal->y * box->rot.x};   
}

void g_BeginDraw()
{
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
}

void g_EndDraw()
{
    SDL_RenderPresent(g_renderer);
}

void g_DrawPlayers()
{
    struct g_player_t *player = g_players;
//    vec2_t player_camera_pos;
    SDL_Rect rect;
    while(player)
    {
        if(player == g_main_player)
        {
            SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
        }
        else
        {
            SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);
        }
        
        
        vec2_t player_size = G_PLAYER_SIZE;
        vec2_t player_camera_pos;
        vec2_t_sub(&player_camera_pos, &player->position, &g_camera_pos);
        rect.w = player_size.x;
        rect.h = player_size.y;
        rect.x = player_camera_pos.x + (float)g_window_width / 2.0 - player_size.x * 0.5;
        rect.y = ((float)g_window_height / 2.0) - player_camera_pos.y - player_size.y * 0.5;
        SDL_RenderFillRect(g_renderer, &rect);
        player = player->next;
    }
}

void g_DrawTiles()
{
    struct g_tile_t *tile = g_tiles;
    SDL_Rect rect;
    SDL_Point point;
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    while(tile)
    {
        vec2_t tile_camera_pos;
        vec2_t_sub(&tile_camera_pos, &tile->position, &g_camera_pos);
        
        rect.w = tile->size.x;
        rect.h = tile->size.y;
        rect.x = tile_camera_pos.x + (float)g_window_width / 2.0 - tile->size.x * 0.5;
        rect.y = ((float)g_window_height / 2.0) - tile_camera_pos.y - tile->size.y * 0.5;
        
        point.x = tile->size.x * 0.5;
        point.y = tile->size.y * 0.5;
        SDL_RenderCopyEx(g_renderer, g_default_texture, NULL, &rect, 180 * -modf(tile->orientation, &(double){0.0}), &point, 0);
//        SDL_RenderFillRect(g_renderer, &rect);
        tile = tile->next;
    }
}

//void g_DrawDbvhRecursive(struct g_dbvh_node_t *node)
//{
//    if(node)
//    {
//        SDL_Rect rect;
//        rect.x = node->min.x - g_camera_pos.x + g_window_width / 2;
//        rect.y = node->min.y - g_camera_pos.y + g_window_height / 2;
//        rect.w = node->max.x - node->min.x;
//        rect.h = node->max.y - node->min.y;
//        
//        SDL_RenderDrawRect(g_renderer, &rect);
//        g_DrawDbvhRecursive(node->left);
//        g_DrawDbvhRecursive(node->right);
//    }
//}

void g_DrawDbvh()
{
    SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
//    g_DrawDbvhRecursive(g_dbvh);
}



