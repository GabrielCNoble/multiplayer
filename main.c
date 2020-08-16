#include <stdio.h>
#include <string.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_net.h"
#undef main

#include "sv.h"
#include "cl.h"

int main(int argc, char *argv[])
{
    char client_name[256];
    char server_address_str[256];
    char *server_address = NULL;
    uint32_t run_cli = 1;
    uint32_t server = 0;
    uint32_t local = 0;
    
    if(argc > 1)
    {
        for(uint32_t arg_index = 1; arg_index < argc; arg_index++)
        {
            if(!strcmp("-server", argv[arg_index]))
            {
                server = 1;
            }
            else if(!strcmp("-local", argv[arg_index]))
            {
                local = 1;
            }
        }
    }
    
    
    if(!server)
    {
        if(SDL_Init(SDL_INIT_EVERYTHING) < 0)
        {
            printf("error initializing SDL: %s\n", SDL_GetError());
            return -1;
        }
    }
    
    if(SDLNet_Init() < 0)
    {
        printf("error initializing SDL_net: %s\n", SDLNet_GetError());
        return -2;
    }
    
    if(server)
    {
        sv_Init();
        sv_RunServer(local);
    }
    else
    {
        if(!local)
        {
            while(1)
            {
                printf("what's the server's address?\n>>> ");
                fgets(server_address_str, sizeof(server_address_str), stdin);
                uint32_t index = 0;
                while(server_address_str[index] != '\n') index++;
                server_address_str[index] = '\0';
                server_address = server_address_str;
                break;
            }
        }
        
        while(1)
        {
            printf("what's your nickname?\n>>> ");
            fgets(client_name, sizeof(client_name), stdin);
            uint32_t index = 0;
            while(client_name[index] != '\n')index++;
            client_name[index] = '\0';
            if(strlen(client_name) == 0)
            {
                printf("please give me a valid name to work with...\n");
            }
            else
            {
                uint8_t answer = 0;
                printf("%s: is that correct? (y/n)\n>>> ", client_name);
                scanf("%c", &answer);
                if(answer == 'y' || answer == 'Y')
                {
                    printf("%s it is. Connecting...\n", client_name);
                    break;
                }
                while(answer != '\n')
                {
                    scanf("%c", &answer);
                }
            }
        }
        
        cl_Init();        
        if(!cl_Connect(client_name, server_address))
        {
            printf("couldn't connect to server!\n");
            return -1;
        }
        cl_RunClient();   
    }
}





