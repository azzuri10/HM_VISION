#pragma once

#include "UI/CameraWidget.h"

#include <QGridLayout>
#include <QImage>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace HMVision
{
class MultiCameraView : public QWidget
{
    Q_OBJECT
public:
    explicit MultiCameraView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        m_layout = new QGridLayout(this);
        m_layout->setContentsMargins(6, 6, 6, 6);
        m_layout->setSpacing(6);
        setLayout(m_layout);
    }

    void addCamera(const std::string& cameraId)
    {
        if (m_widgets.find(cameraId) != m_widgets.end())
        {
            return;
        }

        auto* widget = new CameraWidget(cameraId, this);
        m_widgets.emplace(cameraId, widget);
        relayout();
    }

    void removeCamera(const std::string& cameraId)
    {
        const auto it = m_widgets.find(cameraId);
        if (it == m_widgets.end())
        {
            return;
        }

        m_layout->removeWidget(it->second);
        delete it->second;
        m_widgets.erase(it);
        relayout();
    }

    CameraWidget* findWidget(const std::string& cameraId) const
    {
        const auto it = m_widgets.find(cameraId);
        if (it == m_widgets.end())
        {
            return nullptr;
        }
        return it->second;
    }

    void setPreviewForCamera(const std::string& cameraId, const QImage& image)
    {
        if (auto* w = findWidget(cameraId))
        {
            w->setPreviewImage(image);
        }
    }

    /// \p n > 0 时按固定列数排布（如 2 即 2×N 网格）；0 表示自动按 sqrt(路数)
    void setGridColumns(int n)
    {
        m_gridColumns = n;
        relayout();
    }

private:
    void relayout()
    {
        while (QLayoutItem* item = m_layout->takeAt(0))
        {
            delete item;
        }

        const int count = static_cast<int>(m_widgets.size());
        if (count == 0)
        {
            return;
        }

        int columns = 0;
        if (m_gridColumns > 0)
        {
            columns = std::min(m_gridColumns, count);
        }
        else
        {
            columns
                = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
        }
        if (columns < 1)
        {
            columns = 1;
        }
        int index = 0;
        for (const auto& pair : m_widgets)
        {
            const int row = index / columns;
            const int col = index % columns;
            m_layout->addWidget(pair.second, row, col);
            ++index;
        }
    }

    QGridLayout* m_layout = nullptr;
    std::unordered_map<std::string, CameraWidget*> m_widgets;
    int m_gridColumns{0};
};
} // namespace HMVision
