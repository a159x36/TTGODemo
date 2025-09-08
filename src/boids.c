#include <graphics.h>
#include <fonts.h>
#include <input_output.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

int NPREDATORS=0;
int NBOIDS=400;
const int PRED_FLOCK=7;
const int NEIGH=10;
const int GRIDSIZE=15;
const int NFLOCKS=2; // up to 6
const float MAXACC=0.5f;
const float MAXVEL=2.0f;
const float PREDMAXVEL=2.2f;
const float PREDMASS=0.1f;
const float MINVEL=1.0f;
float FCOHESION=0.05f;
float FALIGN=0.05f;
float FSEPARATION=0.4f;
const float FPRED=2.0f;
const float FOBJAVOID=5.0f;
int NSPHERES=0;
#define MAXSPHERES 3
#define MAXPREDATORS 3

typedef struct boid {
    vec2f pos;      // boid position
    vec2f vel;      // boid velocity
    vec2f acc;      // boid acceleration
    int flock;      // which flock is it in
    struct boid *next;  // linked list for grid
    struct boid *prev;  // previous in list
} boid;

static inline vec2f limitmax(vec2f v,float max) {
    float d2=mag2d(v);
    if(d2>max*max) {
        float mag=Q_rsqrt(d2)*max;
        return (vec2f){v.x*mag, v.y*mag};
    }
    return v;
}
static inline vec2f limitmin(vec2f v,float min) {
    float d2=mag2d(v);
    if(d2<0.001) {
        return (vec2f){1,0};
    }
    if(d2<min*min) {
        float mag=Q_rsqrt(d2)*min;
        return (vec2f){v.x*mag, v.y*mag};
    }
    return v;
}

#define TOGRID(x) (((int)(x))/GRIDSIZE)
#define TOGI(x,y) (TOGRID(x)+gridw*TOGRID(y))

typedef struct sphere {
    vec2f centre;
    float radius;
} sphere;


const char * const mmenu[]={"Exit","Boids","Objects","Cohesion","Alignment","Separation","Predators",NULL};
const char * const bmenu[]={"Exit","100","200","400","800",NULL};
const char * const smenu[]={"Exit","0","1","2","3",NULL};
const char * const casmenu[]={"Exit","0.01","0.02","0.04","0.08","0.1","0.2","0.5","0.8","1.6","3.0",NULL};
const char * const * const menus[]={mmenu,bmenu,smenu,casmenu,casmenu,casmenu,smenu}; 

int menu_no=-1;
int sel=0;
int show_menu(int key) {
    if(key==RIGHT_DOWN && menu_no==-1) {
        menu_no=0;
        sel=0;
        return 0;
    }
    if(menu_no==-1) return 0;

    draw_rectangle(0,sel*9+9,60,8,rgbToColour(100,100,100));
    draw_rectangle(0,0,60,8,rgbToColour(200,200,0));
    setFont(FONT_SMALL);
    setFontColour(0,0,0);
    if(menu_no==0)
        print_xy("Menu",0,0);
    else
        print_xy((char *)(menus[0][menu_no]),0,0);
    setFontColour(255,255,255);
    print_xy("",0,116);
    gprintf("Boids=%d Spheres=%d Cohesion=%.2f\n\
Alignment=%.2f Separation=%.2f Predators=%d", \
        NBOIDS,NSPHERES,FCOHESION,FALIGN,FSEPARATION,NPREDATORS);
    
    int entries=0;
    while(menus[menu_no][entries]) entries++;
    for(int i=0;i<entries;i++)
        print_xy((char *)(menus[menu_no][i]),0,i*9+9);
    if(key==LEFT_DOWN)
        sel=(sel+1)%entries;
    if(key==RIGHT_DOWN) {
        if(sel==0 && menu_no>0) {
            menu_no=0;
            return 1;
        }
        switch (menu_no) {
            case 0:
                if(sel==0) menu_no=-1;
                else {
                    menu_no=sel;
                    sel=0;
                }
                return 1;
            case 1:
                NBOIDS = atoi(bmenu[sel]);
                return 2;
            case 2:
                NSPHERES = atoi(smenu[sel]);
                return 2;
            case 3:
                FCOHESION = atof(casmenu[sel]);
                return 1;
            case 4:
                FALIGN = atof(casmenu[sel]);
                return 1;
            case 5:
                FSEPARATION = atof(casmenu[sel]);
                return 1;
            case 6:
                NPREDATORS = atoi(smenu[sel]);
                return 2;
        }
    }

    return 1;
}

void init() {

}
// boids simulation
// see: http://www.red3d.com/cwr/boids/
// for info on the algorithm
void boids_demo() {
    while(1) {
        // uses a grid to speed up neighbour lookups
        int gridw=(display_width+GRIDSIZE-1)/GRIDSIZE;
        int gridh=(display_height+GRIDSIZE-1)/GRIDSIZE;
        boid *boids = malloc(sizeof(boid)*NBOIDS);
        boid **grid = calloc(gridh*gridw,sizeof(boid *));
        uint16_t cols[6];
        for(int i=0;i<6;i++) {
            cols[i]=rgbToColour(((i+1)&1)*255,((i+1)&2)*127,((i+1)&4)*63);
        }
        sphere spheres[MAXSPHERES];
        for(int i=0;i<NBOIDS;i++) {
            boid *bp=boids+i;
            float angle=(rand()%314159)/50000.0f;
            bp->pos=(vec2f){rand()%display_width/2+display_width/4,
                            rand()%display_height/2+display_height/4};
            bp->vel=(vec2f){2.0f*cos(angle),2.0f*sin(angle)};
            bp->flock=rand()%NFLOCKS;
            if(i<NPREDATORS)
                bp->flock=PRED_FLOCK;
            int gi=TOGI(bp->pos.x,bp->pos.y);
            bp->next=grid[gi];
            if(bp->next!=NULL)
                bp->next->prev=bp;
            bp->prev=NULL;
            grid[gi]=bp;
        }
        spheres[0]=(sphere){(vec2f){50.0f,50.0f},20.0f};
        spheres[1]=(sphere){(vec2f){200.0f,100.0f},20.0f};
        spheres[2]=(sphere){(vec2f){120.0f,80.0f},15.0f};
        
        while(1) {
            cls(0);
            int nearest[MAXPREDATORS]={0};
            float min_d2=display_width*display_width*display_width;
            for(int i=0;i<NSPHERES;i++) {
                int x,x1,y,y1;
                x1=spheres[i].centre.x;
                y1=spheres[i].centre.y+spheres[i].radius;
                const float step=(float)PI/20.0f;
                for(float a=step;a<2*(float)PI+step;a+=step) {
                    x=(int)(spheres[i].centre.x+spheres[i].radius*sin(a));
                    y=(int)(spheres[i].centre.y+spheres[i].radius*cos(a));
                    draw_line(x1,y1,x,y,-1);
                    x1=x;y1=y;
                }
            }
            for(int i=0;i<NBOIDS;i++) {
                boid *bp=boids+i;
                boid *b1p=boids;
                vec2f bpos=bp->pos;
                if(bp->flock!=PRED_FLOCK) // boids are lines
                    draw_line(bpos.x,bpos.y,bpos.x+bp->vel.x*4.0,bpos.y+bp->vel.y*4.0,cols[bp->flock]);
                else
                    draw_rectangle(bpos.x-2,bpos.y-2,5,5,-1); // predators are squares
                int neighb=0;
                vec2f avoid=(vec2f){0,0}; // for separattion
                vec2f nvel=(vec2f){0,0};  // for alignment
                vec2f npos=(vec2f){0,0};  // for cohesion
                float N2=(float)(NEIGH*NEIGH);    
                bp->acc=(vec2f){0,0}; 
                if(bp->flock!=PRED_FLOCK) {       
                    // look at all neightbours within NEIGH radius
                    for(int xi=TOGRID(bpos.x-NEIGH);xi<=TOGRID(bpos.x+NEIGH);xi++) {
                        if(xi>=0 && xi<gridw) {
                            for(int yi=TOGRID(bpos.y-NEIGH);yi<=TOGRID(bpos.y+NEIGH);yi++) {
                                if(yi>=0 && yi<gridh) {
                                    int gi=yi*gridw+xi;
                                    b1p=grid[gi];
                                    while(b1p!=NULL) {
                                        vec2f dif=sub2d(bpos,b1p->pos);
                                        float d2=mag2d(dif);
                                        if(d2<N2 && d2!=0) {
                                            float mag=recip(d2);
                                            dif=mul2d(mag,dif);
                                            avoid=add2d(avoid,dif);
                                            if(bp->flock==b1p->flock) {
                                                nvel=add2d(nvel,b1p->vel);
                                                npos=add2d(npos,b1p->pos);
                                                neighb++;
                                            }
                                        }
                                        b1p=b1p->next;
                                    }
                                }
                            }
                        }
                    }
                    if(neighb!=0) {
                        float ineighb=recip(neighb);
                        avoid=mul2d(FSEPARATION,avoid);
                        avoid=limitmax(avoid,MAXACC);
                        nvel=mul2d(ineighb,nvel);
                        nvel=mul2d(FALIGN,sub2d(nvel,bp->vel));
                        nvel=limitmax(nvel,MAXACC);
                        npos=mul2d(ineighb,npos);
                        npos=mul2d(FCOHESION,sub2d(npos,bp->pos));
                        npos=limitmax(npos,MAXACC);
                        bp->acc=add2d(avoid,add2d(npos,nvel));
                    }   
                    // predator fleeing
                    for(int j=0;j<NPREDATORS;j++) {
                        vec2f dif=sub2d(bpos,boids[j].pos);
                        float d2=mag2d(dif);
                        if(d2>0 && bp->flock<2) { 
                            // find closest edible boid going in same direction as predator
                            if(d2<min_d2 && ((bp->vel.x*boids[j].vel.x)>0 || 
                                (bp->vel.y*boids[j].vel.y)>0)) {
                                min_d2=d2;
                                nearest[j]=i;
                            }
                            d2=recip(d2);
                            dif=limitmax(mul2d(FPRED*d2,dif),MAXACC);
                            bp->acc=add2d(bp->acc,dif);
                        }
                    }
                } 
                // object avoidance using ray casting
                // in 3d the objects would be spheres 
                // but in 2d they are just circles
                float mint=40.0f; // minimum distance to a sphere
                int spcl=-1; // index of closest sphere
                vec2f d=normalise2d(bp->vel);
                for(int i=0;i<NSPHERES;i++) {
                    // project a ray along our velocity vector
                    // and see if it hits a sphere
                    float r=spheres[i].radius+1;
                    vec2f oc=sub2d(bp->pos,spheres[i].centre);
                    float b=2.0f*dot2d(oc,d);
                    float c=mag2d(oc)-r*r;
                    float bac=b*b-4.0f*c;
                    if(bac>=0) {
                        bac=sqrtf(bac);
                        float t0=(-b+bac)*0.5f;
                        float t1=(-b-bac)*0.5f;
                        if(t1*t0<0) {
                            // inside a sphere, get out of here!!
                            mint=0;
                            spcl=i;
                        } else {
                            if(t0>=0 && t0<mint) {
                                mint=t0;
                                spcl=i;
                            }
                            if(t1>=0 && t1<mint) {
                                mint=t1;
                                spcl=i;
                            }
                        }
                    }
                }
                if(spcl!=-1) { // if we are going to hit a circle
                    if(mag2d(bp->vel)==0)
                        continue; 
                    vec2f oc=sub2d(spheres[spcl].centre,bp->pos);
                //    vec2f inter=add2d(bp->pos,mul2d(mint,d));
                //    draw_line(bp->pos.x,bp->pos.y,inter.x,inter.y,rgbToColour(0,255,255));
                //   printf("%f %f %f\n",inter.x,inter.y,mint);
                    
                    float oe=dot2d(oc,d);
                    vec2f e=add2d(bp->pos,mul2d(oe,d));

                    vec2f ce=normalise2d(sub2d(e,spheres[spcl].centre));
                    vec2f dif=limitmax(mul2d(FOBJAVOID,ce),MAXACC*2.0f);
                //  printf("%f,%f,%f,%f\n",d.x,d.y,ce.x,dif.x);
                    bp->acc=add2d(bp->acc,dif);
                }
            }
            // look at all the predators
            for(int i=0;i<NPREDATORS;i++) {
                if(nearest[i]!=0) {
                    // object avoidance takes precedance
                    if(boids[i].acc.x!=0 || boids[i].acc.y!=0)
                        continue;
                    // go towards nearest boid
                    boid *bp=boids+nearest[i];
                    vec2f dif=sub2d(bp->pos,boids[i].pos);
                    float mag=mag2d(dif);
                    // we caught it!!
                    if(mag<1) {
                        bp->flock=2;
                    }
                    dif=mul2d(PREDMASS,normalise2d(dif));
                    boids[i].acc=dif;
                }
            }
            // update positions and velocities
            for(int i=0;i<NBOIDS;i++) {
                boid *bp=boids+i;
                int ogi=TOGI(bp->pos.x,bp->pos.y);
                bp->vel=add2d(bp->acc,bp->vel);
                if(bp->flock==PRED_FLOCK)
                    bp->vel=limitmax(bp->vel,PREDMAXVEL);
                else
                    bp->vel=limitmax(bp->vel,MAXVEL);
                bp->vel=limitmin(bp->vel,MINVEL);
                bp->pos=add2d(bp->pos,bp->vel);
                // bounce off walls
                if(bp->pos.x<0 || bp->pos.x>=display_width) {
                    bp->vel.x=-bp->vel.x;
                    bp->pos.x+=bp->vel.x;
                }
                if(bp->pos.y<0 || bp->pos.y>=display_height) {
                    bp->vel.y=-bp->vel.y;
                    bp->pos.y+=bp->vel.y;
                }
                // get new grid index
                int gi=TOGI(bp->pos.x,bp->pos.y);
                if(gi>gridw*gridh || gi<0) {
                    printf("Gi error %d %f %f\n",gi, bp->pos.x, bp->pos.y);
                    return;
                }
                // if it's moved to a different grid square
                if(gi!=ogi) {
                    // take it out of current list
                    if(bp->next!=NULL)
                        bp->next->prev=bp->prev;
                    if(bp->prev!=NULL)
                        bp->prev->next=bp->next;
                    else 
                        grid[ogi]=bp->next;
                    // add to new list
                    bp->next=grid[gi];
                    if(bp->next!=NULL)
                        bp->next->prev=bp;
                    bp->prev=NULL;
                    grid[gi]=bp;
                }
            }
            key_type key=get_input();
            int m=show_menu(key);
            if(m==2) {
                free(boids);
                free(grid);
                break;
            }
            if(!m) {
                if(key==LEFT_DOWN) {
                    free(boids);
                    free(grid);
                    return;
                }
            }
            flip_frame();
            showfps();
        }
    }
}