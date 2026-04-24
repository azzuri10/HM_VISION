#pragma once

#include <QMainWindow>

class QAction;
class QTabWidget;

namespace HMVision
{
class AlgorithmPage;
class CameraPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupStatusbar();

    QTabWidget* m_tabWidget = nullptr;
    CameraPage* m_cameraPage = nullptr;
    AlgorithmPage* m_algorithmPage = nullptr;

    QAction* m_exitAction = nullptr;
    QAction* m_startAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_aboutAction = nullptr;
};
} // namespace HMVision
