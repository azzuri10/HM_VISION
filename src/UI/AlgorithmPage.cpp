#include "../../include/UI/AlgorithmPage.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace HMVision
{
AlgorithmPage::AlgorithmPage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QHBoxLayout(this);

    // 1) Algorithm list management
    auto* leftPanel = new QGroupBox("算法列表管理", this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    m_algorithmList = new QListWidget(leftPanel);
    m_algorithmList->addItems({"YOLOv8", "YOLOv26", "PaddleOCR", "Halcon"});
    m_algorithmList->setCurrentRow(0);
    leftLayout->addWidget(m_algorithmList);
    root->addWidget(leftPanel, 1);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);

    // 2) Model file selection + 3) Real-time parameter tuning
    auto* configBox = new QGroupBox("模型与参数设置", rightPanel);
    auto* configLayout = new QFormLayout(configBox);
    m_modelPathEdit = new QLineEdit(configBox);
    m_chooseModelButton = new QPushButton("选择模型文件...", configBox);
    auto* modelRow = new QHBoxLayout();
    modelRow->addWidget(m_modelPathEdit, 1);
    modelRow->addWidget(m_chooseModelButton);
    configLayout->addRow("模型文件", modelRow);

    m_confSpin = new QDoubleSpinBox(configBox);
    m_confSpin->setRange(0.0, 1.0);
    m_confSpin->setSingleStep(0.01);
    m_confSpin->setValue(0.25);
    configLayout->addRow("置信度阈值", m_confSpin);

    m_nmsSpin = new QDoubleSpinBox(configBox);
    m_nmsSpin->setRange(0.0, 1.0);
    m_nmsSpin->setSingleStep(0.01);
    m_nmsSpin->setValue(0.45);
    configLayout->addRow("NMS阈值", m_nmsSpin);

    m_deviceCombo = new QComboBox(configBox);
    m_deviceCombo->addItems({"cpu", "cuda"});
    configLayout->addRow("推理设备", m_deviceCombo);
    rightLayout->addWidget(configBox);

    // 4) Algorithm test feature
    auto* testBox = new QGroupBox("算法测试", rightPanel);
    auto* testLayout = new QVBoxLayout(testBox);
    m_testButton = new QPushButton("执行测试", testBox);
    m_statusLabel = new QLabel("待测试", testBox);
    testLayout->addWidget(m_testButton);
    testLayout->addWidget(m_statusLabel);
    rightLayout->addWidget(testBox);

    // 5) Performance monitor display
    auto* perfBox = new QGroupBox("性能监控显示", rightPanel);
    auto* perfLayout = new QVBoxLayout(perfBox);
    m_perfTable = new QTableWidget(perfBox);
    m_perfTable->setColumnCount(4);
    m_perfTable->setHorizontalHeaderLabels({"Time", "Algorithm", "Latency(ms)", "FPS"});
    m_perfTable->horizontalHeader()->setStretchLastSection(true);
    perfLayout->addWidget(m_perfTable);
    rightLayout->addWidget(perfBox, 1);

    root->addWidget(rightPanel, 2);

    connect(m_chooseModelButton, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, "选择模型文件", QString(), "Model Files (*.onnx *.engine *.bin);;All Files (*.*)");
        if (!file.isEmpty())
        {
            m_modelPathEdit->setText(file);
        }
    });

    connect(m_algorithmList, &QListWidget::currentTextChanged, this, [this](const QString&) {
        refreshAlgorithmDetail();
    });

    connect(m_testButton, &QPushButton::clicked, this, [this]() {
        const QString algo = m_algorithmList->currentItem() ? m_algorithmList->currentItem()->text() : "N/A";
        const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        const int row = m_perfTable->rowCount();
        m_perfTable->insertRow(row);
        m_perfTable->setItem(row, 0, new QTableWidgetItem(now));
        m_perfTable->setItem(row, 1, new QTableWidgetItem(algo));
        m_perfTable->setItem(row, 2, new QTableWidgetItem(QString::number(12 + (row % 8))));
        m_perfTable->setItem(row, 3, new QTableWidgetItem(QString::number(80 - (row % 15))));
        m_statusLabel->setText(QString("测试完成: %1").arg(algo));
    });

    refreshAlgorithmDetail();
}

void AlgorithmPage::refreshAlgorithmDetail()
{
    const QString algo = m_algorithmList->currentItem() ? m_algorithmList->currentItem()->text() : "";
    if (algo.contains("YOLO"))
    {
        m_modelPathEdit->setPlaceholderText("选择 YOLO 模型（.onnx/.engine）");
    }
    else if (algo.contains("OCR"))
    {
        m_modelPathEdit->setPlaceholderText("选择 OCR 模型目录/文件");
    }
    else
    {
        m_modelPathEdit->setPlaceholderText("选择传统算法配置文件");
    }
}
} // namespace HMVision
