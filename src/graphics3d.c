#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graphics.h"
#include "graphics3d.h"
#include "teapot_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

vec3f lightpos={0,0,0.7};
float teapot_size=20;
float rmx[3][3];
vec3f rotation={PI/2-0.2,0,0};
int offsetx;
int offsety;
colourtype ambiant={16,16,16};
colourtype diffuse={20,220,40};
typedef  struct {uint8_t p[8]; uint16_t col; int16_t z;} quadtype;
#define MAXQUADS 24*32
int nquads;
quadtype quads[MAXQUADS];

inline int clamp(int x,int min,int max) {
    const int t = x < min ? min : x;
    return t > max ? max : t;
}

inline float clampf(float x,float min,float max) {
    const float t = x < min ? min : x;
    return t > max ? max : t;
}

inline vec2 point3d_to_xy(vec3f p) {
    return (vec2){clamp((int)p.x,0,display_width),
                  clamp((int)p.y,0,display_height)};
}

void draw_line_3d(vec3f p0, vec3f p1, uint16_t colour) {
    vec2 a,b;
    a=point3d_to_xy(p0);
    b=point3d_to_xy(p1);
    draw_line(a.x,a.y,b.x,b.y, colour);
}

void draw_triangle_3d(vec3f p0, vec3f p1, vec3f p2, uint16_t colour) {
    vec2 a,b,c;
    a=point3d_to_xy(p0);
    b=point3d_to_xy(p1);
    c=point3d_to_xy(p2);
    draw_triangle(a.x,a.y,b.x,b.y,c.x,c.y, colour);
}

inline vec3f mid3d(vec3f const p0, vec3f const p1) {
    return (vec3f){(p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f, (p0.z+p1.z)*0.5f};
}

inline vec3f cross3d(vec3f p0, vec3f p1) {
    return (vec3f) {(p0.y)*(p1.z)-(p0.z)*(p1.y),
                    (p0.z)*(p1.x)-(p0.x)*(p1.z),
                    (p0.x)*(p1.y)-(p0.y)*(p1.x)};
}

inline float Q_rsqrt( float number )
{	
	const float x2 = number * 0.5F;
	const float threehalfs = 1.5F;
	union {float f; uint32_t i; } conv  = { .f = number };
	conv.i  = 0x5f3759df - ( conv.i >> 1 );
	conv.f  *= threehalfs - ( x2 * conv.f * conv.f );
	return conv.f;
}

inline vec3f normalise(vec3f p) {
    float mag=Q_rsqrt(p.x*p.x+p.y*p.y+p.z*p.z);
    return (vec3f){(p.x)*mag, (p.y)*mag, (p.z)*mag};
}

inline float dot(vec3f p0,vec3f p1) {
    return ((p0.x*p1.x))+((p0.y*p1.y))+((p0.z*p1.z));
}

inline void maketrotationmatrix() {
    float ca=cos(rotation.x);
    float cb=cos(rotation.y);
    float cc=cos(rotation.z);
    float sa=sin(rotation.x);
    float sb=sin(rotation.y);
    float sc=sin(rotation.z);

    rmx[0][0]=cc*cb;
    rmx[0][1]=cc*sb*sa-sc*ca;
    rmx[0][2]=cc*sb*ca+sc*sa;
    rmx[1][0]=sc*cb;
    rmx[1][1]=sc*sb*sa+cc*ca;
    rmx[1][2]=sc*sb*ca-cc*sa;
    rmx[2][0]=-sb;
    rmx[2][1]=cb*sa;
    rmx[2][2]=cb*ca;
}

portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
void add_quad(vec3f p0, vec3f p1, vec3f p2, vec3f p3) {
    
    vec3f normal=normalise(cross3d(sub3d(p2,p0),sub3d(p3,p0)));
    if(normal.z<=0) return;
    if(nquads>=MAXQUADS) return;
    float light=clampf(dot(normal,lightpos),0,1.0);

    uint16_t colour=rgbToColour(clamp(((int)(diffuse.r*light))+ambiant.r,0,255),
                    clamp((int)(diffuse.g*light)+ambiant.g,0,255),
                    clamp((int)(diffuse.b*light)+ambiant.b,0,255));
    vec2 a,b,c,d;
    a=point3d_to_xy(p0);
    b=point3d_to_xy(p1);
    c=point3d_to_xy(p2);
    d=point3d_to_xy(p3);
//    portENTER_CRITICAL(&mutex);
    quads[nquads++]=(quadtype){{a.x,a.y,b.x,b.y,c.x,c.y,d.x,d.y},
                        colour,(int16_t)((p0.z+p1.z+p2.z+p3.z)*64)};
//    portEXIT_CRITICAL(&mutex);
}

int cmpquad(const void * a, const void * b) {
   return ( ((quadtype *)a)->z - ((quadtype *)b)->z );
}

void sort_quads() {
    qsort(quads, nquads, sizeof(quadtype), cmpquad);    
}

void draw_all_quads() {
    sort_quads();
    for(int i=0;i<nquads;i++) {
        quadtype q=quads[i];
        draw_triangle(q.p[0],q.p[1],q.p[2],q.p[3],q.p[4],q.p[5],q.col);
        draw_triangle(q.p[4],q.p[5],q.p[6],q.p[7],q.p[0],q.p[1],q.col);
    }
}
/*
void draw_quad_3d(vec3f p0, vec3f p1, vec3f p2, vec3f p3) {

    vec3f normal=normalise(cross3d(sub3d(p2,p0),sub3d(p3,p0)));
    if(normal.z<=0) return;

    float light=clampf(dot(normal,lightpos),0,1.0);

    uint16_t colour=rgbToColour(clamp(((int)(diffuse.r*light))+ambiant.r,0,255),
    clamp((int)(diffuse.g*light)+ambiant.g,0,255),
    clamp((int)(diffuse.b*light)+ambiant.b,0,255));
    vec2 a,b,c,d;
    a=point3d_to_xy(p0);
    b=point3d_to_xy(p1);
    c=point3d_to_xy(p2);
    d=point3d_to_xy(p3);
    draw_triangle(a.x,a.y,b.x,b.y,c.x,c.y, colour);
    draw_triangle(c.x,c.y,d.x,d.y,a.x,a.y, colour);
}
*/
inline vec3f vrotate(vec3f v, float f0, float f1, float f2) {
    v.x=v.x*f0;
    v.y=v.y*f1;
    v.z=(v.z-2.0)*f2;
    return (vec3f){ (rmx[0][0]*v.x+rmx[0][1]*v.y+rmx[0][2]*v.z)+offsetx,
                    (rmx[1][0]*v.x+rmx[1][1]*v.y+rmx[1][2]*v.z)+offsety,
                    (rmx[2][0]*v.x+rmx[2][1]*v.y+rmx[2][2]*v.z)};
}

void bezier(vec3f const p[4][4], vec3f np[7][7]) {
    vec3f t[7][4];
    for(int i=0;i<4;i++) {
        t[0][i]=p[i][0];
        t[6][i]=p[i][3];
        vec3f m=mid3d(p[i][1],p[i][2]);
        t[1][i]=mid3d(p[i][0],p[i][1]);
        t[5][i]=mid3d(p[i][2],p[i][3]);
        t[2][i]=mid3d(t[1][i],m);
        t[4][i]=mid3d(t[5][i],m);
        t[3][i]=mid3d(t[2][i],t[4][i]);
    }
    for(int i=0;i<7;i++) {
        np[0][i]=t[i][0];
        np[6][i]=t[i][3];
        vec3f m=mid3d(t[i][1],t[i][2]);
        np[1][i]=mid3d(t[i][0],t[i][1]);
        np[5][i]=mid3d(t[i][2],t[i][3]);
        np[2][i]=mid3d(np[1][i],m);
        np[4][i]=mid3d(np[5][i],m);
        np[3][i]=mid3d(np[2][i],np[4][i]);
    }
}
//TaskHandle_t th;
//static EventGroupHandle_t draw_event_group=NULL;
/*
void teapottask(void *pvParameters ) {
    static vec3f np[7][7];
    static vec3f p[4][4];
    while(1) {
    xEventGroupWaitBits(draw_event_group,2,pdTRUE,pdFALSE,100);
    for(int ii=16;ii<32;ii++) {
        for(int j=0;j<4;j++) {
            for(int k=0;k<4; k++) {
                p[j][k]=vrotate(teapotVertices[teapotPatches[ii][j*4+k]-1],teapot_size,teapot_size,teapot_size);
            }
        }
        bezier(p,np);
        for(int j=1;j<7;j++) {
            for(int k=1;k<7;k++) {
                add_quad(np[j-1][k-1],np[j-1][k],np[j][k],np[j][k-1]);
            }
        }
    }
//    xTaskNotify(th,0,eNoAction);
    xEventGroupSetBits(draw_event_group,1);
    }
    vTaskDelete(NULL);
}
*/

void draw_teapot(vec2 pos, float size, vec3f rot, colourtype col) {
/*
    if(draw_event_group==NULL) {
        draw_event_group=xEventGroupCreate();
        xTaskCreate( &teapottask, "tptask", 2048, NULL, 5, NULL );
    }
    xEventGroupSetBits(draw_event_group,2);
    */
    static vec3f np[7][7];
    static vec3f p[4][4];
    diffuse=col;
    rotation=rot;
    offsetx=pos.x;
    offsety=pos.y;
    teapot_size=size;
    maketrotationmatrix();
    // 28-32=base
    // 20-27=lid
    // 16-19=spout
    // 12-15=handle
    // 8-11=bottom body
    // 0-7=top body

    nquads=0;
    for(int ii=0;ii<32;ii++) {
        for(int j=0;j<4;j++) {
            for(int k=0;k<4; k++) {
                p[j][k]=vrotate(teapotVertices[teapotPatches[ii][j*4+k]-1],teapot_size,teapot_size,teapot_size);
            }
        }
        bezier(p,np);
        for(int j=1;j<7;j++) {
            for(int k=1;k<7;k++) {
                add_quad(np[j-1][k-1],np[j-1][k],np[j][k],np[j][k-1]);
            }
        }
    }
    //xTaskNotifyStateClear( NULL );
 //   xEventGroupWaitBits(draw_event_group,1,pdTRUE,pdFALSE,100);
    

    //xTaskNotifyWait(0,0,NULL,portMAX_DELAY);
    draw_all_quads();
}