#pragma once

#include "MultiCameraView.h"

#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace HMVision
{
class CameraPage : public QWidget
{
    Q_OBJECT
public:
    explicit CameraPage(QWidget* parent = nullptr);

private:
    MultiCameraView* m_multiCameraView = nullptr;
    QTableWidget* m_cameraTable = nullptr;
    QPushButton* m_addMockButton = nullptr;
    QPushButton* m_removeSelectedButton = nullptr;
};
} // namespace HMVision
