#ifndef SV_H
#define SV_H

#include <stdint.h>
#include "math.h"
#include "g.h"
#include "SDL2/SDL_net.h"


#define SV_CONNECT_PORT 1313
#define SV_FRAME_PORT 6969
#define SV_MAX_CLIENTS 64
#define SV_MAX_MESSAGE_LEN 8192
#define SV_PLAYER_NAME_MAX_LEN 64



struct sv_frame_t
{
    vec2_t position;
    vec2_t velocity;
};

struct sv_client_t
{
    struct sv_client_t *next;
    struct sv_client_t *prev;
    IPaddress address;
    TCPsocket connect_socket;
    char *name;
    uint64_t last_update;
    uint32_t join_list_index;
};

enum SV_MESSAGE_TYPE
{
    SV_MESSAGE_TYPE_ACK = 0,
    SV_MESSAGE_TYPE_CONNECT,
    SV_MESSAGE_TYPE_DISCONNECT,
    SV_MESSAGE_TYPE_PLAYER_FRAME,
    SV_MESSAGE_TYPE_SYNC_FRAMES,
    SV_MESSAGE_TYPE_LAST
};

struct sv_message_t
{
    uint32_t type;
};

enum SV_ACK_STATUS
{
    SV_ACK_STATUS_OK = 0,
    SV_ACK_STATUS_ERROR
};

struct sv_ack_t
{
    struct sv_message_t base;
    uint32_t status;
    unsigned char data[];
};

struct sv_connect_t
{
    struct sv_message_t base;
    char name[SV_PLAYER_NAME_MAX_LEN];
    uint32_t frame_sync_port;
    IPaddress join_sync_address;
    struct sv_client_t *client; 
};

struct sv_disconnect_t
{
    struct sv_message_t base;
    struct sv_client_t *client;
};

struct sv_player_frame_t
{
    struct sv_message_t base;
    struct sv_client_t *client;
    struct sv_frame_t frame;
};

struct sv_sync_frames_t
{
    struct sv_message_t base;
    uint32_t frame_count;
    struct sv_player_frame_t frames[];
};

#define SV_SYNC_FRAMES_SIZE(client_count) (sizeof(struct sv_sync_frames_t)+sizeof(struct sv_player_frame_t)*client_count)

struct sv_sync_join_data_t
{
    struct sv_message_t base;
    char name[SV_PLAYER_NAME_MAX_LEN];
    struct sv_client_t *client;
};

struct sv_sync_join_t
{
    uint32_t player_count;
    struct sv_sync_join_data_t join_data[];
};





struct sv_sync_client_join_item_t
{
    char name[SV_PLAYER_NAME_MAX_LEN];
    struct sv_client_t *client;
};

struct sv_sync_client_join_list_t
{
    uint32_t client_count;
    struct sv_sync_client_join_item_t clients[];
};

struct sv_sync_client_drop_item_t
{
    struct sv_client_t *client;
};

struct sv_sync_client_drop_list_t
{
    uint32_t client_count;
    struct sv_sync_client_drop_item_t clients[];
};

enum SV_SYNC_CLIENT_TYPE
{
    SV_SYNC_CLIENT_TYPE_JOIN = 0,
    SV_SYNC_CLIENT_TYPE_DROP
};

struct sv_sync_client_t
{
    struct sv_message_t base;
    uint32_t type;
    char data[];
};

#define SV_SYNC_CLIENT_JOIN_SIZE(client_count) (sizeof(struct sv_sync_client_t)+sizeof(struct sv_sync_client_join_list_t)+sizeof(struct sv_sync_client_join_item_t)*client_count)
#define SV_SYNC_CLIENT_DROP_SIZE(client_count) (sizeof(struct sv_sync_client_t)+sizeof(struct sv_sync_client_drop_list_t)+sizeof(struct sv_sync_client_drop_item_t)*client_count)

#define SV_SYNC_CLIENT_MAX_SIZE (SV_SYNC_CLIENT_JOIN_SIZE(SV_MAX_CLIENTS)>SV_SYNC_CLIENT_DROP_SIZE(SV_MAX_CLIENTS)?SV_SYNC_CLIENT_JOIN_SIZE(SV_MAX_CLIENTS):SV_SYNC_CLIENT_DROP_SIZE(SV_MAX_CLIENTS))


void sv_Init();

void sv_RunServer(uint32_t local);

#endif // SV_H
