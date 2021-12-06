#include "texture.h"
#include "color.h"

#include <assert.h>
#include <iostream>
#include <algorithm>

using namespace std;

namespace CMU462 {

inline void uint8_to_float(float dst[4], unsigned char *src) {
  uint8_t *src_uint8 = (uint8_t *)src;
  dst[0] = src_uint8[0] / 255.f;
  dst[1] = src_uint8[1] / 255.f;
  dst[2] = src_uint8[2] / 255.f;
  dst[3] = src_uint8[3] / 255.f;
}

inline Color uint8_to_color(unsigned char *src) {
  float cr[4];
  uint8_to_float(cr, src);
  return Color(cr[0], cr[1], cr[2], cr[3]);
}

inline void float_to_uint8(unsigned char *dst, float src[4]) {
  uint8_t *dst_uint8 = (uint8_t *)dst;
  dst_uint8[0] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[0])));
  dst_uint8[1] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[1])));
  dst_uint8[2] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[2])));
  dst_uint8[3] = (uint8_t)(255.f * max(0.0f, min(1.0f, src[3])));
}

void Sampler2DImp::generate_mips(Texture &tex, int startLevel) {

  // NOTE:
  // This starter code allocates the mip levels and generates a level
  // map by filling each level with placeholder data in the form of a
  // color that differs from its neighbours'. You should instead fill
  // with the correct data!

  // Task 7: Implement this

  // check start level
  if (startLevel >= tex.mipmap.size()) {
    std::cerr << "Invalid start level";
  }

  // allocate sublevels
  int baseWidth = tex.mipmap[startLevel].width;
  int baseHeight = tex.mipmap[startLevel].height;
  int numSubLevels = (int)(log2f((float)max(baseWidth, baseHeight)));

  numSubLevels = min(numSubLevels, kMaxMipLevels - startLevel - 1);
  tex.mipmap.resize(startLevel + numSubLevels + 1);

  int width = baseWidth;
  int height = baseHeight;
  for (int i = 1; i <= numSubLevels; i++) {

    MipLevel &level = tex.mipmap[startLevel + i];

    // handle odd size texture by rounding down
    width = max(1, width / 2);
    assert(width > 0);
    height = max(1, height / 2);
    assert(height > 0);

    level.width = width;
    level.height = height;
    level.texels = vector<unsigned char>(4 * width * height);
  }

  // fill all 0 sub levels with interchanging colors (JUST AS A PLACEHOLDER)
  for (size_t i = 1; i < tex.mipmap.size(); ++i) {
    MipLevel &mip = tex.mipmap[i];
    MipLevel &up_mip = tex.mipmap[i - 1];

    for (size_t y = 0; y < mip.height; ++y) {
      for (size_t x = 0; x < mip.width; ++x) {
        auto cr0 = uint8_to_color(up_mip.texels.data() +
                                  4 * (2 * x + up_mip.width * 2 * y));
        auto cr1 = uint8_to_color(up_mip.texels.data() +
                                  4 * ((2 * x + 1) + up_mip.width * 2 * y));
        auto cr2 = uint8_to_color(up_mip.texels.data() +
                                  4 * (2 * x + up_mip.width * (2 * y + 1)));
        auto cr3 =
            uint8_to_color(up_mip.texels.data() +
                           4 * ((2 * x + 1) + up_mip.width * (2 * y + 1)));

        Color res = cr0 * 0.25f + cr1 * 0.25f + cr2 * 0.25f + cr3 * 0.25f;

        mip.texels[4 * (x + mip.width * y)] = res.r * 255;
        mip.texels[4 * (x + mip.width * y) + 1] = res.g * 255;
        mip.texels[4 * (x + mip.width * y) + 2] = res.b * 255;
        mip.texels[4 * (x + mip.width * y) + 3] = res.a * 255;
      }
    }
  }
}

Color Sampler2DImp::sample_nearest(Texture &tex, float u, float v, int level) {

  // Task 6: Implement nearest neighbour interpolation

  // return magenta for invalid level
  auto &mipmap = tex.mipmap[level];

  assert(u >= 0 && u <= 1);
  assert(v >= 0 && v <= 1);

  int su = (int)floor(u * mipmap.width);
  int sv = (int)floor(v * mipmap.height);

  return Color(mipmap.texels.data() + 4 * (su + mipmap.width * sv));
}

Color Sampler2DImp::sample_bilinear(Texture &tex, float u, float v, int level) {

  // Task 6: Implement bilinear filtering

  // return magenta for invalid level
  auto &mipmap = tex.mipmap[level];

  assert(u >= 0 && u <= 1);
  assert(v >= 0 && v <= 1);

  float x = u * mipmap.width;
  float y = v * mipmap.height;

  int x00 = (int)floor(x - .5f);
  int y00 = (int)floor(y - .5f);

  int min_x = max(0, x00);
  int min_y = max(0, y00);
  int max_x = min((int)mipmap.width - 1, x00 + 1);
  int max_y = min((int)mipmap.height - 1, y00 + 1);

  float s = x - (x00 + .5f);
  float t = y - (y00 + .5f);
  Color c00 =
      uint8_to_color(mipmap.texels.data() + 4 * (min_x + mipmap.width * min_y));
  Color c01 =
      uint8_to_color(mipmap.texels.data() + 4 * (min_x + mipmap.width * max_y));
  Color c10 =
      uint8_to_color(mipmap.texels.data() + 4 * (max_x + mipmap.width * min_y));
  Color c11 =
      uint8_to_color(mipmap.texels.data() + 4 * (max_x + mipmap.width * max_y));

  return (1.f - t) * ((1.f - s) * c00 + s * c10) +
         t * ((1.f - s) * c01 + s * c11);
}

Color Sampler2DImp::sample_trilinear(Texture &tex, float u, float v,
                                     float u_scale, float v_scale) {

  // Task 7: Implement trilinear filtering

  // return magenta for invalid level

  float levelx = log2f(tex.width) - log2f(u_scale);
  float levely = log2f(tex.height) - log2f(v_scale);
  float level = max(levelx, levely);

  if (level < 0) {
    return sample_bilinear(tex, u, v);
  }

  int level0 = min((int)floor(level), (int)tex.mipmap.size() - 1);
  int level1 = min((int)ceil(level), (int)tex.mipmap.size() - 1);

  float w = level1 - level;

  Color cr0 = sample_bilinear(tex, u, v, level0);
  Color cr1 = sample_bilinear(tex, u, v, level1);

  return w * cr0 + (1 - w) * cr1;
}

} // namespace CMU462
