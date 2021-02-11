#include "math.h"
#include <math.h>

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

void vec2_t_fmadd(vec2_t *r, vec2_t *add_a, vec2_t *add_b, float s)
{
    *r = (vec2_t){fmaf(s, add_b->x, add_a->x), fmaf(s, add_b->y, add_a->y)};
}

void vec2_t_normalize(vec2_t *r, vec2_t *v)
{
    float l = vec2_t_dot(v, v);
    if(l)
    {
        *r = (vec2_t){v->x / l, v->y / l};
    }
}

float vec2_t_dot(vec2_t *a, vec2_t *b)
{
    return a->x * b->x + a->y * b->y;
}
