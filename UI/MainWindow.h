#pragma once

#include "../Core/VisionSystem.h"

#include <QMainWindow>

namespace HMVision
{
class MultiCameraView;
class ResultRepository;
class QTableWidget;
class QTabWidget;
class QLabel;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(VisionSystem& visionSystem, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void createUi();
    QWidget* createCameraSettingsPage();
    QWidget* createAlgorithmParamsPage();
    QWidget* createRecipePage();
    QWidget* createIoSettingsPage();
    QWidget* createDataQueryPage();
    QWidget* createSystemStatusPage();
    void bindSignals();
    void refreshCameraTable();
    void refreshSystemStats();
    void appendLogLine(const QString& line);
    QString statusToText(DeviceStatus status) const;

    VisionSystem& m_visionSystem;
    QTabWidget* m_tabs = nullptr;
    MultiCameraView* m_multiCameraView = nullptr;
    QTableWidget* m_cameraTable = nullptr;
    QTableWidget* m_dataTable = nullptr;
    QLabel* m_cameraCountLabel = nullptr;
    QLabel* m_frameRateLabel = nullptr;
    QLabel* m_schedulerLabel = nullptr;
    QTextEdit* m_logView = nullptr;
    ResultRepository* m_resultRepository = nullptr;
};
} // namespace HMVision
