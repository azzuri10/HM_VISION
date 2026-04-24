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
    explicit CameraWidget(const std::string& cameraId, QWidget* parent = nullptr)
        : QFrame(parent)
        , m_cameraId(QString::fromStdString(cameraId))
    {
        setFrameShape(QFrame::StyledPanel);
        setMinimumSize(240, 180);

        auto* layout = new QVBoxLayout(this);
        auto* title = new QLabel(QString("Camera: %1").arg(m_cameraId), this);
        m_previewLabel = new QLabel("Preview Placeholder", this);
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setStyleSheet("QLabel { background-color: #202020; color: #d0d0d0; }");
        layout->addWidget(title);
        layout->addWidget(m_previewLabel, 1);
    }

    QString cameraId() const
    {
        return m_cameraId;
    }

private:
    QString m_cameraId;
    QLabel* m_previewLabel = nullptr;
};
} // namespace HMVision
