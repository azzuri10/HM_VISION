#include "CameraWidget.h"

#include <opencv2/imgproc.hpp>

#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace HMVision
{
CameraWidget::CameraWidget(const std::string& cameraId, QWidget* parent)
    : QWidget(parent)
    , m_cameraId(QString::fromStdString(cameraId))
{
    auto* layout = new QVBoxLayout(this);
    m_imageLabel = new QLabel("No frame", this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(320, 240);
    m_imageLabel->setStyleSheet("QLabel { background: #1e1e1e; color: #cccccc; }");
    m_imageLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    m_imageLabel->installEventFilter(this);

    m_statusLabel = new QLabel(this);
    updateStatusText();

    layout->addWidget(m_imageLabel, 1);
    layout->addWidget(m_statusLabel);
    setLayout(layout);

    connect(m_imageLabel, &QLabel::customContextMenuRequested, this, [this](const QPoint&) {
        QMenu menu(this);
        auto* saveAction = menu.addAction("Save Image");
        const QAction* selected = menu.exec(QCursor::pos());
        if (selected == saveAction && !m_displayImage.isNull())
        {
            const QString filePath = QFileDialog::getSaveFileName(
                this, "Save Camera Image", QString("%1.png").arg(m_cameraId), "PNG Image (*.png)");
            if (!filePath.isEmpty())
            {
                m_displayImage.save(filePath);
            }
        }
    });
}

void CameraWidget::updateImage(const cv::Mat& image)
{
    updateFrame(matToQImage(image));
}

void CameraWidget::updateFrame(const QImage& frame)
{
    if (frame.isNull())
    {
        return;
    }

    m_rawImage = frame;
    ++m_frameCount;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastFpsUpdateMs == 0)
    {
        m_lastFpsUpdateMs = now;
    }
    const qint64 elapsed = now - m_lastFpsUpdateMs;
    if (elapsed >= 1000)
    {
        m_fps = static_cast<double>(m_frameCount) * 1000.0 / static_cast<double>(elapsed);
        m_frameCount = 0;
        m_lastFpsUpdateMs = now;
    }

    refreshView();
    updateStatusText();
}

void CameraWidget::updateResult(const AlgorithmResult& result)
{
    m_result = result;
    refreshView();
}

void CameraWidget::setConnected(bool connected)
{
    m_connected = connected;
    updateStatusText();
}

QString CameraWidget::cameraId() const
{
    return m_cameraId;
}

bool CameraWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_imageLabel)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            m_selectingRoi = true;
            m_roiStart = mouseEvent->pos();
            m_selectedRoi = QRect(m_roiStart, m_roiStart);
            refreshView();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        if (m_selectingRoi)
        {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            m_selectedRoi = QRect(m_roiStart, mouseEvent->pos()).normalized();
            refreshView();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_selectingRoi)
        {
            m_selectingRoi = false;
            m_selectedRoi = QRect(m_roiStart, mouseEvent->pos()).normalized();
            refreshView();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CameraWidget::wheelEvent(QWheelEvent* event)
{
    const double delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
    m_scaleFactor = std::clamp(m_scaleFactor + delta, 0.2, 5.0);
    refreshView();
    event->accept();
}

void CameraWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshView();
}

void CameraWidget::refreshView()
{
    if (m_rawImage.isNull())
    {
        return;
    }

    m_displayImage = m_rawImage.copy();
    QPainter painter(&m_displayImage);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(QPen(Qt::green, 2));
    for (const auto& det : m_result.detections)
    {
        painter.drawRect(det.box);
        painter.drawText(det.box.topLeft() + QPointF(0, -4),
            QString("%1 (%2)")
                .arg(det.className)
                .arg(QString::number(det.confidence, 'f', 2)));
    }

    if (!m_selectedRoi.isNull())
    {
        QRect imageRect = mapLabelRectToImage(m_selectedRoi);
        painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
        painter.drawRect(imageRect);
    }
    painter.end();

    const QSize scaledSize = m_displayImage.size() * m_scaleFactor;
    QPixmap pixmap = QPixmap::fromImage(m_displayImage.scaled(
        scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imageLabel->setPixmap(pixmap.scaled(
        m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage CameraWidget::matToQImage(const cv::Mat& image)
{
    if (image.empty())
    {
        return {};
    }

    cv::Mat rgb;
    if (image.channels() == 1)
    {
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
    }
    else
    {
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    }

    return QImage(
               rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888)
        .copy();
}

QRect CameraWidget::mapLabelRectToImage(const QRect& rectInLabel) const
{
    if (m_rawImage.isNull() || m_imageLabel->width() <= 0 || m_imageLabel->height() <= 0)
    {
        return {};
    }

    const double sx = static_cast<double>(m_rawImage.width()) / static_cast<double>(m_imageLabel->width());
    const double sy =
        static_cast<double>(m_rawImage.height()) / static_cast<double>(m_imageLabel->height());
    return QRect(
        static_cast<int>(rectInLabel.x() * sx),
        static_cast<int>(rectInLabel.y() * sy),
        static_cast<int>(rectInLabel.width() * sx),
        static_cast<int>(rectInLabel.height() * sy));
}

void CameraWidget::updateStatusText()
{
    const QString connectionText = m_connected ? "Connected" : "Disconnected";
    const QString roiText = m_selectedRoi.isNull()
        ? "ROI: None"
        : QString("ROI: %1x%2").arg(m_selectedRoi.width()).arg(m_selectedRoi.height());
    m_statusLabel->setText(QString("Camera: %1 | %2 | FPS: %3 | %4")
                               .arg(m_cameraId)
                               .arg(connectionText)
                               .arg(QString::number(m_fps, 'f', 1))
                               .arg(roiText));
}
} // namespace HMVision
