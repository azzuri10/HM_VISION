#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <QVariantMap>

namespace HMVision
{
struct DetectionBox
{
    QRectF box;
    float confidence = 0.0F;
    int classId = -1;
    QString className;
};

struct AlgorithmResult
{
    bool success = false;
    QString algorithmName;
    QString message;
    qint64 elapsedMs = 0;
    QList<DetectionBox> detections;
    QVariantMap metadata;
};
} // namespace HMVision
