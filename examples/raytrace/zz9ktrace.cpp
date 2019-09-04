// vim: set et sw=2:

// compile with:
// c++ -O5 -ffast-math -funsafe-math-optimizations [-lm]

// adapted freely from http://fabiensanglard.net/postcard_pathtracer/index.html
// modifications by PoroCYon/K2^TiTAN under CC0, see https://creativecommons.org/publicdomain/zero/1.0/legalcode

//#define PATHTRACE /* don't define if you want simple raytracing */
#define USE_FRAMEBUFFER /* don't define to export a PPM to stdout */
//#define RAYMARCH_NAIVE /* you most likely don't want this */

#define CANVAS_WIDTH  (320)
#define CANVAS_HEIGHT (240)
#define PATHTRACE_SAMPLE_COUNT (4) /* you want at least 16 for a good result,
                                      but anything above 4 is horribly slow */
#define CAMERA_POS_X (-22)
#define CAMERA_POS_Y (  5)
#define CAMERA_POS_Z ( 25)
#define CAMERA_LOOKAT_X (-3)
#define CAMERA_LOOKAT_Y ( 4)
#define CAMERA_LOOKAT_Z ( 0)
#define MNTMN_LOGO_X (0)
#define MNTMN_LOGO_Y (0)
#define MNTMN_LOGO_Z (0)
#define BALL_X ( 0)
#define BALL_Y ( 5)
#define BALL_Z (-5)

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#ifndef USE_FRAMEBUFFER
#include <stdio.h>
#endif

struct Vec {
  float x, y, z;

  Vec(float v = 0) { x = y = z = v; }
  Vec(float a, float b, float c = 0) { x = a; y = b; z = c;}

  Vec operator+(Vec r) { return Vec(x + r.x, y + r.y, z + r.z); }
  Vec operator*(Vec r) { return Vec(x * r.x, y * r.y, z * r.z); }
  // dot product
  float operator%(Vec r) { return x * r.x + y * r.y + z * r.z; }
  // inverse square root
  Vec operator!() {return *this * (1 / sqrtf(*this % *this) );}
};

float max(float l, float r) { return l > r ? l : r; }
float min(float l, float r) { return l < r ? l : r; }
#ifdef PATHTRACE
float randomVal() { return (float) rand() / RAND_MAX; } // TODO: use a different PRNG
#endif

// Rectangle CSG equation. Returns minimum signed distance from
// space carved by lowerLeft vertex and opposite rectangle
// vertex upperRight.
float BoxTest(Vec position, Vec lowerLeft, Vec upperRight) {
  lowerLeft = position + lowerLeft * -1;
  upperRight = upperRight + position * -1;
  return -min(
              min(
                  min(lowerLeft.x, upperRight.x),
                  min(lowerLeft.y, upperRight.y)
                  ),
              min(lowerLeft.z, upperRight.z));
}

#define HIT_NONE 0
#define HIT_LETTER 1
#define HIT_WALL 2
#define HIT_SUN 3
#define HIT_AMIGARED 4
#define HIT_LETTER_CONT 5

char letters[] = // line strip
  "2K2Q7I7aNYNaSYS_X_";

char get(char* x, int i) {
  return x[i]-((i<7)?(70+(i&1)):(88-(i&1)));
}
// Sample the world using Signed Distance Fields.
float sdf(Vec position, int *hitType) {
  float distance = 1e9;
  Vec f = position; // Flattened position (z=0)
  f.x -= MNTMN_LOGO_X;
  f.y -= MNTMN_LOGO_Y;
  f.z = MNTMN_LOGO_Z;
  
  for (size_t i = 0; i < sizeof(letters)-3; i += 2) {
    Vec begin = Vec(get(letters,i), get(letters,i + 1)) * .5;
    Vec e = Vec(get(letters,i + 2), get(letters,i + 3)) * .5 + begin * -1;
    Vec o = f + (begin + e * min(-min((begin + f * -1) % e / (e % e),
                                      0),
                                 1)
                 ) * -1;
    distance = min(distance, o % o); // compare squared distance.
  }
  distance = sqrtf(distance); // Get real distance, not square distance.
  distance = powf(powf(distance, 8) + powf(position.z, 8), .125) - .5;
  *hitType = HIT_LETTER;

  {
    // amiiiiiiiiigaaaaaaaaaaaaah
    Vec fp = position;
    fp.x -= BALL_X;
    fp.y -= BALL_Y;
    fp.z -= BALL_Z;
    const float rad = 5;
    const int quant = 9;
    // tf to sph. coord
    float th = acosf(fp.y/sqrtf(fp%fp)),
      ph = atanf(fp.z / fp.x)-M_PI/2;
    int thi = (int)(th*quant/M_PI), phi = (int)(ph*quant/M_PI);
    float dd = sqrtf(fp.x*fp.x+fp.y*fp.y+fp.z*fp.z) - rad;
    if (dd < distance) {
      *hitType = ((phi ^ thi) & 1) ? HIT_AMIGARED : HIT_LETTER_CONT;
      distance = dd;
    }
  }

  float roomDist;
  roomDist = min(// min(A,B) = Union with Constructive solid geometry
                 //-min carves an empty space
                 -min(// Lower room
                      BoxTest(position, Vec(-30, -.5, -30), Vec(30, 18, 30)),
                      // Upper room
                      BoxTest(position, Vec(-25, 17, -25), Vec(25, 20, 25))
                      ),
                 BoxTest( // Ceiling "planks" spaced 8 units apart.
                         Vec(fmodf(fabsf(position.x), 8),
                             position.y,
                             position.z),
                         Vec(1.5, 18.5, -25),
                         Vec(6.5, 20, 25)
                          )
                 );
  if (roomDist < distance) {distance = roomDist; *hitType = HIT_WALL;}

  float sun = 19.9 - position.y ; // Everything above 19.9 is light source.
  if (sun < distance){distance = sun; *hitType = HIT_SUN;}

  return distance;
}

// Perform signed sphere marching
// Returns hitType 0, 1, 2, or 3 and update hit position/normal
int RayMarching(Vec origin, Vec direction, Vec &hitPos, Vec &hitNorm) {
  int hitType = HIT_NONE;

#ifdef RAYMARCH_NAIVE
  // raymarching, naive implementation
  const float eps = 0.01;
  int noHitCount = 0;
  float d; // distance from closest object in world.
  // Signed distance marching
  for (float total_d=0; total_d < 100; total_d += d)
    if ((d = sdf(hitPos = origin + direction * total_d, &hitType)) < eps
        || noHitCount > 99) {
      hitNorm =
        !Vec(sdf(hitPos + Vec(.01, 0), &noHitCount) - d,
             sdf(hitPos + Vec(0, .01), &noHitCount) - d,
             sdf(hitPos + Vec(0, 0, .01), &noHitCount) - d);
      return hitType; // Weird return statement where a variable is also updated.
    } else ++noHitCount;
#else
  // "enhanced sphere tracing", keinert et al (aka Mercury)
  float O = 1.2, cer = 1e9, t = 0, /*ct = 0,*/ sl = 0, sg, prad = 0;
  sg = sdf(origin, &hitType);
  sg = (sg < 0) ? -1 : 1;
  const float pxrad = 0.5 / sqrtf(CANVAS_WIDTH*CANVAS_WIDTH+CANVAS_HEIGHT*CANVAS_HEIGHT);

  for (int i = 0; i < 100; ++i) {
    hitPos = origin + direction * t;
    float sr = sg * sdf(hitPos, &hitType);
    float rr = fabsf(sr);

    bool sfail = O > 1 && (rr + prad) < sl;
    if (sfail) {
      sl -= O*sl;
      O = 1;
    } else {
      sl = sr * O;
    }

    prad = rr;
    float err = rr / t;
    if (!sfail && err < cer) {
      //ct = t;
      cer = err;
    }
    if ((!sfail && err < pxrad) || t > 100) {
      int _;
      hitNorm =
        !Vec(sdf(hitPos + Vec(.01, 0), &_) - sr,
             sdf(hitPos + Vec(0, .01), &_) - sr,
             sdf(hitPos + Vec(0, 0, .01), &_) - sr);
      return (t > 100) ? 0 : hitType;
    }
    t += sl;
  }
#endif

  return 0;
}

Vec Trace(Vec origin, Vec direction) {
  Vec sampledPosition=origin, normal(1,0,0), color(0,0,0), attenuation = 1;
  Vec lightDirection(!Vec(.6, .6, 1)); // Directional light

  for (int bounceCount = 3; bounceCount--;) {
#ifdef PATHTRACE
    int hitType = RayMarching(origin, direction, sampledPosition, normal);
    if (hitType == HIT_NONE) break; // No hit. This is over, return color.
    else if (hitType == HIT_LETTER) { // Specular bounce on a letter. No color acc.
      //return Vec(1,0,0);
      direction = direction + normal * ( normal % direction * -2);
      origin = sampledPosition + direction * 0.1;
      attenuation = attenuation * 0.2; // Attenuation via distance traveled.
    }
    else if (hitType == HIT_WALL) { // Wall hit uses color yellow?
      //return Vec(0,1,0);
      float incidence = normal % lightDirection;
      float p = 6.283185 * randomVal();
      float c = randomVal();
      float s = sqrtf(1 - c);
      float g = normal.z < 0 ? -1 : 1;
      float u = -1 / (g + normal.z);
      float v = normal.x * normal.y * u;
      direction = Vec(v,
                      g + normal.y * normal.y * u,
                      -normal.y) * (cosf(p) * s)
                           +
                           Vec(1 + g * normal.x * normal.x * u,
                               g * v,
                               -g * normal.x) * (sinf(p) * s) + normal * sqrtf(c);
      origin = sampledPosition + direction * .1;
      attenuation = attenuation * 0.2;
      if (incidence > 0 &&
          RayMarching(sampledPosition + normal * .1,
                      lightDirection,
                      sampledPosition,
                      normal) == HIT_SUN)
        color = color + attenuation * Vec(500, 400, 100) * incidence;
    } else if (hitType == HIT_AMIGARED) {
      color = color + attenuation * Vec(80, 0, 0); break;
    }
    else {//if (hitType == HIT_SUN) { //
      //return Vec(0,0,1);
      color = color + attenuation * Vec(50, 80, 100); break; // Sun Color
    }
#else

    Vec lights[] = {
                    Vec(BALL_X,BALL_Y,BALL_Z), Vec(80,0,0), // amigaball
                    Vec(0,19.9,0), Vec(50,80,100), // sun
                    //Vec(3,6,3), Vec(0.1)
    };
    
    int hitType = RayMarching(origin, direction, sampledPosition, normal);
    if (hitType == HIT_NONE) return color; // No hit. This is over, return color.
    else if (hitType == HIT_LETTER || hitType == HIT_WALL || hitType==HIT_LETTER_CONT) {
      bool ltr = hitType != HIT_WALL;
      // obj color
      color = ltr ? Vec() : Vec(0.9,0.85,0.8);

      // I'm so sorry
      for (size_t i = 0; i < sizeof(lights)/(sizeof(lights[0])); i+=2) {
        Vec ld = lights[i] + sampledPosition*(-1), mld = ld*(-1);
        Vec lc = lights[i+1];
        float len = sqrtf(ld%ld);
        ld = ld*(1/len);
        float latt = min(1/(4*len*len),1);
        // I, N --> I - 2.0 * dot(N, I) * N
        Vec refl = mld+normal*(-2*(normal%mld));

        float amb = 0.1;
        float spp = 1;
        float dif = max(0,normal%ld);
        double spc = max(0,refl%!(origin+sampledPosition*(-1)));
        spc = pow(spc, spp);
        double spcc = spc*((i == 2 && ltr)?0.0000001:0.5);
        Vec coeff = color*(dif*0.8f+amb);
        if (hitType!=HIT_LETTER_CONT)coeff=coeff+Vec((float)spcc*0.5f);
        color = color+coeff*(lc*latt);
      }

      float incidence = normal % lightDirection;

      if (hitType == HIT_WALL && incidence > 0 &&
          RayMarching(sampledPosition + normal * .1,
                      lightDirection,
                      sampledPosition,
                      normal) == HIT_SUN)
        color = color + Vec(0.2,0.2,0.2)* Vec(500, 400, 100) * incidence;

      if (hitType!=HIT_LETTER_CONT) return color;
      else {
        float p = 6.283185 * 0.5f;
        float c = 0.5;
        float s = sqrtf(1 - c);
        float g = normal.z < 0 ? -1 : 1;
        float u = -1 / (g + normal.z);
        float v = normal.x * normal.y * u;
        direction = Vec(v,
                        g + normal.y * normal.y * u,
                        -normal.y) * (cosf(p) * s)
                             +
                             Vec(1 + g * normal.x * normal.x * u,
                                 g * v,
                                 -g * normal.x) * (sinf(p) * s) + normal * sqrtf(c);
        origin = sampledPosition + direction * .1;
        attenuation=attenuation*0.02;
      }
    } else if (hitType == HIT_AMIGARED) {
      return Vec(80, 0, 0) * attenuation + color;
    }
    else {//if (hitType == HIT_SUN) { //
      return Vec(50, 80, 100) * attenuation + color; // Sun Color
    }
#endif
  }
  return color;
}

#ifdef USE_FRAMEBUFFER

uint32_t* fb;
uint32_t fb_stride = 640;

void init(int w, int h) {
  // ???
}
inline void put(int x, int y, int r, int g, int b) {
  //printf("put %d/%d\n",x,y);
  fb[y*fb_stride+x] = (r<<16)|(g<<8)|b;
}
#else
void init(int w, int h) {
  printf("P6 %d %d 255 ", w, h);
}
void put(int x, int y, int r, int g, int b) {
  printf("%c%c%c", r, g, b);
}
#endif

void trace_main(uint32_t* fb_, uint32_t stride_) {
#ifdef USE_FRAMEBUFFER
  fb = fb_;
  fb_stride = stride_;
#endif
  
  int w = CANVAS_WIDTH, h = CANVAS_HEIGHT, samplesCount = PATHTRACE_SAMPLE_COUNT;
  Vec position(CAMERA_POS_X, CAMERA_POS_Y, CAMERA_POS_Z);// ray origin
  Vec goal = !(Vec(CAMERA_LOOKAT_X, CAMERA_LOOKAT_Y, CAMERA_LOOKAT_Z)
               + position * -1);// ray target
  Vec left = !Vec(goal.z, 0, -goal.x) * (1. / w);// camera left vector (use this to rotate the camera)

  // Cross-product to get the up vector
  Vec up(goal.y * left.z - goal.z * left.y,
         goal.z * left.x - goal.x * left.z,
         goal.x * left.y - goal.y * left.x);
  init(w, h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int xx = w - x, yy = h - y; // dunno if this is needed for the fb backend
      Vec color;
      for (int p = samplesCount; p--;)
        color = color + Trace(position, !(goal + left * (xx - w / 2
#ifdef PATHTRACE
                                                         + randomVal()
#endif
                                                         )+
                                          up * (yy - h / 2
#ifdef PATHTRACE
                                                + randomVal()
#endif
                                                )));

      // Reinhard tone mapping
      color = color * (1. / samplesCount) + 14. / 241;
      Vec o = color + 1;
      color = Vec(color.x / o.x, color.y / o.y, color.z / o.z) * 255;
      put(x, y, (int)color.x, (int)color.y, (int)color.z);
    }
}

#ifndef USE_FRAMEBUFFER
int main() {
  trace_main(NULL);
  return 0;
}
#endif

