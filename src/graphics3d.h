#ifndef __GRAPHICS3D_H__
#define __GRAPHICS3D_H__
#define PI 3.1415926
typedef struct { int16_t x; int16_t y;} vec2;
typedef struct { float x; float y; float z;} vec3f;
typedef struct { float x; float y;} vec2f;
typedef struct {uint8_t r; uint8_t g; uint8_t b;} colourtype;

extern uint64_t starttime;
extern uint64_t beztime;
extern uint64_t quadtime;

static inline vec3f sub3d(const vec3f p0, const vec3f p1) {
    return (vec3f){p0.x-p1.x, p0.y-p1.y, p0.z-p1.z};
}

static inline vec3f add3d(const vec3f p0, const vec3f p1) {
    return (vec3f){p0.x+p1.x, p0.y+p1.y, p0.z+p1.z};
}

static inline vec3f mul3d(const vec3f p0,const vec3f p1) {
    return (vec3f){p0.x*p1.x,p0.y*p1.y,p0.z*p1.z};
}

static inline vec3f mul3df(const float f,const vec3f p1) {
    return (vec3f){f*p1.x,f*p1.y,f*p1.z};
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

// square of the length of a vector
static inline float mag3d(vec3f p) {
    return (p.x*p.x+p.y*p.y+p.z*p.z);
}

static inline float dot2d(vec2f p0, vec2f p1) {
    return (p0.x*p1.x+p0.y*p1.y);
}

static inline vec3f mid3d(vec3f const p0, vec3f const p1) {
    return (vec3f){(p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f, (p0.z+p1.z)*0.5f};
}

static inline vec3f cross3d(vec3f p0, vec3f p1) {
    return (vec3f) {(p0.y)*(p1.z)-(p0.z)*(p1.y),
                    (p0.z)*(p1.x)-(p0.x)*(p1.z),
                    (p0.x)*(p1.y)-(p0.y)*(p1.x)};
}
// approximate reciprocal because fp division is very slow
static inline float recip(float number) {
    union {float f; uint32_t i; } conv  = { .f = number };
    conv.i  = 0x7EF477D5 - conv.i;
    conv.f *= 2.0f-number*conv.f;
 //   conv.f *= 2.0f-number*conv.f;
    float h=1.0f-number*conv.f;
    conv.f *= (1.0f+h*(1.0f+h));
    return conv.f;
}
// fast inverse square root 
static inline float Q_rsqrt( float number )
{	
	const float x2 = number * 0.5F;
	const float threehalfs = 1.5F;
	union {float f; uint32_t i; } conv  = { .f = number };
	conv.i  = 0x5f3759df - ( conv.i >> 1 );
	conv.f  *= threehalfs - ( x2 * conv.f * conv.f );
	return conv.f;
}

static inline vec3f normalise(const vec3f p) {
    float mag=Q_rsqrt(p.x*p.x+p.y*p.y+p.z*p.z);
    return (vec3f){(p.x)*mag, (p.y)*mag, (p.z)*mag};
}

static inline vec2f normalise2d(const vec2f p) {
    float d2=p.x*p.x+p.y*p.y;
   // if(d2<0.01f)
   //     return p;
    float mag=Q_rsqrt(d2);
    return (vec2f){(p.x)*mag, (p.y)*mag};
}

static inline float dot(const vec3f p0,const vec3f p1) {
    return ((p0.x*p1.x))+((p0.y*p1.y))+((p0.z*p1.z));
}

static inline float maxf(float a, float b) {return a>b?a:b;}
static inline float minf(float a, float b) {return a<b?a:b;}
static inline float dist(vec3f a, vec3f b) {return (mag3d(sub3d(a,b)));}

static inline int clamp(int x,int min,int max) {
    const int t = x < min ? min : x;
    return t > max ? max : t;
}

static inline float clampf(float x,float min,float max) {
    const float t = x < min ? min : x;
    return t > max ? max : t;
}
void draw_teapot(vec2f pos, float size, vec3f rot, colourtype col, int multicolour, uint64_t *time1, uint64_t *time2);
void draw_cube(vec2f pos, float size, vec3f rot);
#endif