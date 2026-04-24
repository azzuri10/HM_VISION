#include "Data/ResultRepository.h"

#include "Core/Logger.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace HMVision
{
ResultRepository::ResultRepository()
{
    m_connectionName = QString("hmvision_db_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

ResultRepository::~ResultRepository()
{
    if (m_db != nullptr)
    {
        if (m_db->isOpen())
        {
            m_db->close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        delete m_db;
        m_db = nullptr;
    }
}

bool ResultRepository::open(const QString& dbPath)
{
    if (m_db != nullptr)
    {
        delete m_db;
        m_db = nullptr;
    }

    auto db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        Logger::error(
            QString("Failed to open SQLite: %1").arg(db.lastError().text()).toStdString());
        return false;
    }

    m_db = new QSqlDatabase(db);
    return initSchema();
}

bool ResultRepository::initSchema()
{
    if (m_db == nullptr || !m_db->isOpen())
    {
        return false;
    }

    QSqlQuery query(*m_db);
    const bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS detection_results ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "camera_id TEXT NOT NULL,"
        "algorithm_name TEXT NOT NULL,"
        "result_summary TEXT,"
        "success INTEGER NOT NULL,"
        "timestamp_ms INTEGER NOT NULL)");

    if (!ok)
    {
        Logger::error(
            QString("Init schema failed: %1").arg(query.lastError().text()).toStdString());
    }
    return ok;
}

bool ResultRepository::insertResult(const QString& cameraId, const AlgorithmResult& result)
{
    if (m_db == nullptr || !m_db->isOpen())
    {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(
        "INSERT INTO detection_results "
        "(camera_id, algorithm_name, result_summary, success, timestamp_ms) "
        "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(cameraId);
    query.addBindValue(QString::fromStdString(result.algorithmName));
    query.addBindValue(QString::fromStdString(result.message));
    query.addBindValue(result.success ? 1 : 0);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec())
    {
        Logger::warning(
            QString("Insert result failed: %1").arg(query.lastError().text()).toStdString());
        return false;
    }
    return true;
}

QVector<DetectionRecord> ResultRepository::queryRecent(int limit) const
{
    QVector<DetectionRecord> records;
    if (m_db == nullptr || !m_db->isOpen())
    {
        return records;
    }

    QSqlQuery query(*m_db);
    query.prepare(
        "SELECT id, camera_id, algorithm_name, result_summary, success, timestamp_ms "
        "FROM detection_results ORDER BY id DESC LIMIT ?");
    query.addBindValue(limit);
    if (!query.exec())
    {
        Logger::warning(
            QString("Query recent failed: %1").arg(query.lastError().text()).toStdString());
        return records;
    }

    while (query.next())
    {
        DetectionRecord row;
        row.id = query.value(0).toLongLong();
        row.cameraId = query.value(1).toString();
        row.algorithmName = query.value(2).toString();
        row.resultSummary = query.value(3).toString();
        row.success = query.value(4).toInt() != 0;
        row.timestampMs = query.value(5).toLongLong();
        records.push_back(row);
    }
    return records;
}
} // namespace HMVision
