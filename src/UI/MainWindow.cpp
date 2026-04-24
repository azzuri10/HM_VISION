#include "../../include/UI/MainWindow.h"

#include "../../include/UI/AlgorithmPage.h"
#include "../../include/UI/CameraPage.h"

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
    setWindowTitle("HM_VISION - Qt6 Main Window");
    resize(1400, 900);

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

    m_cameraPage = new CameraPage(m_tabWidget);
    m_tabWidget->addTab(m_cameraPage, "CameraPage: 相机设置");
    m_algorithmPage = new AlgorithmPage(m_tabWidget);
    m_tabWidget->addTab(m_algorithmPage, "AlgorithmPage: 算法设置");

    auto makePlaceholder = [this](const QString& title) {
        auto* page = new QWidget(m_tabWidget);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->addWidget(new QLabel(title + " (Placeholder)", page));
        pageLayout->addStretch(1);
        return page;
    };

    m_tabWidget->addTab(makePlaceholder("RecipePage: 产品配方"), "RecipePage");
    m_tabWidget->addTab(makePlaceholder("IOPage: IO设置"), "IOPage");
    m_tabWidget->addTab(makePlaceholder("DataPage: 数据查询"), "DataPage");
    m_tabWidget->addTab(makePlaceholder("StatusPage: 系统状态"), "StatusPage");
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
        statusBar()->showMessage("HM_VISION Qt6 placeholder framework", 3000);
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
    statusBar()->showMessage("Ready | Qt 6.3.1 Framework");
}
} // namespace HMVision
