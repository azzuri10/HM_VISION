#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace HMVision
{
struct BoundingBox
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct DetectionResult
{
    BoundingBox box;
    float confidence = 0.0F;
    int classId = -1;
    std::string className;
};

struct AlgorithmResult
{
    bool success = false;
    std::string algorithmName;
    std::string message;
    std::int64_t elapsedMs = 0;
    std::vector<DetectionResult> detections;
    std::map<std::string, std::string> metadata;
};
} // namespace HMVision
