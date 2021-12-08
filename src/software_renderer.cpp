#include "software_renderer.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>
#include <algorithm>
#include <array>

#include "triangulation.h"

using namespace std;

namespace CMU462 {

// check if point (x,y) is inside the triangle
bool inside_triangle(double x, double y, double x0, double y0, double x1,
                     double y1, double x2, double y2) {
  float a = (y1 - y0) * (x - x0) - (x1 - x0) * (y - y0);
  float b = (y2 - y1) * (x - x1) - (x2 - x1) * (y - y1);
  if (a * b < 0)
    return false;
  float c = (y0 - y2) * (x - x2) - (x0 - x2) * (y - y2);
  if (a * c < 0)
    return false;

  return true;
}

// Implements SoftwareRenderer //

void SoftwareRendererImp::draw_svg(SVG &svg) {
  memset(this->supersample_target.data(), 255, this->supersample_target.size());

  // set top level transformation
  transformation = svg_2_screen;

  // draw all elements
  for (size_t i = 0; i < svg.elements.size(); ++i) {
    draw_element(svg.elements[i]);
  }

  // draw canvas outline
  Vector2D a = transform(Vector2D(0, 0));
  a.x--;
  a.y--;
  Vector2D b = transform(Vector2D(svg.width, 0));
  b.x++;
  b.y--;
  Vector2D c = transform(Vector2D(0, svg.height));
  c.x--;
  c.y++;
  Vector2D d = transform(Vector2D(svg.width, svg.height));
  d.x++;
  d.y++;

  rasterize_line(a.x, a.y, b.x, b.y, Color::Black);
  rasterize_line(a.x, a.y, c.x, c.y, Color::Black);
  rasterize_line(d.x, d.y, b.x, b.y, Color::Black);
  rasterize_line(d.x, d.y, c.x, c.y, Color::Black);

  // resolve and send to render target
  resolve();
  if (enable_mlaa)
    mlaa();
}

void SoftwareRendererImp::set_sample_rate(size_t sample_rate) {

  // Task 4:
  // You may want to modify this for supersampling support
  this->sample_rate = sample_rate;

  this->supersample_target.resize(this->target_w * this->target_h *
                                  sample_rate * sample_rate * 4);
}

void SoftwareRendererImp::set_render_target(unsigned char *render_target,
                                            size_t width, size_t height) {

  // Task 4:
  // You may want to modify this for supersampling support
  this->render_target = render_target;
  this->target_w = width;
  this->target_h = height;

  this->supersample_target.resize(this->target_w * this->target_h *
                                  sample_rate * sample_rate * 4);
}

void SoftwareRendererImp::draw_element(SVGElement *element) {

  // Task 5 (part 1):
  // Modify this to implement the transformation stack

  auto saved_trans = transformation;
  transformation = transformation * element->transform;

  switch (element->type) {
  case POINT:
    draw_point(static_cast<Point &>(*element));
    break;
  case LINE:
    draw_line(static_cast<Line &>(*element));
    break;
  case POLYLINE:
    draw_polyline(static_cast<Polyline &>(*element));
    break;
  case RECT:
    draw_rect(static_cast<Rect &>(*element));
    break;
  case POLYGON:
    draw_polygon(static_cast<Polygon &>(*element));
    break;
  case ELLIPSE:
    draw_ellipse(static_cast<Ellipse &>(*element));
    break;
  case IMAGE:
    draw_image(static_cast<Image &>(*element));
    break;
  case GROUP:
    draw_group(static_cast<Group &>(*element));
    break;
  default:
    break;
  }

  transformation = saved_trans;
}

// Primitive Drawing //

void SoftwareRendererImp::draw_point(Point &point) {

  Vector2D p = transform(point.position);
  rasterize_point(p.x, p.y, point.style.fillColor);
}

void SoftwareRendererImp::draw_line(Line &line) {

  Vector2D p0 = transform(line.from);
  Vector2D p1 = transform(line.to);
  rasterize_line(p0.x, p0.y, p1.x, p1.y, line.style.strokeColor,
                 line.style.strokeWidth);
}

void SoftwareRendererImp::draw_polyline(Polyline &polyline) {

  Color c = polyline.style.strokeColor;

  if (c.a != 0) {
    int nPoints = polyline.points.size();
    for (int i = 0; i < nPoints - 1; i++) {
      Vector2D p0 = transform(polyline.points[(i + 0) % nPoints]);
      Vector2D p1 = transform(polyline.points[(i + 1) % nPoints]);
      rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
    }
  }
}

void SoftwareRendererImp::draw_rect(Rect &rect) {

  Color c;

  // draw as two triangles
  float x = rect.position.x;
  float y = rect.position.y;
  float w = rect.dimension.x;
  float h = rect.dimension.y;

  Vector2D p0 = transform(Vector2D(x, y));
  Vector2D p1 = transform(Vector2D(x + w, y));
  Vector2D p2 = transform(Vector2D(x, y + h));
  Vector2D p3 = transform(Vector2D(x + w, y + h));

  // draw fill
  c = rect.style.fillColor;
  if (c.a != 0) {
    rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
    rasterize_triangle(p2.x, p2.y, p1.x, p1.y, p3.x, p3.y, c);
  }

  // draw outline
  c = rect.style.strokeColor;
  if (c.a != 0) {
    rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
    rasterize_line(p1.x, p1.y, p3.x, p3.y, c);
    rasterize_line(p3.x, p3.y, p2.x, p2.y, c);
    rasterize_line(p2.x, p2.y, p0.x, p0.y, c);
  }
}

void SoftwareRendererImp::draw_polygon(Polygon &polygon) {

  Color c;

  // draw fill
  c = polygon.style.fillColor;
  if (c.a != 0) {

    // triangulate
    vector<Vector2D> triangles;
    triangulate(polygon, triangles);

    // draw as triangles
    for (size_t i = 0; i < triangles.size(); i += 3) {
      Vector2D p0 = transform(triangles[i + 0]);
      Vector2D p1 = transform(triangles[i + 1]);
      Vector2D p2 = transform(triangles[i + 2]);
      rasterize_triangle(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c);
    }
  }

  // draw outline
  c = polygon.style.strokeColor;
  if (c.a != 0) {
    int nPoints = polygon.points.size();
    for (int i = 0; i < nPoints; i++) {
      Vector2D p0 = transform(polygon.points[(i + 0) % nPoints]);
      Vector2D p1 = transform(polygon.points[(i + 1) % nPoints]);
      rasterize_line(p0.x, p0.y, p1.x, p1.y, c);
    }
  }
}

void SoftwareRendererImp::draw_ellipse(Ellipse &ellipse) {
  // (x - h)^2 / a^2 + (y - k)^2 / b^2 = 1

  // Extra credit
  auto center = transform(ellipse.center);
  auto radius =
      transformation * Vector3D(ellipse.radius.x, ellipse.radius.y, 0);

  int l = center.x - radius.x - 1;
  int r = ceil(center.x + radius.x + 1);
  int b = center.y - radius.y - 1;
  int t = ceil(center.y + radius.y + 1);

  auto f = [&center, &radius](float x, float y) {
    float offsetx = x - center.x;
    float offsety = y - center.y;

    return offsetx * offsetx / radius.x / radius.x +
           offsety * offsety / radius.y / radius.y - 1;
  };

  auto get_x_boundary = [&center, &radius](float y, float res[2]) {
    float offsety = y - center.y;
    float root = sqrt((1 - offsety * offsety / radius.y / radius.y) * radius.x *
                      radius.x);
    res[0] = -root + center.x;
    res[1] = root + center.x;
  };

  auto get_y_boundary = [&center, &radius](float x, float res[2]) {
    float offsetx = x - center.x;
    float root = sqrt((1 - offsetx * offsetx / radius.x / radius.x) * radius.y *
                      radius.y);
    res[0] = -root + center.y;
    res[1] = root + center.y;
  };

  if (ellipse.style.fillColor.a != 0) {
    for (int y = b; y <= t; ++y) {
      for (int x = l; x <= r; ++x) {
        float val = f(x, y);
        if (val <= 0)
          rasterize_point(x, y, ellipse.style.fillColor);
      }
    }
  }

  if (ellipse.style.strokeColor.a != 0) {
    for (int y = b; y <= t; ++y) {
      float x[2];
      get_x_boundary(y, x);
      for (int i = 0; i < ellipse.style.strokeWidth; ++i) {
        rasterize_point(x[0] + i, y, ellipse.style.strokeColor);
        rasterize_point(x[1] - i, y, ellipse.style.strokeColor);
      }
    }
    for (int x = l; x <= r; ++x) {
      float y[2];
      get_y_boundary(x, y);
      for (int i = 0; i < ellipse.style.strokeWidth; ++i) {
        rasterize_point(x, y[0] + i, ellipse.style.strokeColor);
        rasterize_point(x, y[1] - i, ellipse.style.strokeColor);
      }
    }
  }
}

void SoftwareRendererImp::draw_image(Image &image) {

  Vector2D p0 = transform(image.position);
  Vector2D p1 = transform(image.position + image.dimension);

  rasterize_image(p0.x, p0.y, p1.x, p1.y, image.tex);
}

void SoftwareRendererImp::draw_group(Group &group) {

  for (size_t i = 0; i < group.elements.size(); ++i) {
    draw_element(group.elements[i]);
  }
}

// Rasterization //

// The input arguments in the rasterization functions
// below are all defined in screen space coordinates

void SoftwareRendererImp::rasterize_point(float x, float y, Color color) {

  // fill in the nearest pixel
  int sx = (int)floor(x);
  int sy = (int)floor(y);

  // check bounds
  if (sx < 0 || sx >= target_w)
    return;
  if (sy < 0 || sy >= target_h)
    return;

  // fill sample - NOT doing alpha blending!
  for (int j = 0; j < sample_rate; ++j) {
    for (int i = 0; i < sample_rate; ++i) {
      set_color_in_supersample_target(sx * sample_rate + i,
                                      sy * sample_rate + j, color);
    }
  }
}

void SoftwareRendererImp::rasterize_line(float x0, float y0, float x1, float y1,
                                         Color color, float width) {

  // Task 2:
  // Implement line rasterization
  width *= sample_rate;

  int sample_w = (target_w - 1) * sample_rate + 1,
      sample_h = (target_h - 1) * sample_rate + 1;

  int delta = 1;

  if (abs(y1 - y0) <= abs(x1 - x0)) {
    // 0 <= |k| <= 1
    if (x0 > x1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;

    if (dy < 0) {
      delta = -1;
      dy = -dy;
    }

    int sx0 = (int)floor(x0 * sample_rate);
    int sx1 = (int)floor(x1 * sample_rate);

    int y = (int)floor(y0 * sample_rate);
    if ((delta > 0 && y >= sample_h) || (delta < 0 && y < 0))
      return;
    float D = 2 * dy - dx;

    for (int x = sx0; x <= sx1; ++x) {
      if (x >= 0 && x < sample_w) {
        for (int w = -(width - 1) / 2; w <= width / 2; ++w) {
          int w_y = y + w;
          if (w_y >= 0 && w_y < sample_h) {
            for (int i = 0; i < sample_rate; ++i) {
              set_color_in_supersample_target(x, w_y + i, color);
            }
          }
        }
      }

      if (D >= 0) {
        y += delta;
        if ((delta > 0 && y >= sample_h) || (delta < 0 && y < 0))
          break;
        D += 2 * (dy - dx);
      } else {
        D += 2 * dy;
      }
    }
  } else {
    // |k| > 1, or vert line
    if (y0 > y1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;

    if (dx < 0) {
      delta = -1;
      dx = -dx;
    }

    int sy0 = (int)floor(y0 * sample_rate);
    int sy1 = (int)floor(y1 * sample_rate);

    int x = (int)floor(x0 * sample_rate);
    if ((delta > 0 && x >= sample_w) || (delta < 0 && x < 0))
      return;
    int D = 2 * dx - dy;

    for (int y = sy0; y <= sy1; ++y) {
      if (y >= 0 && y < sample_h) {
        for (int w = -(width - 1) / 2; w <= width / 2; ++w) {
          int w_x = x + w;
          if (w_x >= 0 && w_x < sample_w) {
            for (int i = 0; i < sample_rate; ++i) {
              set_color_in_supersample_target(w_x + i, y, color);
            }
          }
        }
      }

      if (D >= 0) {
        x += delta;
        if ((delta > 0 && x >= sample_w) || (delta < 0 && x < 0))
          break;
        D += 2 * (dx - dy);
      } else {
        D += 2 * dx;
      }
    }
  }
}

void SoftwareRendererImp::rasterize_triangle(float x0, float y0, float x1,
                                             float y1, float x2, float y2,
                                             Color color) {
  // Task 3:
  // Implement triangle rasterization
  float l = std::max(0.f, std::min({x0, x1, x2}));
  float r = std::min((float)target_w - 1, ceil(std::max({x0, x1, x2})));
  float b = std::max(0.f, std::min({y0, y1, y2}));
  float t = std::min((float)target_h - 1, ceil(std::max({y0, y1, y2})));

  for (int x = l * sample_rate; x <= ceil(r * sample_rate); ++x) {
    for (int y = b * sample_rate; y <= ceil(t * sample_rate); ++y) {
      if (inside_triangle(x + .5f, y + .5f, x0 * sample_rate, y0 * sample_rate,
                          x1 * sample_rate, y1 * sample_rate, x2 * sample_rate,
                          y2 * sample_rate)) {
        set_color_in_supersample_target(x, y, color);
      }
    }
  }
}

void SoftwareRendererImp::rasterize_image(float x0, float y0, float x1,
                                          float y1, Texture &tex) {
  // Task 6:
  // Implement image rasterization

  int sx0 = floor((x0)*sample_rate);
  int sy0 = floor((y0)*sample_rate);
  int sx1 = ceil((x1)*sample_rate);
  int sy1 = ceil((y1)*sample_rate);

  for (int y = std::max(0, sy0);
       y < std::min((int)(target_h * sample_rate), sy1); ++y) {
    for (int x = std::max(0, sx0);
         x < std::min((int)(target_w * sample_rate), sx1); ++x) {
      float u = (float)std::max(0, x - sx0) / (sx1 - sx0);
      float v = (float)std::max(0, y - sy0) / (sy1 - sy0);

      Color color = sampler->sample_trilinear(tex, u, v, sx1 - sx0, sy1 - sy0);
      //Color color = sampler->sample_bilinear(tex, u, v);

      set_color_in_supersample_target(x, y, color);
    }
  }
}

// resolve samples to render target
void SoftwareRendererImp::resolve(void) {

  // Task 4:
  // Implement supersampling
  // You may also need to modify other functions marked with "Task 4".
  size_t ss_w = target_w * sample_rate;
  unsigned char sample_rate_square = sample_rate * sample_rate;
  size_t end_sy = 0;

  for (int y = 0; y < target_h; ++y) {
    size_t end_sx = 0;
    end_sy += sample_rate;

    for (int x = 0; x < target_w; ++x) {
      end_sx += sample_rate;
      uint32_t r = 0, g = 0, b = 0, a = 0;

      for (size_t sy = end_sy - sample_rate; sy < end_sy; ++sy) {
        for (size_t sx = end_sx - sample_rate; sx < end_sx; ++sx) {
          size_t idx = (sx + sy * ss_w) << 2;
          r += supersample_target[idx];
          g += supersample_target[idx + 1];
          b += supersample_target[idx + 2];
          a += supersample_target[idx + 3];
        }
      }

      render_target[4 * (x + y * target_w)] = r / sample_rate_square;
      render_target[4 * (x + y * target_w) + 1] = g / sample_rate_square;
      render_target[4 * (x + y * target_w) + 2] = b / sample_rate_square;
      render_target[4 * (x + y * target_w) + 3] = a / sample_rate_square;
    }
  }
}

// Step 1. Edge detection
std::vector<unsigned char> SoftwareRendererImp::mlaa_detect_edge(float L) {
  auto get_luma = [&](int x, int y) {
    auto cr = render_target + 4 * (x + y * target_w);
    return (cr[0] * 0.2126f + cr[1] * 0.7152f + cr[2] * 0.0722f) *
           (cr[3] / 255.f);
  };

  // TODO: Local contract adaptation
  std::vector<unsigned char> edge_target(target_w * target_h, 0);

  for (int y = 0; y < target_h; ++y) {
    for (int x = 0; x < target_w; ++x) {
      auto luma_cur = get_luma(x, y);
      if (x > 0) {
        auto luma_left = get_luma(x - 1, y);

        if (abs(luma_cur - luma_left) > L) {
          edge_target[x + y * target_w] |= 0x1; // L: 0001
        }
      }

      if (y > 0) {
        auto luma_top = get_luma(x, y - 1);

        if (abs(luma_cur - luma_top) > L) {
          edge_target[x + y * target_w] |= 0x2; // T: 0010
        }
      }
    }
  }

  return edge_target;
}

struct MLAAArea {
  float me = 0.f;
  float opposite = 0.f;
};

static MLAAArea mlaa_calc_area(std::array<float, 2> p0, std::array<float, 2> p1,
                               float x0) {
  float dx = p1[0] - p0[0];
  float dy = p1[1] - p0[1];
  float x1 = x0 + 1;
  if (x0 >= p1[0] || x1 < p0[0]) {
    // outside
    return {0.f, 0.f};
  }

  float y0 = (p1[1] - p0[1]) / (p1[0] - p0[0]) * (x0 - p0[0]) + p0[1];
  float y1 = (p1[1] - p0[1]) / (p1[0] - p0[0]) * (x1 - p0[0]) + p0[1];

  if (copysign(1.f, y0) == copysign(1.f, y1) || abs(y0) < FLT_EPSILON ||
      abs(y1) < FLT_EPSILON) {
    // trapezoid
    float res = abs(y0 + y1) / 2.f;
    if (y0 + y1 > 0)
      return {res, 0.f};
    else
      return {0.f, res};
  }

  // triangle
  float midx = -p0[1] * (p1[0] - p0[0]) / (p1[1] - p0[1]) + p0[0];
  float area0 = abs(y0 * (midx - x0) / 2.f);
  float area1 = abs(y1 * (1.f - (x1 - midx)) / 2.f);
  if (y0 > 0) {
    return {area0, area1};
  } else {
    return {area1, area0};
  }
}

static MLAAArea mlaa_get_weights_for_pattern(int pattern, float dl, float dr) {
  float d = dl + dr + 1;

  // lb rb lt rt
  switch (pattern) {
    // 1 edge
  case 0b1000:
    if (dl > dr) {
      return {0.f, 0.f};
    }

    return mlaa_calc_area({0, .5f}, {d / 2.f, 0.f}, dl);
  case 0b0100:
    if (dl < dr) {
      return {0.f, 0.f};
    }

    return mlaa_calc_area({d / 2.f, 0.f}, {d, .5f}, dl);
  case 0b0010:
    if (dl > dr) {
      return {0.f, 0.f};
    }

    return mlaa_calc_area({0, -.5f}, {d / 2.f, 0.f}, dl);
  case 0b0001:
    if (dl < dr) {
      return {0.f, 0.f};
    }

    return mlaa_calc_area({d / 2.f, 0.f}, {d, -.5f}, dl);

    // 2 edges
  case 0b1100: {
    auto a0 = mlaa_calc_area({0, .5f}, {d / 2.f, 0.f}, dl);
    auto a1 = mlaa_calc_area({d / 2.f, 0.f}, {d, .5f}, dl);
    return {a0.me + a1.me, a0.opposite + a1.opposite};
  }
  case 0b0011: {
    auto a0 = mlaa_calc_area({0, -.5f}, {d / 2.f, 0.f}, dl);
    auto a1 = mlaa_calc_area({d / 2.f, 0.f}, {d, -.5f}, dl);
    return {a0.me + a1.me, a0.opposite + a1.opposite};
  }

  case 0b1001: {
    auto a0 = mlaa_calc_area({0, .5f}, {d / 2.f, 0.f}, dl);
    auto a1 = mlaa_calc_area({d / 2.f, 0.f}, {d, -.5f}, dl);
    auto a2 = mlaa_calc_area({0, .5f}, {d, -.5f}, dl);
    return {(a0.me + a1.me + a2.me) / 2.f,
            (a0.opposite + a1.opposite + a2.opposite) / 2.f};
  }

  break;
  case 0b0110: {
    auto a0 = mlaa_calc_area({d / 2.f, 0.f}, {d, .5f}, dl);
    auto a1 = mlaa_calc_area({0, -.5f}, {d / 2.f, 0.f}, dl);
    auto a2 = mlaa_calc_area({0, -.5f}, {d, .5f}, dl);
    return {(a0.me + a1.me + a2.me) / 2.f,
            (a0.opposite + a1.opposite + a2.opposite) / 2.f};
  }

    // 3 edges
  case 0b0111:
  case 0b1110:
    return mlaa_calc_area({0, -.5f}, {d, .5f}, dl);
  case 0b1011:
  case 0b1101:
    return mlaa_calc_area({0, .5f}, {d, -.5f}, dl);

    // no area
  case 0b0000:
  case 0b1010:
  case 0b0101:
  case 0b1111:
    return {0.f, 0.f};
  default:
    assert(false);
  }
}

void SoftwareRendererImp::set_mlaa(bool enable) { enable_mlaa = enable; }

void SoftwareRendererImp::mlaa(void) {
  // Extra credits: MLAA on render_target

  auto get_index = [&](int x, int y) { return 4 * (x + y * target_w); };

  std::vector<unsigned char> old_target(4 * target_w * target_h);
  memcpy(old_target.data(), render_target, old_target.size());

  auto edge_target = mlaa_detect_edge(0.05f);

  // Step 2. Pattern handling
  const int MAX_SEARCH_STEP = 20;

  for (int y = 0; y < target_h; ++y) {
    for (int x = 0; x < target_w; ++x) {
      auto index = get_index(x, y);
      auto cur_cr = render_target + index;
      auto old_cur_cr = old_target.data() + index;

      auto e = edge_target[x + y * target_w];

      if (e & 0x1) {
        // Left
        float du, dd;
        int i = 0;

        // Y up distance
        for (i = 1; i < MAX_SEARCH_STEP; ++i) {
          if (y - i < 0)
            break;
          if ((edge_target[x + (y - i) * target_w] & 0x1) == 0)
            break;
        }
        du = -(i - 1);

        // Y down distance
        for (i = 1; i < MAX_SEARCH_STEP; ++i) {
          if (y + i >= target_h)
            break;
          if ((edge_target[x + (y + i) * target_w] & 0x1) == 0)
            break;
        }
        dd = i - 1;

        int ur_edge = edge_target[x + (y + du) * target_w] & 0x2;
        int dr_edge = edge_target[x + (y + dd) * target_w] & 0x2;
        int ul_edge = edge_target[x - 1 + (y + du) * target_w] & 0x2;
        int dl_edge = edge_target[x - 1 + (y + dd) * target_w] & 0x2;

        int pattern =
            (dr_edge << 2) | (ur_edge << 1) | (dl_edge) | (ul_edge >> 1);
        auto w = mlaa_get_weights_for_pattern(pattern, dd, -du);
        assert(w.me + w.opposite <= 1);

        // Blend with left
        auto old_left_cr = old_target.data() + get_index(x - 1, y);
        auto left_cr = render_target + get_index(x - 1, y);
        for (int j = 0; j < 4; ++j) {
          cur_cr[j] = old_cur_cr[j] * (1.f - w.me) + old_left_cr[j] * w.me;
          left_cr[j] =
              old_left_cr[j] * (1.f - w.opposite) + old_cur_cr[j] * w.opposite;
        }
      }

      if (e & 0x2) {
        // Top
        float dl, dr;
        int i = 0;

        // X left distance
        for (i = 1; i < MAX_SEARCH_STEP; ++i) {
          if (x - i < 0)
            break;
          if ((edge_target[x - i + y * target_w] & 0x2) == 0)
            break;
        }
        dl = -(i - 1);

        // X right distance
        for (i = 1; i < MAX_SEARCH_STEP; ++i) {
          if (x + i >= target_w)
            break;
          if ((edge_target[x + i + y * target_w] & 0x2) == 0)
            break;
        }
        dr = i - 1;

        float d = dl + dr + 1;

        int lb_edge = edge_target[x + dl + y * target_w] & 0x1;
        int rb_edge = edge_target[x + dr + y * target_w] & 0x1;
        int lt_edge = edge_target[x + dl + (y - 1) * target_w] & 0x1;
        int rt_edge = edge_target[x + dr + (y - 1) * target_w] & 0x1;

        int pattern =
            (lb_edge << 3) | (rb_edge << 2) | (lt_edge << 1) | rt_edge;
        auto w = mlaa_get_weights_for_pattern(pattern, -dl, dr);
        assert(w.me + w.opposite <= 1);

        // Blend with top
        auto old_top_cr = old_target.data() + get_index(x, y - 1);
        auto top_cr = render_target + get_index(x, y - 1);
        for (int j = 0; j < 4; ++j) {
          cur_cr[j] = old_cur_cr[j] * (1.f - w.me) + old_top_cr[j] * w.me;
          top_cr[j] =
              old_top_cr[j] * (1.f - w.opposite) + old_cur_cr[j] * w.opposite;
        }
      }
    }
  }
}

void SoftwareRendererImp::set_color_in_supersample_target(int x, int y,
                                                          Color color) {
  size_t ss_w = target_w * sample_rate;
  auto &r = supersample_target[4 * (x + y * ss_w)];
  auto &g = supersample_target[4 * (x + y * ss_w) + 1];
  auto &b = supersample_target[4 * (x + y * ss_w) + 2];
  auto &a = supersample_target[4 * (x + y * ss_w) + 3];

  r = (1 - color.a) * r + color.r * 255 * color.a;
  g = (1 - color.a) * g + color.g * 255 * color.a;
  b = (1 - color.a) * b + color.b * 255 * color.a;
  a = (1 - color.a) * a + color.a * 255;
}

} // namespace CMU462
