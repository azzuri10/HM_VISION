#pragma once

#include "../Common/AlgorithmResult.h"

#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

#include <string>

namespace cv
{
class Mat;
}

namespace HMVision
{
class CameraWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraWidget(const std::string& cameraId, QWidget* parent = nullptr);

    void updateImage(const cv::Mat& image);
    void updateFrame(const QImage& frame);
    void updateResult(const AlgorithmResult& result);
    void setConnected(bool connected);
    QString cameraId() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void refreshView();
    static QImage matToQImage(const cv::Mat& image);
    QRect mapLabelRectToImage(const QRect& rectInLabel) const;
    void updateStatusText();

    QLabel* m_imageLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QString m_cameraId;
    QImage m_rawImage;
    QImage m_displayImage;
    AlgorithmResult m_result;
    double m_scaleFactor = 1.0;
    bool m_connected = false;
    int m_frameCount = 0;
    qint64 m_lastFpsUpdateMs = 0;
    double m_fps = 0.0;
    bool m_selectingRoi = false;
    QPoint m_roiStart;
    QRect m_selectedRoi;
};
} // namespace HMVision
