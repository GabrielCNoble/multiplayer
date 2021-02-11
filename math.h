#ifndef MATH_H
#define MATH_H


typedef struct
{
    float x;
    float y;
}vec2_t;

void vec2_t_add(vec2_t *r, vec2_t *a, vec2_t *b);

void vec2_t_sub(vec2_t *r, vec2_t *a, vec2_t *b);

void vec2_t_mul(vec2_t *r, vec2_t *a, float s);

void vec2_t_fmadd(vec2_t *r, vec2_t *add_a, vec2_t *add_b, float s);

void vec2_t_normalize(vec2_t *r, vec2_t *v);

float vec2_t_dot(vec2_t *a, vec2_t *b);


#endif // MATH_H
