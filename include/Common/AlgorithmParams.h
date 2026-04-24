#pragma once

#include <map>
#include <string>
#include <vector>

namespace HMVision
{
struct AlgorithmParams
{
    std::string modelPath;
    std::string device = "cpu";
    float confidenceThreshold = 0.25F;
    float nmsThreshold = 0.45F;
    int inputWidth = 640;
    int inputHeight = 640;
    std::vector<std::string> labels;
    std::map<std::string, std::string> custom;
};
} // namespace HMVision
