#include "bp/geometry.hpp"

#include <algorithm>

namespace bp {

float iou(const Detection& a, const Detection& b) {
    const float ix = std::max(0.0F, std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x));
    const float iy = std::max(0.0F, std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y));
    const float inter = ix * iy;
    const float uni = a.w * a.h + b.w * b.h - inter;
    return uni > 0.0F ? inter / uni : 0.0F;
}

} // namespace bp
