#include "../../include/UI/MainWindow.h"

#include "../../include/UI/AlgorithmPage.h"
#include "../../include/UI/CameraPage.h"
#include "../../include/UI/MainInterfaceWidget.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace HMVision
{
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("HM_VISION — 慧眼视觉");
    resize(1600, 900);

    setupUi();
    setupMenus();
    setupToolbar();
    setupStatusbar();
}

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    m_tabWidget = new QTabWidget(central);
    layout->addWidget(m_tabWidget);
    setCentralWidget(central);

    m_mainInterface = new MainInterfaceWidget(m_tabWidget);
    m_tabWidget->addTab(m_mainInterface, "主界面");

    m_cameraPage = new CameraPage(
        m_mainInterface->multiCameraView(),
        m_mainInterface->operationLogWidget(),
        m_mainInterface->alarmLogWidget(),
        m_tabWidget);
    m_tabWidget->addTab(m_cameraPage, "相机设置");

    connect(
        m_cameraPage,
        &CameraPage::camerasChanged,
        m_mainInterface,
        &MainInterfaceWidget::onCameraManagerCountChanged);

    m_algorithmPage = new AlgorithmPage(m_tabWidget);
    m_tabWidget->addTab(m_algorithmPage, "算法设置");

    auto makePlaceholder = [this](const QString& title) {
        auto* page = new QWidget(m_tabWidget);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->addWidget(new QLabel(title + " (Placeholder)", page));
        pageLayout->addStretch(1);
        return page;
    };

    m_tabWidget->addTab(makePlaceholder("RecipePage: 产品配方"), "配方");
    m_tabWidget->addTab(makePlaceholder("IOPage: IO设置"), "IO");
    m_tabWidget->addTab(makePlaceholder("DataPage: 数据查询"), "数据");
    m_tabWidget->addTab(makePlaceholder("StatusPage: 系统状态"), "状态");
}

void MainWindow::setupMenus()
{
    auto* fileMenu = menuBar()->addMenu("File");
    auto* systemMenu = menuBar()->addMenu("System");
    auto* helpMenu = menuBar()->addMenu("Help");

    m_exitAction = fileMenu->addAction("Exit");
    m_startAction = systemMenu->addAction("Start");
    m_stopAction = systemMenu->addAction("Stop");
    m_aboutAction = helpMenu->addAction("About");

    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_startAction, &QAction::triggered, this, [this]() {
        statusBar()->showMessage("System start requested", 2000);
    });
    connect(m_stopAction, &QAction::triggered, this, [this]() {
        statusBar()->showMessage("System stop requested", 2000);
    });
    connect(m_aboutAction, &QAction::triggered, this, [this]() {
        statusBar()->showMessage("HM_VISION", 3000);
    });
}

void MainWindow::setupToolbar()
{
    auto* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);
    toolbar->addAction(m_startAction);
    toolbar->addAction(m_stopAction);
    toolbar->addSeparator();
    toolbar->addAction(m_exitAction);
}

void MainWindow::setupStatusbar()
{
    statusBar()->showMessage("Ready | 主界面查看多路画面，「相机设置」中连接与采集");
}
} // namespace HMVision
