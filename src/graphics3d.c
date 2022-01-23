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

void add_quad(vec3f p0, vec3f p1, vec3f p2, vec3f p3) {
    
    vec3f normal=cross3d(sub3d(p2,p0),sub3d(p3,p0));
    if(normal.z<=0) return;
    normal=normalise(normal);
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
    quads[nquads++]=(quadtype){{a.x,a.y,b.x,b.y,c.x,c.y,d.x,d.y},
                        colour,(int16_t)((p0.z+p1.z+p2.z+p3.z)*64)};
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


void draw_teapot(vec2 pos, float size, vec3f rot, colourtype col) {

    static vec3f np[7][7];
    static vec3f p[4][4];
    diffuse=col;
    rotation=rot;
    offsetx=pos.x;
    offsety=pos.y;
    teapot_size=size;
    maketrotationmatrix();

    // the teapot is made from 32 patches as follows:
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
    draw_all_quads();
}