#include "../../include/UI/CameraPage.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace HMVision
{
CameraPage::CameraPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    m_multiCameraView = new MultiCameraView(this);
    layout->addWidget(m_multiCameraView, 1);

    auto* realtimeTitle = new QLabel("实时图像显示区域", this);
    layout->addWidget(realtimeTitle);

    m_cameraTable = new QTableWidget(this);
    m_cameraTable->setColumnCount(6);
    m_cameraTable->setHorizontalHeaderLabels({"ID", "Type", "Status", "Exposure", "Gain", "FPS"});
    m_cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_cameraTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cameraTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cameraTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_cameraTable);

    // Camera parameter setting panel
    auto* parameterPanel = new QWidget(this);
    auto* parameterForm = new QFormLayout(parameterPanel);
    auto* typeCombo = new QComboBox(parameterPanel);
    typeCombo->addItems({"USB", "GigE", "Virtual"});
    auto* exposureSpin = new QSpinBox(parameterPanel);
    exposureSpin->setRange(0, 100000);
    exposureSpin->setValue(1000);
    auto* gainSpin = new QSpinBox(parameterPanel);
    gainSpin->setRange(0, 100);
    gainSpin->setValue(1);
    auto* fpsSpin = new QSpinBox(parameterPanel);
    fpsSpin->setRange(1, 240);
    fpsSpin->setValue(30);
    auto* ipEdit = new QLineEdit(parameterPanel);
    ipEdit->setPlaceholderText("192.168.1.10");
    parameterForm->addRow("Camera Type", typeCombo);
    parameterForm->addRow("Exposure", exposureSpin);
    parameterForm->addRow("Gain", gainSpin);
    parameterForm->addRow("FPS", fpsSpin);
    parameterForm->addRow("IP (GigE)", ipEdit);
    layout->addWidget(parameterPanel);

    auto* row = new QHBoxLayout();
    m_addMockButton = new QPushButton("添加相机", this);
    m_removeSelectedButton = new QPushButton("删除相机", this);
    auto* applyParamButton = new QPushButton("应用参数", this);
    auto* startButton = new QPushButton("开始采集", this);
    auto* stopButton = new QPushButton("停止采集", this);
    auto* triggerButton = new QPushButton("软触发", this);
    row->addWidget(m_addMockButton);
    row->addWidget(m_removeSelectedButton);
    row->addWidget(applyParamButton);
    row->addWidget(startButton);
    row->addWidget(stopButton);
    row->addWidget(triggerButton);
    row->addStretch(1);
    layout->addLayout(row);

    connect(m_addMockButton, &QPushButton::clicked, this, [this]() {
        const int rowIndex = m_cameraTable->rowCount();
        const QString id = QString("cam-%1").arg(rowIndex + 1);

        m_cameraTable->insertRow(rowIndex);
        m_cameraTable->setItem(rowIndex, 0, new QTableWidgetItem(id));
        m_cameraTable->setItem(rowIndex, 1, new QTableWidgetItem("Mock"));
        m_cameraTable->setItem(rowIndex, 2, new QTableWidgetItem("Disconnected"));
        m_cameraTable->setItem(rowIndex, 3, new QTableWidgetItem("1000"));
        m_cameraTable->setItem(rowIndex, 4, new QTableWidgetItem("1"));
        m_cameraTable->setItem(rowIndex, 5, new QTableWidgetItem("30"));

        m_multiCameraView->addCamera(id.toStdString());
    });

    connect(m_removeSelectedButton, &QPushButton::clicked, this, [this]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }

        const int row = selected.first()->row();
        const QString id = m_cameraTable->item(row, 0)->text();
        m_multiCameraView->removeCamera(id.toStdString());
        m_cameraTable->removeRow(row);
    });

    connect(m_cameraTable, &QTableWidget::itemSelectionChanged, this, [=]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const int row = selected.first()->row();
        typeCombo->setCurrentText(m_cameraTable->item(row, 1)->text());
        exposureSpin->setValue(m_cameraTable->item(row, 3)->text().toInt());
        gainSpin->setValue(m_cameraTable->item(row, 4)->text().toInt());
        fpsSpin->setValue(m_cameraTable->item(row, 5)->text().toInt());
    });

    connect(applyParamButton, &QPushButton::clicked, this, [=]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const int row = selected.first()->row();
        m_cameraTable->setItem(row, 1, new QTableWidgetItem(typeCombo->currentText()));
        m_cameraTable->setItem(row, 3, new QTableWidgetItem(QString::number(exposureSpin->value())));
        m_cameraTable->setItem(row, 4, new QTableWidgetItem(QString::number(gainSpin->value())));
        m_cameraTable->setItem(row, 5, new QTableWidgetItem(QString::number(fpsSpin->value())));
        Q_UNUSED(ipEdit);
    });

    connect(startButton, &QPushButton::clicked, this, [=]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const int row = selected.first()->row();
        m_cameraTable->setItem(row, 2, new QTableWidgetItem("Streaming"));
    });

    connect(stopButton, &QPushButton::clicked, this, [=]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const int row = selected.first()->row();
        m_cameraTable->setItem(row, 2, new QTableWidgetItem("Stopped"));
    });

    connect(triggerButton, &QPushButton::clicked, this, [=]() {
        const auto selected = m_cameraTable->selectedItems();
        if (selected.isEmpty())
        {
            return;
        }
        const int row = selected.first()->row();
        m_cameraTable->setItem(row, 2, new QTableWidgetItem("Triggered"));
    });
}
} // namespace HMVision
