#pragma once

#include "../Common/AlgorithmParams.h"
#include "../Common/AlgorithmResult.h"

#include <QImage>
#include <QString>

namespace HMVision
{
class IVisionAlgorithm
{
public:
    virtual ~IVisionAlgorithm() = default;

    virtual QString name() const = 0;
    virtual bool initialize(const AlgorithmParams& params) = 0;
    virtual AlgorithmResult process(const QImage& frame) = 0;
    virtual void release() = 0;
    virtual bool isInitialized() const = 0;
};
} // namespace HMVision
