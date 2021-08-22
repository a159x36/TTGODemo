#ifndef __GRAPHICS3D_H__
#define __GRAPHICS3D_H__
#define PI 3.1415926
typedef struct { int x; int y; int z;} vec3;
typedef struct { int16_t x; int16_t y;} vec2;
typedef struct { float x; float y; float z;} vec3f;
typedef struct { float x; float y;} vec2f;
typedef struct {uint16_t r; uint16_t g; uint16_t b;} colourtype;


static inline vec3f sub3d(vec3f p0, vec3f p1) {
    return (vec3f){p0.x-p1.x, p0.y-p1.y, p0.z-p1.z};
}

static inline vec3f add3d(vec3f p0, vec3f p1) {
    return (vec3f){p0.x+p1.x, p0.y+p1.y, p0.z+p1.z};
}


static inline vec2f sub2d(vec2f p0, vec2f p1) {
    return (vec2f){p0.x-p1.x, p0.y-p1.y};
}

static inline vec2f add2d(vec2f p0, vec2f p1) {
    return (vec2f){p0.x+p1.x, p0.y+p1.y};
}

static inline vec2f mul2d(float f, vec2f p) {
    return (vec2f){f*p.x, f*p.y};
}

static inline float mag2d(vec2f p) {
    return (p.x*p.x+p.y*p.y);
}


static inline vec3f mid3d(vec3f const p0, vec3f const p1) {
    return (vec3f){(p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f, (p0.z+p1.z)*0.5f};
}

static inline vec3f cross3d(vec3f p0, vec3f p1) {
    return (vec3f) {(p0.y)*(p1.z)-(p0.z)*(p1.y),
                    (p0.z)*(p1.x)-(p0.x)*(p1.z),
                    (p0.x)*(p1.y)-(p0.y)*(p1.x)};
}
static inline float recip(float number) {
    union {float f; uint32_t i; } conv  = { .f = number };
    conv.i  = 0xbe6eb3be - ( conv.i >> 1 );
    return conv.f*conv.f;
}
static inline float Q_rsqrt( float number )
{	
	const float x2 = number * 0.5F;
	const float threehalfs = 1.5F;
	union {float f; uint32_t i; } conv  = { .f = number };
	conv.i  = 0x5f3759df - ( conv.i >> 1 );
	conv.f  *= threehalfs - ( x2 * conv.f * conv.f );
	return conv.f;
}

static inline vec3f normalise(vec3f p) {
    float mag=Q_rsqrt(p.x*p.x+p.y*p.y+p.z*p.z);
    return (vec3f){(p.x)*mag, (p.y)*mag, (p.z)*mag};
}

static inline vec2f normalise2d(vec2f p) {
    float d2=p.x*p.x+p.y*p.y;
    if(d2==0)
        return p;
    float mag=Q_rsqrt(d2);
    return (vec2f){(p.x)*mag, (p.y)*mag};
}

static inline float dot(vec3f p0,vec3f p1) {
    return ((p0.x*p1.x))+((p0.y*p1.y))+((p0.z*p1.z));
}

void draw_teapot(vec2 pos, float size, vec3f rot, colourtype col);
#endif