#pragma once

#include "CameraWidget.h"
#include "../Common/AlgorithmResult.h"

#include <opencv2/core.hpp>

#include <QGridLayout>
#include <QWidget>

#include <string>
#include <unordered_map>

namespace HMVision
{
class MultiCameraView : public QWidget
{
    Q_OBJECT
public:
    explicit MultiCameraView(QWidget* parent = nullptr);

    void addCamera(const std::string& cameraId);
    void removeCamera(const std::string& cameraId);
    void updateCameraView(const std::string& cameraId, const cv::Mat& image);
    void updateCameraView(const QString& cameraId, const QImage& image);
    void updateCameraResult(const std::string& cameraId, const AlgorithmResult& result);
    void setCameraConnected(const std::string& cameraId, bool connected);
    bool hasCamera(const std::string& cameraId) const;

private:
    void relayout();

    std::unordered_map<std::string, CameraWidget*> m_cameraWidgets;
    QGridLayout* m_gridLayout = nullptr;
};
} // namespace HMVision
