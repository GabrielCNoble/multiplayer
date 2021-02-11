#include "t.h"
#include "SDL2/SDL.h"

uint64_t last_counter = 0;
uint64_t counter_frequency = 0;

void t_Init()
{
    counter_frequency = SDL_GetPerformanceFrequency();
    t_DeltaTime();
}

float t_CounterDiff(uint64_t a, uint64_t b)
{
    return (float)(b - a)/(float)counter_frequency;
}

float t_DeltaTime()
{
    uint64_t cur_counter = SDL_GetPerformanceCounter();
    float delta_time = t_CounterDiff(last_counter, cur_counter);
    last_counter = cur_counter;
    return delta_time;
}




