#pragma once

#include "IVisionAlgorithm.h"

#include <QString>
#include <memory>

namespace HMVision
{
enum class AlgorithmType
{
    Detection,
    OCR,
    Halcon
};

class AlgorithmFactory
{
public:
    static std::shared_ptr<IVisionAlgorithm> create(AlgorithmType type);
    static std::shared_ptr<IVisionAlgorithm> create(const QString& typeName);
};
} // namespace HMVision
