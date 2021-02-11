#include "sv.h"
#include "t.h"
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
struct sv_sync_client_t *sv_sync_client_join = NULL;
struct sv_sync_client_t *sv_sync_client_drop = NULL;

void sv_Init()
{
    sv_connect_ack = calloc(1, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));
    
    sv_sync_client_join = calloc(1, SV_SYNC_CLIENT_JOIN_SIZE(SV_MAX_CLIENTS));                                    
    sv_sync_client_drop = calloc(1, SV_SYNC_CLIENT_DROP_SIZE(SV_MAX_CLIENTS));
    sv_sync_frames = calloc(1, SV_SYNC_FRAMES_SIZE(SV_MAX_CLIENTS));
    
    sv_sync_client_join->type = SV_SYNC_CLIENT_TYPE_JOIN;
    sv_sync_client_drop->type = SV_SYNC_CLIENT_TYPE_DROP;
    
    IPaddress server_address;
    SDLNet_ResolveHost(&server_address, NULL, SV_CONNECT_PORT);
    sv_connect_socket = SDLNet_TCP_Open(&server_address);
    sv_frame_socket = SDLNet_UDP_Open(SV_FRAME_PORT);
}

void sv_RunServer(uint32_t local)
{
    TCPsocket connect_socket;
    UDPpacket packet;
    
    while(1)
    {
//        SDL_Delay(40);
        float delta_time = 0.0;
        struct sv_client_t *new_clients = NULL;
        struct sv_client_t *last_new_client = NULL;
        uint32_t new_client_count = 0;
        
        struct sv_sync_client_join_list_t *join_list = (struct sv_sync_client_join_list_t *)&sv_sync_client_join->data;
        join_list->client_count = 0;
        
        while((connect_socket = SDLNet_TCP_Accept(sv_connect_socket)))
        {
            /* handle connecting clients... */
            struct sv_connect_t connect_message;
            SDLNet_TCP_Recv(connect_socket, &connect_message, sizeof(struct sv_connect_t));
            struct sv_client_t *client = calloc(1, sizeof(struct sv_client_t));
            client->name = strdup(connect_message.name);
            client->last_update = sv_frame - 1;
            client->connect_socket = connect_socket;
            
            if(!new_clients)
            {
                new_clients = client;
            }
            else
            {
                last_new_client->next = client;
                client->prev = last_new_client;
            }
            
            last_new_client = client;
             
            client->join_list_index = join_list->client_count;
            join_list->clients[join_list->client_count].client = client;
            strcpy(join_list->clients[join_list->client_count].name, client->name);
            join_list->client_count++;
            
            sv_connect_ack->status = SV_ACK_STATUS_OK;  
            memcpy(&sv_connect_ack->data, &client, sizeof(struct sv_client_t *));
            
            SDLNet_TCP_Send(connect_socket, sv_connect_ack, sizeof(struct sv_ack_t) + sizeof(struct sv_client_t *));       
            printf("client %p (%s) connected\n", client, client->name);
            new_client_count++;
        }
        
        if(sv_client_count)
        {
            struct sv_player_frame_t player_frame;            
            sv_sync_frames->frame_count = 0;
            packet.data = (uint8_t *)&player_frame;
            packet.maxlen = sizeof(struct sv_player_frame_t);
            
            while((delta_time += t_DeltaTime()) < 0.008)
            {
                /* pool frames for 8ms */
                while(SDLNet_UDP_Recv(sv_frame_socket, &packet))
                {
                    /* gather frames from clients already connected before this frame */
                    struct sv_player_frame_t *frame = (struct sv_player_frame_t *)packet.data;
                    frame->client->address = packet.address;
                    frame->client->last_update = sv_frame;
                    uint32_t frame_index;
                    
                    if(frame->client->frame_list_index == 0xffffffff)
                    {
                        frame->client->frame_list_index = sv_sync_frames->frame_count;
                        sv_sync_frames->frame_count++;
                    }
                    
                    frame_index = frame->client->frame_list_index;
                    
                    sv_sync_frames->frames[frame_index].frame = frame->frame;
                    sv_sync_frames->frames[frame_index].client = frame->client;
                }
            }
        }
        
        sv_client_count += new_client_count;
        
        struct sv_client_t *client = sv_clients;
        struct sv_client_t *drop_clients = NULL;
        struct sv_client_t *last_drop_client = NULL;
        
        struct sv_sync_client_drop_list_t *drop_list = (struct sv_sync_client_drop_list_t *)&sv_sync_client_drop->data;
        drop_list->client_count = 0;
        
        while(client)
        {
            client->frame_list_index = 0xffffffff;
            if(sv_frame - client->last_update > 50)
            {
                /* it's been long since this client last communicated with the server,
                so drop it */
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
                
                if(!drop_clients)
                {
                    drop_clients = client;
                }
                else
                {
                    last_drop_client->next = client;
                    client->prev = last_drop_client;
                }
                last_drop_client = client;
                
                /* add this new client to the join list, so we can sync with the 
                clients that were already connected */
                drop_list->clients[drop_list->client_count].client = client;
                drop_list->client_count++;
                
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
                /* send the frames back, to sync client positions. In the future, movement prediction will
                happen before this */
                packet.address = client->address;
                packet.data = (uint8_t *)sv_sync_frames;
                packet.maxlen = sizeof(struct sv_sync_frames_t) + sizeof(struct sv_player_frame_t) * sv_sync_frames->frame_count;
                packet.len = packet.maxlen;
                packet.channel = -1;
                SDLNet_UDP_Send(sv_frame_socket, -1, &packet);
            }
            
            client = client->next;
        }
        
        if(drop_clients)
        {
            /* we have clients that will be dropped, so tell all the previously connected
            clients that they should drop the client too */
            struct sv_client_t *client = sv_clients;
            while(client)
            {
                SDLNet_TCP_Send(client->connect_socket, sv_sync_client_drop, SV_SYNC_CLIENT_DROP_SIZE(drop_list->client_count));
                client = client->next;
            }
        }
        
        if(new_clients)
        {
            /* we have clients connecting this frame, so we gotta tell the other
            clients about them */            
            uint32_t syncing_new_clients = 0;
            struct sv_client_t *client = sv_clients;
            uint32_t client_count = join_list->client_count;
            
            while(client)
            {
                /* add this client to the join list, so we can tell the new clients about it */
                join_list->clients[client_count].client = client;
                strcpy(join_list->clients[client_count].name, client->name);
                client_count++;
                
                /* sync the clients that were already connected */
                SDLNet_TCP_Send(client->connect_socket, sv_sync_client_join, SV_SYNC_CLIENT_JOIN_SIZE(join_list->client_count));
                client = client->next;
            }
            
            join_list->client_count = client_count;
            
            client = new_clients;
            join_list->client_count--;
            if(join_list->client_count)
            {
                while(client)
                {
                    /* the join list contains first all the new clients, and then the already existing
                    clients. We don't need to tell a client itself is joining, since it already knows
                    that. To avoid reconstructing this list every iteration to remove it what we do
                    instead is move it to the last position of the list, and move the client that is
                    in the last position to its position in the list. This will effectively remove it 
                    from the list (since the client count was decremented). In the next iteration, the 
                    next client will move itself to the last position of the list and move the last client 
                    (which is the previous new client) to its position in the list */
                    struct sv_sync_client_join_item_t join_item = join_list->clients[join_list->client_count];
                    join_list->clients[join_list->client_count] = join_list->clients[client->join_list_index];
                    join_list->clients[client->join_list_index] = join_item;
                    
                    SDLNet_TCP_Send(client->connect_socket, sv_sync_client_join, SV_SYNC_CLIENT_JOIN_SIZE(join_list->client_count));
                    client = client->next;
                }
            }
            
            if(!sv_clients)
            {
                sv_clients = new_clients;
            }
            else
            {
                sv_last_client->next = new_clients;
                new_clients->prev = sv_last_client;
            }
            
            sv_last_client = last_new_client;
        }
        
        while((delta_time += t_DeltaTime()) < 0.016);
        
        sv_frame++;
    }
}

