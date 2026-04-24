#pragma once

#include "../Common/AlgorithmResult.h"

#include <QString>
#include <QVector>

class QSqlDatabase;

namespace HMVision
{
struct DetectionRecord
{
    qint64 id = 0;
    QString cameraId;
    QString algorithmName;
    QString resultSummary;
    qint64 timestampMs = 0;
    bool success = false;
};

class ResultRepository
{
public:
    ResultRepository();
    ~ResultRepository();

    bool open(const QString& dbPath);
    bool initSchema();
    bool insertResult(const QString& cameraId, const AlgorithmResult& result);
    QVector<DetectionRecord> queryRecent(int limit = 100) const;

private:
    QString m_connectionName;
    QSqlDatabase* m_db = nullptr;
};
} // namespace HMVision
