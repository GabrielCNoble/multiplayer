#include "sv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TCPsocket sv_connect_socket;
TCPsocket sv_sync_join_socket;
UDPsocket sv_frame_socket;

struct sv_client_t *sv_clients = NULL;
struct sv_client_t *sv_last_client = NULL;
uint32_t sv_client_count = 0;
uint64_t sv_frame = 0;


struct sv_ack_t *sv_connect_ack;
struct sv_sync_frames_t *sv_sync_frames = NULL;
struct sv_sync_join_t *sv_sync_join = NULL;

void sv_Init()
{
    sv_connect_ack = calloc(1, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));
    
    IPaddress server_address;
    SDLNet_ResolveHost(&server_address, NULL, SV_CONNECT_PORT);
    sv_connect_socket = SDLNet_TCP_Open(&server_address);
    sv_frame_socket = SDLNet_UDP_Open(SV_FRAME_PORT);
}

void sv_RunServer(uint32_t local)
{
    TCPsocket connect_socket;
    struct sv_connect_t connect_message;
//    struct sv_frame_t frame;
    struct sv_client_t *client;
    struct sv_client_t *new_clients;
//    IPaddress *peer_address;
    UDPpacket packet;
//    struct sv_frame_t *frames = NULL;
//    struct sv_player_frame_t player_frame;
    uint32_t frame_count = 0;
    uint32_t new_client_count = 0;
    uint32_t sync_count = 0;
    struct sv_sync_join_t sync_join;
    
    while(1)
    {
        SDL_Delay(40);
        new_clients = NULL;
        new_client_count = 0;
        while((connect_socket = SDLNet_TCP_Accept(sv_connect_socket)))
        {
            SDLNet_TCP_Recv(connect_socket, &connect_message, sizeof(struct sv_connect_t));
            client = calloc(1, sizeof(struct sv_client_t));
            client->name = strdup(connect_message.name);
            client->last_update = sv_frame - 1;
            client->connect_socket = connect_socket;
            
            if(!sv_clients)
            {
                sv_clients = client;
            }
            else
            {
                sv_last_client->next = client;
                client->prev = sv_last_client;
            }
            
            sv_last_client = client;
            
            if(!new_clients)
            {
                new_clients = client;
            }
            
             

            sv_connect_ack->status = SV_ACK_STATUS_OK;  
            memcpy(&sv_connect_ack->data, &client, sizeof(struct sv_client_t *));
            
            SDLNet_TCP_Send(connect_socket, sv_connect_ack, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));       
            printf("client %p (%s) connected\n", client, client->name);
            new_client_count++;
        }
        
        sv_client_count += new_client_count;
        
        if(frame_count < sv_client_count)
        {
            new_client_count = sv_client_count - frame_count;
            sv_sync_frames = realloc(sv_sync_frames, sizeof(struct sv_sync_frames_t) + sizeof(struct sv_player_frame_t) * sv_client_count);
            frame_count = sv_client_count;
        }
        
        if(sv_client_count)
        {
            if(new_clients)
            {
                if(sync_count < new_client_count)
                {
                    sv_sync_join = realloc(sv_sync_join, sizeof(struct sv_sync_join_t) + sizeof(struct sv_sync_join_data_t) * new_client_count);
                    sync_count = new_client_count;
                }
                
                uint32_t syncing_new_clients = 0;
                struct sv_client_t *client = sv_clients;
                
                while(client)
                {                    
                    sv_sync_join->player_count = 0;
                    syncing_new_clients |= client == new_clients;
                    
                    struct sv_client_t *sync_client;
                    
                    if(syncing_new_clients)
                    {
                        /* the client we're about to send the data to is a new
                        client, so we need to send all existing clients to it */
                        sync_client = sv_clients;
                    }
                    else
                    {
                        /* the client we're about to send the data to already
                        exists, so we only need send the new clients to it */
                        sync_client = new_clients;
                    }
                                        
                    while(sync_client)
                    {
                        if(client != sync_client)
                        {
                            struct sv_sync_join_data_t *join_data = sv_sync_join->join_data + sv_sync_join->player_count;
                            join_data->client = sync_client;
                            strcpy(join_data->name, sync_client->name);
                            sv_sync_join->player_count++;
                        }
                        
                        sync_client = sync_client->next;
                    }
                    
                    if(sv_sync_join->player_count)
                    {
                        SDLNet_TCP_Send(client->connect_socket, sv_sync_join, sizeof(struct sv_sync_join_t) + sizeof(struct sv_sync_join_data_t) * sv_sync_join->player_count);
                    }
                    client = client->next;
                }
            }
            
            struct sv_player_frame_t player_frame;            
            sv_sync_frames->frame_count = 0;
            packet.data = (uint8_t *)&player_frame;
            packet.maxlen = sizeof(struct sv_player_frame_t);
            
            while(SDLNet_UDP_Recv(sv_frame_socket, &packet))
            {
                struct sv_player_frame_t *frame = (struct sv_player_frame_t *)packet.data;
                frame->client->address = packet.address;
                frame->client->last_update = sv_frame;
                sv_sync_frames->frames[sv_sync_frames->frame_count].frame = frame->frame;
                sv_sync_frames->frames[sv_sync_frames->frame_count].client = frame->client;
                sv_sync_frames->frame_count++;
            }
        }
        
        struct sv_client_t *client = sv_clients;
        struct sv_client_t *prev_client = NULL;
        
        while(client)
        {
            if(sv_frame - client->last_update > 50000)
            {
                struct sv_client_t *next_client = NULL;
                next_client = client->next;
                
                if(client->next)
                {
                    client->next->prev = client->prev;
                }
                else
                {
                    sv_last_client = sv_last_client->prev;
                }
                
                if(client->prev)
                {
                    client->prev->next = client->next;
                }
                else
                {
                    sv_clients = sv_clients->next;
                }
                
                
                SDLNet_TCP_Close(client->connect_socket);
                printf("client %p (%s) disconnected due to inactivity\n", client, client->name);
                free(client->name);
                free(client);                
                sv_client_count--;
                client = next_client;
                continue;
            }
            
            if(client->address.host)
            {
                packet.address = client->address;
                packet.data = (uint8_t *)sv_sync_frames;
                packet.maxlen = sizeof(struct sv_sync_frames_t) + sizeof(struct sv_player_frame_t) * sv_sync_frames->frame_count;
                packet.len = packet.maxlen;
                packet.channel = -1;
                SDLNet_UDP_Send(sv_frame_socket, -1, &packet);
            }
            
            prev_client = client;
            client = client->next;
        }
        
        sv_frame++;
    }
}

