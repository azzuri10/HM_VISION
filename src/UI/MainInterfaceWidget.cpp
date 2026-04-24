#include "UI/MainInterfaceWidget.h"
#include "Device/CameraManager.h"

#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>

namespace HMVision
{
MainInterfaceWidget::MainInterfaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    onCameraManagerCountChanged();
}

void MainInterfaceWidget::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_multiView = new MultiCameraView(m_splitter);
    m_multiView->setMinimumSize(480, 360);
    m_multiView->setGridColumns(2);

    auto* right = new QWidget(m_splitter);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);

    auto* alarmBox = new QGroupBox("报警信息", right);
    auto* abLay = new QVBoxLayout(alarmBox);
    m_alarmLog = new QPlainTextEdit(alarmBox);
    m_alarmLog->setReadOnly(true);
    m_alarmLog->setMaximumBlockCount(200);
    m_alarmLog->setPlaceholderText("异常 / 失败信息将显示在这里");
    abLay->addWidget(m_alarmLog);
    rightLay->addWidget(alarmBox, 1);

    auto* opBox = new QGroupBox("操作信息", right);
    auto* opLay = new QVBoxLayout(opBox);
    m_operationLog = new QPlainTextEdit(opBox);
    m_operationLog->setReadOnly(true);
    m_operationLog->setMaximumBlockCount(3000);
    m_operationLog->setPlaceholderText("系统与相机操作日志");
    opLay->addWidget(m_operationLog);
    rightLay->addWidget(opBox, 2);

    m_statusLine = new QLabel(this);
    m_statusLine->setStyleSheet("QLabel { padding: 4px; background: #2d2d2d; color: #e0e0e0; }");
    m_statusLine->setMinimumHeight(28);

    m_splitter->addWidget(m_multiView);
    m_splitter->addWidget(right);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->addWidget(m_splitter, 1);
    mainLay->addWidget(m_statusLine, 0);
    setLayout(mainLay);
}

void MainInterfaceWidget::appendOperationLog(const QString& line)
{
    if (!m_operationLog)
    {
        return;
    }
    const QString ts
        = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ");
    m_operationLog->appendPlainText(ts + line);
}

void MainInterfaceWidget::appendAlarmLog(const QString& line)
{
    if (!m_alarmLog)
    {
        return;
    }
    const QString ts
        = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ");
    m_alarmLog->appendPlainText(ts + line);
}

void MainInterfaceWidget::setStatusLine(const QString& text)
{
    if (m_statusLine)
    {
        m_statusLine->setText(text);
    }
}

void MainInterfaceWidget::onCameraManagerCountChanged()
{
    const int n
        = static_cast<int>(CameraManager::getInstance().getCameraList().size());
    setStatusLine(
        tr("已连接相机: %1 路  |  在「相机设置」中添加 / 开始采集，画面显示于上方网格")
            .arg(n));
}
} // namespace HMVision
