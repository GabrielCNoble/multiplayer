#include "math.h"

void vec2_t_add(vec2_t *r, vec2_t *a, vec2_t *b)
{
    *r = (vec2_t){a->x + b->x, a->y + b->y};
}

void vec2_t_sub(vec2_t *r, vec2_t *a, vec2_t *b)
{
    *r = (vec2_t){a->x - b->x, a->y - b->y};
}

void vec2_t_mul(vec2_t *r, vec2_t *a, float s)
{
    *r = (vec2_t){a->x *s, a->y *s};
}

float vec2_t_dot(vec2_t *a, vec2_t *b)
{
    return a->x * b->x + a->y * b->y;
}
