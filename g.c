#include "g.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_render.h"
#include <stdlib.h>
#include <string.h>

struct g_player_t *g_players = NULL;
struct g_player_t *g_last_player = NULL;
struct g_player_t *g_main_player = NULL;

SDL_Event g_event;
SDL_Window *g_window;
SDL_Renderer *g_renderer;

extern struct sv_sync_frames_t *cl_sync_frames;

void g_Init()
{
    g_window = SDL_CreateWindow("game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    
    g_CreatePlayer(G_PLAYER_TYPE_CONTROLLED, &(vec2_t){50.0, 50.0});
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
    }
    
    g_last_player = player;
    
    if(type == G_PLAYER_TYPE_CONTROLLED)
    {
        g_main_player = player;
    }
    
    return player;
}

void g_UpdatePlayers()
{
    struct g_player_t *player = g_players;
    struct sv_player_frame_t *player_frame;
    const uint8_t *keyboard_state;
    while(player)
    {
        switch(player->type)
        {
            case G_PLAYER_TYPE_CONTROLLED:
                SDL_PollEvent(&g_event);
                keyboard_state = SDL_GetKeyboardState(NULL);
                
                if(keyboard_state[SDL_SCANCODE_W])
                {
                    player->position.y -= 1.0;
                }
                if(keyboard_state[SDL_SCANCODE_S])
                {
                    player->position.y += 1.0;
                }
                
                if(keyboard_state[SDL_SCANCODE_A])
                {
                    player->position.x -= 1.0;
                }
                if(keyboard_state[SDL_SCANCODE_D])
                {
                    player->position.x += 1.0;
                }
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
        
        rect.w = 20;
        rect.h = 20;
        rect.x = player->position.x;
        rect.y = player->position.y;
        SDL_RenderFillRect(g_renderer, &rect);
        player = player->next;
    }
}



