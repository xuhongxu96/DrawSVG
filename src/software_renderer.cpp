#include "software_renderer.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>
#include <algorithm>

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
  rasterize_line(p0.x, p0.y, p1.x, p1.y, line.style.strokeColor);
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

  // Extra credit
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
                                         Color color) {

  // Task 2:
  // Implement line rasterization

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
      if (x >= 0 && x < sample_w && y >= 0 && y < sample_h)
        for (int i = 0; i < sample_rate; ++i)
          set_color_in_supersample_target(x, y + i, color);

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
      if (x >= 0 && x < sample_w && y >= 0 && y < sample_h)
        for (int i = 0; i < sample_rate; ++i)
          set_color_in_supersample_target(x + i, y, color);

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
