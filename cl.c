#include <stdio.h>
#include <string.h>
#include "cl.h"
#include "sv.h"
#include "g.h"
#include "t.h"
#include "SDL2/SDL_net.h"


UDPsocket cl_player_socket;
TCPsocket cl_connect_socket;
SDLNet_SocketSet cl_socket_set;
IPaddress cl_server_address;
struct sv_client_t *cl_client;
uint32_t sv_got_response = 1;

struct sv_sync_frames_t *cl_sync_frames;
struct sv_ack_t *cl_connect_status;
struct sv_sync_client_t *cl_sync_client;
IPaddress cl_sync_join_address;

extern struct g_player_t *g_players;
extern struct g_player_t *g_main_player;

void cl_Init()
{
    cl_socket_set = SDLNet_AllocSocketSet(1);
    
    cl_sync_frames = calloc(1, sizeof(struct sv_sync_frames_t) + sizeof(struct sv_frame_t) * SV_MAX_CLIENTS);
    cl_connect_status = calloc(1, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));
    cl_sync_client = calloc(1, SV_SYNC_CLIENT_MAX_SIZE);
    g_Init();
}

void cl_RunClient()
{    
    t_DeltaTime();
    while(1)
    {
        float delta_time = t_DeltaTime();
        cl_SyncClients();
        cl_SyncFrames();
        g_BeginDraw();
        g_PlayerInput(0.0166);
        cl_SendPlayerFrame(g_main_player);
        g_UpdatePlayers(0.0166);
        g_UpdateCamera(0.0166);        
        g_DrawPlayers();
        g_DrawTiles();
        g_EndDraw();
    }
}

uint32_t cl_Connect(char *player_name, char *server_address)
{
    struct sv_connect_t connect_message;
    strcpy(connect_message.name, player_name);
    
    if(!server_address)
    {
        server_address = "localhost";
    }
    
    SDLNet_ResolveHost(&cl_server_address, server_address, SV_CONNECT_PORT);
    cl_connect_socket = SDLNet_TCP_Open(&cl_server_address);    
    SDLNet_TCP_Send(cl_connect_socket, &connect_message, sizeof(struct sv_connect_t));
    SDLNet_TCP_Recv(cl_connect_socket, cl_connect_status, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));
    
    if(cl_connect_status->status == SV_ACK_STATUS_OK)
    {
        memcpy(&cl_client, (struct sv_client_t **)&cl_connect_status->data, sizeof(struct sv_client_t *));
        SDLNet_ResolveHost(&cl_server_address, server_address, SV_FRAME_PORT);
        printf("cl_Connect: connected to server. Client address is %p\n", cl_client);
        cl_player_socket = SDLNet_UDP_Open(0);
        SDLNet_TCP_AddSocket(cl_socket_set, cl_connect_socket);        
        g_main_player->client = cl_client;
        return 1;
    }
    
    return 0;
}

void cl_Disconnect()
{
    
}

void cl_SyncClients()
{    
    struct g_player_t *joined_player;
    if(SDLNet_CheckSockets(cl_socket_set, 0) > 0)
    {
        SDLNet_TCP_Recv(cl_connect_socket, cl_sync_client, SV_SYNC_CLIENT_MAX_SIZE);
        switch(cl_sync_client->type)
        {
            case SV_SYNC_CLIENT_TYPE_DROP:
            {
                struct sv_sync_client_drop_list_t *drop_list = (struct sv_sync_client_drop_list_t *)&cl_sync_client->data;
                for(uint32_t drop_index = 0; drop_index < drop_list->client_count; drop_index++)
                {
                    struct g_player_t *player = cl_FindPlayerByClient(drop_list->clients[drop_index].client);
                    g_DestroyPlayer(player);
                }
            }
            break;
            
            case SV_SYNC_CLIENT_TYPE_JOIN:
            {
                struct sv_sync_client_join_list_t *join_list = (struct sv_sync_client_join_list_t *)&cl_sync_client->data;
                for(uint32_t join_index = 0; join_index < join_list->client_count; join_index++)
                {
                    joined_player = g_CreatePlayer(G_PLAYER_TYPE_REMOTE, &(vec2_t){50.0, 50.0});
                    joined_player->client = join_list->clients[join_index].client;
                }
            }
            break;            
        }
    }
}

void cl_SyncFrames()
{
    UDPpacket packet;
    
    packet.channel = -1;
    packet.data = (uint8_t *)cl_sync_frames;
    packet.maxlen = sizeof(struct sv_sync_frames_t) + sizeof(struct sv_frame_t) * SV_MAX_CLIENTS;
    
    while(SDLNet_UDP_Recv(cl_player_socket, &packet))
    {
        sv_got_response = 1;
    }
}

void cl_SendPlayerFrame(struct g_player_t *player)
{
    struct sv_player_frame_t frame;
    UDPpacket packet;
    
//    if(sv_got_response)
    {
        frame.client = cl_client;
        frame.frame.position = player->position;
        frame.frame.velocity = player->velocity;
        
        packet.address = cl_server_address;
        packet.data = (uint8_t*)&frame;
        packet.len = sizeof(struct sv_player_frame_t);
        packet.maxlen = packet.len;
        packet.channel = -1;
        
        SDLNet_UDP_Send(cl_player_socket, -1, &packet);
        sv_got_response = 0;
    }
}

struct g_player_t *cl_FindPlayerByClient(struct sv_client_t *client)
{
    struct g_player_t *player = g_players;
    
    while(player)
    {
        if(player->client == client)
        {
            break;
        }
        player = player->next;
    }
    
    return player;
}










