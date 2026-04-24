#include "MainWindow.h"
#include "MultiCameraView.h"
#include "../Core/Logger.h"
#include "../Data/ResultRepository.h"
#include "../Device/GigE_Camera.h"
#include "../Device/USB_Camera.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QUuid>
#include <QWidget>
#include <memory>

namespace HMVision
{
MainWindow::MainWindow(VisionSystem& visionSystem, QWidget* parent)
    : QMainWindow(parent)
    , m_visionSystem(visionSystem)
{
    setWindowTitle("HM_VISION");
    resize(1440, 900);

    m_resultRepository = new ResultRepository();
    m_resultRepository->open("hm_vision.db");

    createUi();
    bindSignals();
    refreshCameraTable();
    refreshSystemStats();

    auto* toolBar = addToolBar("Main Toolbar");
    toolBar->addAction("Initialize", [this]() {
        const bool success = m_visionSystem.initialize();
        statusBar()->showMessage(success ? "System initialized" : "Initialize failed", 2000);
    });
    toolBar->addAction("Shutdown", [this]() {
        m_visionSystem.shutdown();
        statusBar()->showMessage("System stopped", 2000);
    });
    toolBar->addAction("Start All", [this]() {
        const bool success = m_visionSystem.cameraManager().startAll();
        statusBar()->showMessage(success ? "All cameras started" : "Some cameras failed to start", 2000);
    });
    toolBar->addAction("Stop All", [this]() {
        m_visionSystem.cameraManager().stopAll();
        statusBar()->showMessage("All cameras stopped", 2000);
    });

    statusBar()->showMessage("Ready");

    Logger::registerSink([this](Logger::Level, const QString& message) {
        appendLogLine(message);
    });
}

MainWindow::~MainWindow()
{
    delete m_resultRepository;
    m_resultRepository = nullptr;
}

void MainWindow::createUi()
{
    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_tabs->addTab(createCameraSettingsPage(), "相机设置页面");
    m_tabs->addTab(createAlgorithmParamsPage(), "算法参数页面");
    m_tabs->addTab(createRecipePage(), "产品配方页面");
    m_tabs->addTab(createIoSettingsPage(), "IO设置页面");
    m_tabs->addTab(createDataQueryPage(), "数据查询页面");
    m_tabs->addTab(createSystemStatusPage(), "系统状态页面");
}

QWidget* MainWindow::createCameraSettingsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_multiCameraView = new MultiCameraView(page);
    layout->addWidget(m_multiCameraView, 1);

    m_cameraTable = new QTableWidget(page);
    m_cameraTable->setColumnCount(3);
    m_cameraTable->setHorizontalHeaderLabels({"ID", "Name", "Status"});
    m_cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_cameraTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cameraTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cameraTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_cameraTable);

    auto* row = new QHBoxLayout();
    auto* addUsbButton = new QPushButton("Add USB Camera", page);
    auto* addGigEButton = new QPushButton("Add GigE Camera", page);
    auto* removeButton = new QPushButton("Remove Camera", page);
    auto* startButton = new QPushButton("Start Camera", page);
    auto* stopButton = new QPushButton("Stop Camera", page);
    row->addWidget(addUsbButton);
    row->addWidget(addGigEButton);
    row->addWidget(removeButton);
    row->addWidget(startButton);
    row->addWidget(stopButton);
    layout->addLayout(row);

    connect(addUsbButton, &QPushButton::clicked, this, [this]() {
        const QString id = QString("usb-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        auto camera = std::make_shared<USB_Camera>(id, 0, this);
        if (m_visionSystem.cameraManager().registerCamera(id, camera))
        {
            m_multiCameraView->addCamera(id.toStdString());
            Logger::info(QString("USB camera added: %1").arg(id));
        }
    });
    connect(addGigEButton, &QPushButton::clicked, this, [this]() {
        const QString id = QString("gige-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        auto camera = std::make_shared<GigE_Camera>(id, "192.168.1.10:3956", this);
        if (m_visionSystem.cameraManager().registerCamera(id, camera))
        {
            m_multiCameraView->addCamera(id.toStdString());
            Logger::info(QString("GigE camera added: %1").arg(id));
        }
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const QString id = m_cameraTable->item(selected.first()->row(), 0)->text();
        m_visionSystem.cameraManager().removeCamera(id);
    });
    connect(startButton, &QPushButton::clicked, this, [this]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const QString id = m_cameraTable->item(selected.first()->row(), 0)->text();
        m_visionSystem.cameraManager().startCamera(id);
    });
    connect(stopButton, &QPushButton::clicked, this, [this]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const QString id = m_cameraTable->item(selected.first()->row(), 0)->text();
        m_visionSystem.cameraManager().stopCamera(id);
    });
    return page;
}

QWidget* MainWindow::createAlgorithmParamsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("Algorithm Type (YOLOv8/YOLOv26/PaddleOCR/Halcon)", page));
    layout->addWidget(new QLineEdit(page));
    layout->addWidget(new QLabel("Confidence Threshold", page));
    layout->addWidget(new QLineEdit(page));
    layout->addWidget(new QLabel("NMS Threshold", page));
    layout->addWidget(new QLineEdit(page));
    layout->addStretch(1);
    return page;
}

QWidget* MainWindow::createRecipePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("Product Recipe Management", page));
    layout->addWidget(new QTextEdit(page));
    return page;
}

QWidget* MainWindow::createIoSettingsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("PLC/MES IO settings", page));
    layout->addWidget(new QLineEdit(page));
    layout->addStretch(1);
    return page;
}

QWidget* MainWindow::createDataQueryPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    m_dataTable = new QTableWidget(page);
    m_dataTable->setColumnCount(5);
    m_dataTable->setHorizontalHeaderLabels({"ID", "Camera", "Algorithm", "Message", "Timestamp"});
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_dataTable);

    auto* refreshButton = new QPushButton("Refresh Data", page);
    layout->addWidget(refreshButton);
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        if (m_resultRepository == nullptr)
        {
            return;
        }
        const auto rows = m_resultRepository->queryRecent(200);
        m_dataTable->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i)
        {
            const auto& r = rows[i];
            m_dataTable->setItem(i, 0, new QTableWidgetItem(QString::number(r.id)));
            m_dataTable->setItem(i, 1, new QTableWidgetItem(r.cameraId));
            m_dataTable->setItem(i, 2, new QTableWidgetItem(r.algorithmName));
            m_dataTable->setItem(i, 3, new QTableWidgetItem(r.resultSummary));
            m_dataTable->setItem(i, 4, new QTableWidgetItem(QString::number(r.timestampMs)));
        }
    });
    return page;
}

QWidget* MainWindow::createSystemStatusPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    m_cameraCountLabel = new QLabel(page);
    m_frameRateLabel = new QLabel(page);
    m_schedulerLabel = new QLabel(page);
    m_logView = new QTextEdit(page);
    m_logView->setReadOnly(true);

    layout->addWidget(m_cameraCountLabel);
    layout->addWidget(m_frameRateLabel);
    layout->addWidget(m_schedulerLabel);
    layout->addWidget(new QLabel("日志显示窗口", page));
    layout->addWidget(m_logView, 1);
    return page;
}

void MainWindow::bindSignals()
{
    auto& manager = m_visionSystem.cameraManager();
    connect(&manager, &CameraManager::cameraRegistered, this, [this](const QString&) {
        refreshCameraTable();
        refreshSystemStats();
    });
    connect(&manager, &CameraManager::cameraRemoved, this, [this](const QString& id) {
        m_multiCameraView->removeCamera(id.toStdString());
        refreshCameraTable();
        refreshSystemStats();
    });
    connect(&manager, &CameraManager::cameraStatusChanged, this, [this](const QString& id, DeviceStatus status) {
        m_multiCameraView->setCameraConnected(id.toStdString(), status == DeviceStatus::Connected);
        refreshCameraTable();
        refreshSystemStats();
    });
    connect(&manager, &CameraManager::cameraFrameReady, this, [this](const QString& id, const QImage& frame) {
        m_multiCameraView->updateCameraView(id, frame);
        if (m_resultRepository != nullptr)
        {
            AlgorithmResult row;
            row.success = true;
            row.algorithmName = "camera_frame";
            row.message = "Frame captured";
            m_resultRepository->insertResult(id, row);
        }
    });
}

void MainWindow::refreshCameraTable()
{
    const QList<CameraInfo> cameras = m_visionSystem.cameraManager().cameraList();
    m_cameraTable->setRowCount(cameras.size());

    for (int row = 0; row < cameras.size(); ++row)
    {
        const auto& info = cameras[row];
        m_cameraTable->setItem(row, 0, new QTableWidgetItem(info.id));
        m_cameraTable->setItem(row, 1, new QTableWidgetItem(info.name));
        m_cameraTable->setItem(row, 2, new QTableWidgetItem(statusToText(info.status)));
    }
}

void MainWindow::refreshSystemStats()
{
    const int cameraCount = m_visionSystem.cameraManager().cameraList().size();
    m_cameraCountLabel->setText(QString("Camera Count: %1").arg(cameraCount));
    m_frameRateLabel->setText("Realtime FPS: managed in each camera widget");
    m_schedulerLabel->setText("Scheduler: queue/priority enabled");
}

void MainWindow::appendLogLine(const QString& line)
{
    if (m_logView == nullptr)
    {
        return;
    }
    m_logView->append(line);
}

QString MainWindow::statusToText(DeviceStatus status) const
{
    switch (status)
    {
    case DeviceStatus::Disconnected:
        return "Disconnected";
    case DeviceStatus::Connecting:
        return "Connecting";
    case DeviceStatus::Connected:
        return "Connected";
    case DeviceStatus::Error:
        return "Error";
    default:
        return "Unknown";
    }
}
} // namespace HMVision
