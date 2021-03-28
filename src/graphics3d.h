#include <stdio.h>
#define PI 3.1415926
typedef struct { int x; int y; int z;} vec3;
typedef struct { int16_t x; int16_t y;} vec2;
typedef struct { float x; float y; float z;} vec3f;
typedef struct {uint16_t r; uint16_t g; uint16_t b;} colourtype;


static inline vec3f sub3d(vec3f p0, vec3f p1) {
    return (vec3f){p0.x-p1.x, p0.y-p1.y, p0.z-p1.z};
}

static inline vec3f add3d(vec3f p0, vec3f p1) {
    return (vec3f){p0.x+p1.x, p0.y+p1.y, p0.z+p1.z};
}

void draw_teapot(vec2 pos, float size, vec3f rot, colourtype col);
