#include "texture.h"
#include "CGL/color.h"

#include <cmath>
#include <algorithm>

namespace CGL {

Color Texture::sample(const SampleParams &sp) {
  // Parts 5 and 6: Fill this in.
  // Should return a color sampled based on the psm and lsm parameters given
  if (sp.psm == P_NEAREST){
    return sample_nearest(sp.p_uv, get_level(sp));
  } else if (sp.psm == P_LINEAR){
    //Defaults to bilinear if level is integer
    return sample_trilinear(sp.p_uv, get_level(sp));
  } else return Color(1, 0, 0);
}

float Texture::get_level(const SampleParams &sp) {
  if (sp.lsm == L_ZERO){
    //Use original texture
    return 0;
  } else {
    Vector2D dx = width*(sp.p_dx_uv - sp.p_uv);

    Vector2D dy = height*(sp.p_dy_uv - sp.p_uv);

    float level = log2(max(sqrt(pow(dx[0] , 2)+pow(dx[1] , 2)), sqrt(pow(dy[0] , 2)+pow(dy[1], 2))));
    float clampedlevel = (float)clamp(level, 0.f, (float)mipmap.max_size());

    if (sp.lsm == L_NEAREST){
      //Find nearest mipmap level

      int rounded = (int)floorf(clampedlevel);
      return ((clampedlevel - rounded) < 0.5) ? clampedlevel: (clampedlevel+1);
    } else {
      //Return float value of mipmap level for linear interpolation between mipmap values
      return clampedlevel;
    } 
  }
}

// Returns the nearest sample given a particular level and set of uv coords
Color Texture::sample_nearest(Vector2D uv, float level) {
  // Optional helper function for Parts 5 and 6
  // Feel free to ignore or create your own

  int intlevel = (int)floorf(level);
  int mipmapOffset = level - intlevel;

  int u00x = clamp((int)floorf((mipmap[intlevel].width-1)*uv.x), 0, (int)mipmap[intlevel].width-1);
  int u00y = clamp((int)floorf((mipmap[intlevel].height-1)*uv.y), 0, (int)mipmap[intlevel].height-1);

  float offsetS = (mipmap[intlevel].width-1)*uv.x-u00x;
  float offsetT = (mipmap[intlevel].height-1)*uv.y-u00y;
  
  int texelX = (offsetS < 0.5f)? u00x : (u00x+1);
  int texelY = (offsetT < 0.5f)? u00y : (u00y+1);

  
  Color texCol = mipmap[intlevel].get_texel( texelX , texelY ) ;
  return texCol;
}

// Returns the bilinear sample given a particular level and set of uv coords
Color Texture::sample_bilinear(Vector2D uv, float level) {
  // Optional helper function for Parts 5 and 6
  // Feel free to ignore or create your own



  int intlevel = (int)floorf(level);
  int mipmapOffset = level - intlevel;

  int currentMapWidth = mipmap[intlevel].width;
  int currentMapHeight = mipmap[intlevel].height;

  int u00x = (int)floorf(currentMapWidth*uv.x);
  int u00y = (int)floorf(currentMapHeight*uv.y);

  if (u00x < 0){
    u00x = 0;
  }

  if (u00y < 0){
    u00y = 0;
  }

  if (u00x > currentMapWidth){
    u00x = currentMapWidth;
  }

  if (u00y > currentMapHeight){
    u00y = currentMapHeight;
  }

  

  float offsetS = currentMapWidth*uv.x-u00x;
  float offsetT = currentMapHeight*uv.y-u00y;

  // cout << u00x << " and " << u00y << "map width " << currentMapWidth << " height: " << currentMapHeight << endl;
  Color u00 = mipmap[intlevel].get_texel(u00x, u00y);

  int yclamp01 = clamp(u00y+1, 0, currentMapHeight-1);
  Color u01 = mipmap[intlevel].get_texel(u00x, yclamp01);

  int xclamp10 = clamp(u00x+1, 0, currentMapWidth-1);
  Color u10 = mipmap[intlevel].get_texel(xclamp10 , u00y);

  int xclamp11 = clamp(u00x+1, 0, currentMapWidth-1);
  int yclamp11 = clamp(u00y+1, 0, currentMapHeight-1);
  Color u11 = mipmap[intlevel].get_texel( xclamp11 , yclamp11);
  
  Color interpol = lerp(offsetT, lerp(offsetS, u00, u01), lerp(offsetS, u10, u11));

  return interpol;
}

Color Texture::sample_trilinear(Vector2D uv, float level){

  int intlevel = (int)floorf(level);
  int mipmapOffset = level - intlevel;

  Color interpol1 = sample_bilinear(uv, intlevel);

  if (mipmapOffset < 0.005f){
    return interpol1;
  } else {
    return lerp(mipmapOffset, interpol1, sample_bilinear(uv, intlevel +1));
  }
}

Color Texture::lerp(float val, Color start, Color end){

  float r = start.r + val*(end.r-end.r);
  float g = start.g + val*(end.g-end.g);
  float b = start.b + val*(end.b-end.b);

  return Color(r, g, b);
}



/****************************************************************************/



inline void uint8_to_float(float dst[3], unsigned char *src) {
  uint8_t *src_uint8 = (uint8_t *)src;
  dst[0] = src_uint8[0] / 255.f;
  dst[1] = src_uint8[1] / 255.f;
  dst[2] = src_uint8[2] / 255.f;
}

inline void float_to_uint8(unsigned char *dst, float src[3]) {
  uint8_t *dst_uint8 = (uint8_t *)dst;
  dst_uint8[0] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[0])));
  dst_uint8[1] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[1])));
  dst_uint8[2] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[2])));
}

void Texture::generate_mips(int startLevel) {

  // make sure there's a valid texture
  if (startLevel >= mipmap.size()) {
    std::cerr << "Invalid start level";
  }

  // allocate sublevels
  int baseWidth = mipmap[startLevel].width;
  int baseHeight = mipmap[startLevel].height;
  int numSubLevels = (int)(log2f((float)max(baseWidth, baseHeight)));

  numSubLevels = min(numSubLevels, kMaxMipLevels - startLevel - 1);
  mipmap.resize(startLevel + numSubLevels + 1);

  int width = baseWidth;
  int height = baseHeight;
  for (int i = 1; i <= numSubLevels; i++) {

    MipLevel &level = mipmap[startLevel + i];

    // handle odd size texture by rounding down
    width = max(1, width / 2);
    //assert (width > 0);
    height = max(1, height / 2);
    //assert (height > 0);

    level.width = width;
    level.height = height;
    level.texels = vector<unsigned char>(3 * width * height);
  }

  // create mips
  int subLevels = numSubLevels - (startLevel + 1);
  for (int mipLevel = startLevel + 1; mipLevel < startLevel + subLevels + 1;
       mipLevel++) {

    MipLevel &prevLevel = mipmap[mipLevel - 1];
    MipLevel &currLevel = mipmap[mipLevel];

    int prevLevelPitch = prevLevel.width * 3; // 32 bit RGB
    int currLevelPitch = currLevel.width * 3; // 32 bit RGB

    unsigned char *prevLevelMem;
    unsigned char *currLevelMem;

    currLevelMem = (unsigned char *)&currLevel.texels[0];
    prevLevelMem = (unsigned char *)&prevLevel.texels[0];

    float wDecimal, wNorm, wWeight[3];
    int wSupport;
    float hDecimal, hNorm, hWeight[3];
    int hSupport;

    float result[3];
    float input[3];

    // conditional differentiates no rounding case from round down case
    if (prevLevel.width & 1) {
      wSupport = 3;
      wDecimal = 1.0f / (float)currLevel.width;
    } else {
      wSupport = 2;
      wDecimal = 0.0f;
    }

    // conditional differentiates no rounding case from round down case
    if (prevLevel.height & 1) {
      hSupport = 3;
      hDecimal = 1.0f / (float)currLevel.height;
    } else {
      hSupport = 2;
      hDecimal = 0.0f;
    }

    wNorm = 1.0f / (2.0f + wDecimal);
    hNorm = 1.0f / (2.0f + hDecimal);

    // case 1: reduction only in horizontal size (vertical size is 1)
    if (currLevel.height == prevLevel.height) {
      //assert (currLevel.height == 1);

      for (int i = 0; i < currLevel.width; i++) {
        wWeight[0] = wNorm * (1.0f - wDecimal * i);
        wWeight[1] = wNorm * 1.0f;
        wWeight[2] = wNorm * wDecimal * (i + 1);

        result[0] = result[1] = result[2] = 0.0f;

        for (int ii = 0; ii < wSupport; ii++) {
          uint8_to_float(input, prevLevelMem + 3 * (2 * i + ii));
          result[0] += wWeight[ii] * input[0];
          result[1] += wWeight[ii] * input[1];
          result[2] += wWeight[ii] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (3 * i), result);
      }

      // case 2: reduction only in vertical size (horizontal size is 1)
    } else if (currLevel.width == prevLevel.width) {
      //assert (currLevel.width == 1);

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        result[0] = result[1] = result[2] = 0.0f;
        for (int jj = 0; jj < hSupport; jj++) {
          uint8_to_float(input, prevLevelMem + prevLevelPitch * (2 * j + jj));
          result[0] += hWeight[jj] * input[0];
          result[1] += hWeight[jj] * input[1];
          result[2] += hWeight[jj] * input[2];
        }

        // convert back to format of the texture
        float_to_uint8(currLevelMem + (currLevelPitch * j), result);
      }

      // case 3: reduction in both horizontal and vertical size
    } else {

      for (int j = 0; j < currLevel.height; j++) {
        hWeight[0] = hNorm * (1.0f - hDecimal * j);
        hWeight[1] = hNorm;
        hWeight[2] = hNorm * hDecimal * (j + 1);

        for (int i = 0; i < currLevel.width; i++) {
          wWeight[0] = wNorm * (1.0f - wDecimal * i);
          wWeight[1] = wNorm * 1.0f;
          wWeight[2] = wNorm * wDecimal * (i + 1);

          result[0] = result[1] = result[2] = 0.0f;

          // convolve source image with a trapezoidal filter.
          // in the case of no rounding this is just a box filter of width 2.
          // in the general case, the support region is 3x3.
          for (int jj = 0; jj < hSupport; jj++)
            for (int ii = 0; ii < wSupport; ii++) {
              float weight = hWeight[jj] * wWeight[ii];
              uint8_to_float(input, prevLevelMem +
                                        prevLevelPitch * (2 * j + jj) +
                                        3 * (2 * i + ii));
              result[0] += weight * input[0];
              result[1] += weight * input[1];
              result[2] += weight * input[2];
            }

          // convert back to format of the texture
          float_to_uint8(currLevelMem + currLevelPitch * j + 3 * i, result);
        }
      }
    }
  }
}

}
