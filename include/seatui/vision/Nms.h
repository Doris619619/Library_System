#pragma once
#include <vector>
#include "Types.h"

namespace vision {

// 对同类别的检测框按置信度排序并做 IoU 抑制
// 输入: 原始检测框集合
// 参数: iou_thres 重叠阈值，>该阈值则抑制低分框
// 输出: 经过 NMS 后的检测框集合
std::vector<BBox> nmsClasswise(const std::vector<BBox>& boxes, float iou_thres);

} // namespace vision
