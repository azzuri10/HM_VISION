#include "UI/CameraWidget.h"

#include <QImage>

namespace HMVision
{
CameraWidget::CameraWidget(const std::string& cameraId, QWidget* parent)
    : QFrame(parent)
    , m_cameraId(QString::fromStdString(cameraId))
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumSize(240, 180);

    auto* layout = new QVBoxLayout(this);
    m_statusLabel = new QLabel(QString("Camera: %1").arg(m_cameraId), this);
    m_previewLabel = new QLabel("No image", this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("QLabel { background-color: #202020; color: #d0d0d0; }");
    m_previewLabel->setScaledContents(false);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_previewLabel, 1);
}

void CameraWidget::setPreviewText(const QString& text)
{
    m_previewLabel->setText(text);
    m_previewLabel->setPixmap(QPixmap());
}

void CameraWidget::setPreviewImage(const QImage& image)
{
    if (image.isNull())
    {
        setPreviewText("No image");
        return;
    }
    m_previewLabel->setPixmap(QPixmap::fromImage(image).scaled(
        m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_previewLabel->setText(QString());
}
} // namespace HMVision
