/*
 * MNT ZZ9000 Amiga Graphics and ARM Coprocessor SDK
 *            Code example: "vector"
 *
 * Copyright (C) 2019, Lukas F. Hartmann <lukas@mntre.com>
 *                     MNT Research GmbH, Berlin
 *                     https://mntre.com
 *
 * More Info: https://mntre.com/zz9000
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * GNU General Public License v3.0 or later
 *
 * https://spdx.org/licenses/GPL-3.0-or-later.html
 */

/*
  Run it like this:

  zz9k load vector.bin
  zz9k run -320x240 !screen !width
*/

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "printf/printf.h"
#include "zz9k_env.h"

typedef struct Vec2 {
	float x;
	float y;
} Vec2;

typedef struct Vec3 {
	float x,y,z;
} Vec3;

typedef struct Tri3 {
	uint32_t color1;
	uint32_t color2;
	uint32_t color3;
	Vec3 a;
	Vec3 b;
	Vec3 c;
} Tri3;

static uint32_t* fb=0;
static uint32_t fb_pitch=0;

struct ZZ9K_ENV* _zz9k_env;

void __aeabi_idiv0(int r) {
  while (1) {
    printf("__aeabi_idiv0()!\n");
  }
}
void __aeabi_ldiv0(int r) {
  while (1) {
    printf("__aeabi_idiv0()!\n");
  }
}
int errno_var = 0;

int* __errno() {
  return &errno_var;
}
void _putchar(char c) {
  _zz9k_env->putchar(c);
};

void set_fb(uint32_t* fb_, uint32_t pitch) {
	fb=fb_;
	fb_pitch=pitch;
}

void horizline(uint16_t x1, uint16_t x2, uint16_t y, uint32_t color) {
	uint32_t* p=fb+y*fb_pitch;
	uint16_t tmp;
	if (x2>x1) {
		tmp=x1; x1=x2; x2=tmp;
	}
	while (x1>x2) {
		p[x1]=(p[x1]+color)/2;
    x1--;
	}
}

void horizline2(uint16_t x1, uint16_t x2, uint16_t y, uint32_t color1, uint32_t color2, uint32_t z) {
	uint32_t* p=fb+y*fb_pitch;
	uint16_t tmp;
	if (x2>x1) {
		tmp=x1; x1=x2; x2=tmp;
	}
  float b1=(color1&0xff0000)>>16;
  float g1=(color1&0xff00)>>8;
  float r1=color1&0xff;
  
  float b2=(color2&0xff0000)>>16;
  float g2=(color2&0xff00)>>8;
  float r2=color2&0xff;

  float d = x1-x2;
  float fb1 = (b2-b1)/d;
  float fg1 = (g2-g1)/d;
  float fr1 = (r2-r1)/d;
  
	while (x1>x2) {
    float zz = p[x1]>>24;
    if (zz<z) {
      p[x1]=((uint32_t)z<<24)|((uint32_t)b2<<16)|((uint32_t)g2<<8)|((uint32_t)r2);
    }
    x1--;
    b2+=fb1;
    g2+=fg1;
    r2+=fr1;
	}
}

void fill_triangle(Vec2 A, Vec2 B, Vec2 C, uint32_t color) {
	float dx1 = 0;
	float dx2 = 0;
	float dx3 = 0;

	// sort points by Y coord
	Vec2 tmp;
	if (A.y>B.y) { tmp=A; A=B; B=tmp; }
	if (B.y>C.y) {
		tmp=B; B=C; C=tmp;
		if (A.y>B.y) { tmp=A; A=B; B=tmp; }
	}
	//printf("triangle: %f,%f %f,%f %f,%f\n",A.x,A.y,B.x,B.y,C.x,C.y);

	if (B.y-A.y > 0) dx1=(B.x-A.x)/(B.y-A.y); else dx1=0;
	if (C.y-A.y > 0) dx2=(C.x-A.x)/(C.y-A.y); else dx2=0;
	if (C.y-B.y > 0) dx3=(C.x-B.x)/(C.y-B.y); else dx3=0;

	Vec2 S=A;
	Vec2 E=A;

	if(dx1 > dx2) {
		for(;S.y<=B.y;S.y++,E.y++,S.x+=dx2,E.x+=dx1)
			horizline(S.x,E.x,S.y,color);
		E=B;
		for(;S.y<=C.y;S.y++,E.y++,S.x+=dx2,E.x+=dx3)
			horizline(S.x,E.x,S.y,color);
	} else {
		for(;S.y<=B.y;S.y++,E.y++,S.x+=dx1,E.x+=dx2)
			horizline(S.x,E.x,S.y,color);
		S=B;
		for(;S.y<=C.y;S.y++,E.y++,S.x+=dx3,E.x+=dx2)
			horizline(S.x,E.x,S.y,color);
	}
}

void fill_triangle2(Vec2 A, Vec2 B, Vec2 C, uint32_t color1, uint32_t color2, uint32_t color3,float z) {
	float dx1 = 0;
	float dx2 = 0;
	float dx3 = 0;

	// sort points by Y coord
	Vec2 tmp;
	if (A.y>B.y) { tmp=A; A=B; B=tmp; }
	if (B.y>C.y) {
		tmp=B; B=C; C=tmp;
		if (A.y>B.y) { tmp=A; A=B; B=tmp; }
	}
	//printf("triangle: %f,%f %f,%f %f,%f\n",A.x,A.y,B.x,B.y,C.x,C.y);

	if (B.y-A.y > 0) dx1=(B.x-A.x)/(B.y-A.y); else dx1=0;
	if (C.y-A.y > 0) dx2=(C.x-A.x)/(C.y-A.y); else dx2=0;
	if (C.y-B.y > 0) dx3=(C.x-B.x)/(C.y-B.y); else dx3=0;

	Vec2 S=A;
	Vec2 E=A;

	if(dx1 > dx2) {
		for(;S.y<=B.y;S.y++,E.y++,S.x+=dx2,E.x+=dx1)
			horizline2(S.x,E.x,S.y,color1,color2,z);
		E=B;
		for(;S.y<=C.y;S.y++,E.y++,S.x+=dx2,E.x+=dx3)
			horizline2(S.x,E.x,S.y,color1,color2,z);
	} else {
		for(;S.y<=B.y;S.y++,E.y++,S.x+=dx1,E.x+=dx2)
			horizline2(S.x,E.x,S.y,color1,color2,z);
		S=B;
		for(;S.y<=C.y;S.y++,E.y++,S.x+=dx3,E.x+=dx2)
			horizline2(S.x,E.x,S.y,color1,color2,z);
	}
}

void fill_rect32(uint16_t rect_x1, uint16_t rect_y1, uint16_t rect_x2, uint16_t rect_y2, uint32_t rect_rgb) {
	for (uint16_t y=rect_y1; y<=rect_y2; y++) {
		uint32_t* p=fb+y*fb_pitch;
		for (uint16_t x=rect_x1; x<=rect_x2; x++) {
			p[x]=rect_rgb;
		}
	}
}

void copy_rect32(uint16_t rect_x1, uint16_t rect_y1, uint16_t rect_x2, uint16_t rect_y2, uint16_t rect_sx, uint16_t rect_sy) {
	int8_t ystep=1, xstep=1;
	uint16_t tmp;
	if (rect_sy < rect_y1) {
		uint16_t h = rect_y2-rect_y1;
		ystep=-1;
		tmp=rect_y2; rect_y2=rect_y1; rect_y1=tmp;
		rect_sy+=h;
	}
	if (rect_sx < rect_x1) {
		uint16_t w = rect_x2-rect_x1;
		xstep=-1;
		tmp=rect_x2; rect_x2=rect_x1; rect_x1=tmp;
		rect_sx+=w;
	}
	rect_y2+=ystep;
	rect_x2+=xstep;
	for (uint16_t sy=rect_sy, dy=rect_y1; dy!=rect_y2; sy+=ystep, dy+=ystep) {
		uint32_t* dp=(uint32_t*)(fb+dy*fb_pitch);
		uint32_t* sp=(uint32_t*)(fb+sy*fb_pitch);
		for (uint16_t sx=rect_sx, dx=rect_x1; dx!=rect_x2; sx+=xstep, dx+=xstep) {
			dp[dx]=sp[sx];
		}
	}
}

float rot_matrix[4][4];
float matrix_out[4][1];
float matrix_in[4][1];

void multiply_matrix()
{
	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 1; j++) {
			matrix_out[i][j] = 0;
			for(int k = 0; k < 4; k++) {
				matrix_out[i][j] += rot_matrix[i][k] * matrix_in[k][j];
			}
		}
	}
}

void init_rot_matrix(float angle, float u, float v, float w)
{
    float L = (u*u + v * v + w * w);
    angle = angle * M_PI / 180.0; //converting to radian value
    float u2 = u * u;
    float v2 = v * v;
    float w2 = w * w;

    rot_matrix[0][0] = (u2 + (v2 + w2) * cos(angle)) / L;
    rot_matrix[0][1] = (u * v * (1 - cos(angle)) - w * sqrt(L) * sin(angle)) / L;
    rot_matrix[0][2] = (u * w * (1 - cos(angle)) + v * sqrt(L) * sin(angle)) / L;
    rot_matrix[0][3] = 0.0;

    rot_matrix[1][0] = (u * v * (1 - cos(angle)) + w * sqrt(L) * sin(angle)) / L;
    rot_matrix[1][1] = (v2 + (u2 + w2) * cos(angle)) / L;
    rot_matrix[1][2] = (v * w * (1 - cos(angle)) - u * sqrt(L) * sin(angle)) / L;
    rot_matrix[1][3] = 0.0;

    rot_matrix[2][0] = (u * w * (1 - cos(angle)) - v * sqrt(L) * sin(angle)) / L;
    rot_matrix[2][1] = (v * w * (1 - cos(angle)) + u * sqrt(L) * sin(angle)) / L;
    rot_matrix[2][2] = (w2 + (u2 + v2) * cos(angle)) / L;
    rot_matrix[2][3] = 0.0;

    rot_matrix[3][0] = 0.0;
    rot_matrix[3][1] = 0.0;
    rot_matrix[3][2] = 0.0;
    rot_matrix[3][3] = 1.0;
}

static int last_angle=0;

void render_faces(float screen_x, float screen_y, float y_angle) {

	fill_rect32(screen_x-150,screen_y-100,screen_x+150,screen_y+100,0);

	init_rot_matrix(y_angle,1,1,0);

	// BGRA
	// front
	Tri3 triangles[] = {
    { 0x00000000, 0x0000ffff, 0x00ffffff, {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}},
	  //{ 0x00008888, {-1,-1,-1}, { 1, 1,-1}, {-1, 1,-1}},
    
	  // back
	  { 0x00000000, 0x00ffff00, 0x00ffffff, {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}},
	  //{ 0x00888800, {-1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}},
    //
	  { 0x00000000, 0x00ff00ff, 0x00ffffff, {-1,-1,-1}, {-1,-1, 1}, {-1, 1,-1}},
	  //{ 0x00880088, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}},
	};

	float trans_z=2;
	float scale=64;

	matrix_in[3][0]=1;

	for (int i=0; i<3; i++) {
		Tri3 t = triangles[i];

		matrix_in[0][0]=t.a.x;
		matrix_in[1][0]=t.a.y;
		matrix_in[2][0]=t.a.z;
		multiply_matrix();
		float x=matrix_out[0][0], y=matrix_out[1][0], z=matrix_out[2][0];
		Vec2 a = {screen_x+scale*x/(trans_z+z), screen_y+scale*y/(trans_z+z)};

		matrix_in[0][0]=t.b.x;
		matrix_in[1][0]=t.b.y;
		matrix_in[2][0]=t.b.z;
		multiply_matrix();
		x=matrix_out[0][0], y=matrix_out[1][0], z=matrix_out[2][0];
		Vec2 b = {screen_x+scale*x/(trans_z+z), screen_y+scale*y/(trans_z+z)};

		matrix_in[0][0]=t.c.x;
		matrix_in[1][0]=t.c.y;
		matrix_in[2][0]=t.c.z;
		multiply_matrix();
		x=matrix_out[0][0], y=matrix_out[1][0], z=matrix_out[2][0];
		Vec2 c = {screen_x+scale*x/(trans_z+z), screen_y+scale*y/(trans_z+z)};

		fill_triangle2(a,b,c,t.color1,t.color2,t.color3,128-z);
	}

  copy_rect32(screen_x-150,screen_y-100-500,screen_x+150,screen_y+100-500, screen_x-150, screen_y-100);
}

int __attribute__ ((section (".binstart"))) main(struct ZZ9K_ENV* env) {
  _zz9k_env = env;

  if (!env) {
    return 1;
  }
  
  if (env->argc<2) {
    return 1;
  }

  // arg0: framebuffer pointer
  // arg1: screen width
  set_fb((uint32_t*)env->argv[0],env->argv[1]);
  
	fill_rect32(0,0,319,240-13,0);

  float angle = 0;
  while (1) {
    render_faces(env->argv[1]/2,env->argv[1]/3+500,angle);
    angle+=1.0;
  }
  
  return 0;
}
