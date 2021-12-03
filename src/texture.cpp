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
  Color colors[3] = {Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1)};
  for (size_t i = 1; i < tex.mipmap.size(); ++i) {

    Color c = colors[i % 3];
    MipLevel &mip = tex.mipmap[i];

    for (size_t i = 0; i < 4 * mip.width * mip.height; i += 4) {
      float_to_uint8(&mip.texels[i], &c.r);
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

  int su00 = max(0, (int)floor(x - .5f));
  int sv00 = max(0, (int)floor(y - .5f));
  float s = x - (su00 + .5f);
  float t = y - (sv00 + .5f);
  Color c00 = Color(mipmap.texels.data() + 4 * (su00 + mipmap.width * sv00));

  int su01 = max(0, (int)floor(x - .5f));
  int sv01 = min((int)mipmap.height - 1, (int)floor(y + .5f));
  Color c01 = Color(mipmap.texels.data() + 4 * (su01 + mipmap.width * sv01));

  int su10 = min((int)mipmap.height - 1, (int)floor(x + .5f));
  int sv10 = max(0, (int)floor(y - .5f));
  Color c10 = Color(mipmap.texels.data() + 4 * (su10 + mipmap.width * sv10));

  int su11 = min((int)mipmap.height - 1, (int)floor(x + .5f));
  int sv11 = min((int)mipmap.height - 1, (int)floor(y + .5f));
  Color c11 = Color(mipmap.texels.data() + 4 * (su11 + mipmap.width * sv11));

  return (1.f - t) * ((1.f - s) * c00 + s * c10) +
         t * ((1.f - s) * c01 + s * c11);
}

Color Sampler2DImp::sample_trilinear(Texture &tex, float u, float v,
                                     float u_scale, float v_scale) {

  // Task 7: Implement trilinear filtering

  // return magenta for invalid level
  return Color(1, 0, 1, 1);
}

} // namespace CMU462
