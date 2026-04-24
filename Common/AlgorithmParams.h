#pragma once

#include <QString>
#include <QStringList>

namespace HMVision
{
struct AlgorithmParams
{
    QString modelPath;
    QString classesPath;
    QString device = "cpu";
    float confidenceThreshold = 0.25F;
    float nmsThreshold = 0.45F;
    int inputWidth = 640;
    int inputHeight = 640;
    QStringList labels;
};
} // namespace HMVision
