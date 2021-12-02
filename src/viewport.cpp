#include "viewport.h"

#include "CMU462.h"

namespace CMU462 {

void ViewportImp::set_viewbox(float centerX, float centerY, float vspan) {

  // Task 5 (part 2):
  // Set svg coordinate to normalized device coordinate transformation. Your input
  // arguments are defined as normalized SVG canvas coordinates.
  double scale[] = {.5 / vspan, 0,          .5 - centerX * .5 / vspan,
                    0,          .5 / vspan, .5 - centerY * .5 / vspan,
                    0,          0,          1};

  this->centerX = centerX;
  this->centerY = centerY;
  this->vspan = vspan;

  set_svg_2_norm(Matrix3x3(scale));
}

void ViewportImp::update_viewbox(float dx, float dy, float scale) {

  this->centerX -= dx;
  this->centerY -= dy;
  this->vspan *= scale;
  set_viewbox(centerX, centerY, vspan);
}

} // namespace CMU462
