#include "../mylibc/mylibc.h"

#include "model3d.h"

#define SCRW 320
#define SCRH 200

int  sorted[NTRIS]; // key<<16 | id   (64K triangles max ... anyway)

void init_sort()
{
  for (int i = 0; i < NTRIS ; i++) {    
    sorted[i] = i;
  }
}

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

void update_sort()
{
  unsigned int *trpts = (unsigned int *)0x10000;  
  for (int i = 0; i < NTRIS ; i++) {    
    int t  = sorted[i]&65535;
    int t3 = t + (t<<1); // t*3
    int z0 = (trpts[idx[t3+0]]>>20)&1023;
    int z1 = (trpts[idx[t3+1]]>>20)&1023;
    int z2 = (trpts[idx[t3+2]]>>20)&1023;
    int z  = (z0+z1+z2);
    sorted[i] = (z<<16 | t);
  }
}

void sort() // bubble sort, assumes order is almost always correct
{
  int done = 0;
  while (!done) {
    done = 1;
    for (int i = 0; i < NTRIS-1 ; i++) {
      int n = i + 1;
      if (sorted[n] > sorted[i]) {
        int tmp   = sorted[n];
        sorted[n] = sorted[i];
        sorted[i] = tmp;
        done      = 0;
      }
    }
  }
}

void transform_points(const int *M)
{
  *(TRIANGLE+ 7) = (M[0]&1023) | ((M[1]&1023)<<8) | ((M[2]&1023)<<16);
  *(TRIANGLE+ 8) = (M[3]&1023) | ((M[4]&1023)<<8) | ((M[5]&1023)<<16);
  *(TRIANGLE+ 9) = (M[6]&1023) | ((M[7]&1023)<<8) | ((M[8]&1023)<<16);
  if (fbuffer) {
    *(TRIANGLE+10) = (SCRW/4)     | ((SCRH/2)<<16);
  } else {
    *(TRIANGLE+10) = (SCRW/2 + 4) | ((SCRH/4)<<16);
  }
  *(TRIANGLE+11) = 1; // reinit write address
  for (int p = 0; p < NVERTS*3 ; p = p + 3) {
    *(TRIANGLE+12) = (pts[p+0]&1023) | ((pts[p+1]&1023) << 10) | ((pts[p+2]&1023) << 20);
  }
}

void draw_triangle_raw(int t,char color,unsigned int p0,unsigned int p1,unsigned int p2)
{
  int tmp;
  
  int px0 = p0 & 1023;
  int px1 = p1 & 1023;
  int px2 = p2 & 1023;
  int py0 = (p0>>10) & 1023;
  int py1 = (p1>>10) & 1023;
  int py2 = (p2>>10) & 1023;
  
  // front facing?
  int d10x  = px1 - px0;
  int d10y  = py1 - py0;
  int d20x  = px2 - px0;
  int d20y  = py2 - py0;
  int cross = d10x*d20y - d10y*d20x;  
  if (cross <= 0) return;
  
  color    = color + (cross*inv_area[t])>>11;

  // 0 smallest y , 2 largest y
  if (py0 > py1) {
    tmp = py1; py1 = py0; py0 = tmp;
    tmp = px1; px1 = px0; px0 = tmp;
  }
  if (py0 > py2) {
    tmp = py2; py2 = py0; py0 = tmp;
    tmp = px2; px2 = px0; px0 = tmp;
  }
  if (py1 > py2) {
    tmp = py2; py2 = py1; py1 = tmp;
    tmp = px2; px2 = px1; px1 = tmp;
  }

  int e_incr0 = (py1-py0 == 0) ? 0xFFFFF : ((px1-px0)<<10) / (py1-py0);
  int e_incr1 = (py2-py1 == 0) ? 0xFFFFF : ((px2-px1)<<10) / (py2-py1);
  int e_incr2 = (py2-py0 == 0) ? 0xFFFFF : ((px2-px0)<<10) / (py2-py0);

  if ((e_incr0 == 0xFFFFF && e_incr1 == 0xFFFFF) 
   || (e_incr0 == 0xFFFFF && e_incr2 == 0xFFFFF) 
   || (e_incr1 == 0xFFFFF && e_incr2 == 0xFFFFF)) {
    // flat triangle
    return; 
  }

  // wait for any pending draw to complete
  while ((userdata()&1) == 1) {  }

  // send commands
  *(TRIANGLE+  0) = (px0&1023) | ((py0&1023) << 10);
  *(TRIANGLE+  1) = (px1&1023) | ((py1&1023) << 10);
  *(TRIANGLE+  2) = (px2&1023) | ((py2&1023) << 10);
  *(TRIANGLE+  3) = (e_incr0&0xffffff) | (color << 24);
  *(TRIANGLE+  4) = (e_incr1&0xffffff);
  *(TRIANGLE+  5) = (e_incr2&0xffffff);
}

void main()
{

  char a     = 66;
  char b     = 60;
  char c     = 64;
  int  frame = 0;
  
  // clear(0,0,SCRW,SCRH);
  
  *LEDS = 0;

  int posy = 0;
  int posx = 0;

  init_sort();

  unsigned int *trpts = (unsigned int *)0x10000;

  while(1) {
    
    clear(0,0,SCRW,SCRH);

    int Rx[9];
    rotX(Rx,64);
    int Ry[9];
    rotY(Ry,(a + frame)&255);
    int Sc[9];
    if (fbuffer) {
      scale3(Sc,64,127,127);
    } else {
      scale3(Sc,127,64,127);
    }
    int M[9];
    mulM(M,Ry,Rx);
    int F[9];
    mulM(F,Sc,M);

    ///////////////////////// transform
    int tm_trsf_start = time();
    transform_points(F);
    int tm_trsf_end   = time();

    ///////////////////////// sort
    int tm_sort_start = time();
    update_sort();
    int tm_sort_mid = time();
    sort();
    int tm_sort_end = time();
  
    ///////////////////////// frame buffer 0
    int tm_tris_start = time();
    for (int i = 0; i < NTRIS ; i++) {    
      int t  = sorted[i]&65535;
      int t3 = t + (t<<1);
      draw_triangle_raw(
        t,0,
        trpts[idx[t3+0]],trpts[idx[t3+1]],trpts[idx[t3+2]]
        );
    }
    int tm_tris_end = time();

    if (fbuffer == 0) {
      printf("trsf %d sort1 %d sort2 %d tris %d",tm_trsf_end-tm_trsf_start,tm_sort_mid-tm_sort_start,tm_sort_end-tm_sort_mid,tm_tris_end-tm_tris_start);
      set_cursor(4,0);
    }

    // wait for any pending draw to complete
    while ((userdata()&1) == 1) {  }
    // wait for vblank
    while ((userdata()&2) == 0) {  }
    // swap buffers
    *(LEDS+4) = 1;
    fbuffer = 1 - fbuffer;
    
    
/*
    ///////////////////////// frame buffer 1

    clear(0,0,SCRW,SCRH);
    
    for (int i = 0; i < NTRIS ; i++) {    
      int t = sorted[i]&65535;
      t += (t<<1);
      draw_triangle_raw(
        0,
        (((trpts[idx[t+0]]&1023))), (((trpts[idx[t+0]]>>10)&1023)), 
        (((trpts[idx[t+1]]&1023))), (((trpts[idx[t+1]]>>10)&1023)), 
        (((trpts[idx[t+2]]&1023))), (((trpts[idx[t+2]]>>10)&1023)) 
        );
    }

    // wait for any pending draw to complete
    while ((userdata()&1) == 1) {  }
    // wait for vblank
    while ((userdata()&2) == 0) {  }
    // swap buffers
    *(LEDS+4) = 1;  
    fbuffer = 1 - fbuffer;
*/   
    ///////////////////////// next
    ++frame;
  
    // pause(200000);
    
  }

}
