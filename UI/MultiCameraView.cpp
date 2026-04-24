#include "MultiCameraView.h"

#include <algorithm>
#include <cmath>

namespace HMVision
{
MultiCameraView::MultiCameraView(QWidget* parent)
    : QWidget(parent)
{
    m_gridLayout = new QGridLayout(this);
    m_gridLayout->setContentsMargins(6, 6, 6, 6);
    m_gridLayout->setSpacing(6);
    setLayout(m_gridLayout);
}

void MultiCameraView::addCamera(const std::string& cameraId)
{
    if (m_cameraWidgets.find(cameraId) != m_cameraWidgets.end())
    {
        return;
    }

    auto* widget = new CameraWidget(cameraId, this);
    m_cameraWidgets.emplace(cameraId, widget);
    relayout();
}

void MultiCameraView::removeCamera(const std::string& cameraId)
{
    auto iter = m_cameraWidgets.find(cameraId);
    if (iter == m_cameraWidgets.end())
    {
        return;
    }

    CameraWidget* widget = iter->second;
    m_gridLayout->removeWidget(widget);
    delete widget;
    m_cameraWidgets.erase(iter);
    relayout();
}

void MultiCameraView::updateCameraView(const std::string& cameraId, const cv::Mat& image)
{
    auto iter = m_cameraWidgets.find(cameraId);
    if (iter == m_cameraWidgets.end())
    {
        addCamera(cameraId);
        iter = m_cameraWidgets.find(cameraId);
    }
    iter->second->updateImage(image);
}

void MultiCameraView::updateCameraView(const QString& cameraId, const QImage& image)
{
    const std::string id = cameraId.toStdString();
    auto iter = m_cameraWidgets.find(id);
    if (iter == m_cameraWidgets.end())
    {
        addCamera(id);
        iter = m_cameraWidgets.find(id);
    }
    iter->second->updateFrame(image);
}

void MultiCameraView::updateCameraResult(const std::string& cameraId, const AlgorithmResult& result)
{
    auto iter = m_cameraWidgets.find(cameraId);
    if (iter == m_cameraWidgets.end())
    {
        return;
    }
    iter->second->updateResult(result);
}

void MultiCameraView::setCameraConnected(const std::string& cameraId, bool connected)
{
    auto iter = m_cameraWidgets.find(cameraId);
    if (iter == m_cameraWidgets.end())
    {
        return;
    }
    iter->second->setConnected(connected);
}

bool MultiCameraView::hasCamera(const std::string& cameraId) const
{
    return m_cameraWidgets.find(cameraId) != m_cameraWidgets.end();
}

void MultiCameraView::relayout()
{
    while (QLayoutItem* item = m_gridLayout->takeAt(0))
    {
        if (item->widget())
        {
            item->widget()->setParent(this);
        }
        delete item;
    }

    const int count = static_cast<int>(m_cameraWidgets.size());
    if (count <= 0)
    {
        return;
    }

    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    int index = 0;
    for (const auto& pair : m_cameraWidgets)
    {
        const int row = index / columns;
        const int col = index % columns;
        m_gridLayout->addWidget(pair.second, row, col);
        ++index;
    }
}
} // namespace HMVision
