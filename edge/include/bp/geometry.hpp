#pragma once

#include "bp/types.hpp"

namespace bp {

// Intersection over Union di due box: 0 = disgiunte, 1 = identiche.
// Usata dal tracker (abbinamento tra frame) e dalla NMS del detector (doppioni).
float iou(const Detection& a, const Detection& b);

} // namespace bp
