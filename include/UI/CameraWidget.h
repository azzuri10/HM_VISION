#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <string>

namespace HMVision
{
class CameraWidget : public QFrame
{
    Q_OBJECT
public:
    explicit CameraWidget(const std::string& cameraId, QWidget* parent = nullptr);

    QString qCameraId() const
    {
        return m_cameraId;
    }
    std::string cameraId() const
    {
        return m_cameraId.toStdString();
    }

    void setPreviewText(const QString& text);
    void setPreviewImage(const QImage& image);

private:
    QString m_cameraId;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
};
} // namespace HMVision
