#pragma once

#include "UI/MultiCameraView.h"

#include <QWidget>

class QGroupBox;
class QPlainTextEdit;
class QSplitter;
class QLabel;

namespace HMVision
{
/// 主界面：多路实时图（默认 2 列排布）+ 右侧报警 / 操作日志（参考工业视觉 HMI 布局）
class MainInterfaceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MainInterfaceWidget(QWidget* parent = nullptr);

    MultiCameraView* multiCameraView() const
    {
        return m_multiView;
    }
    QPlainTextEdit* operationLogWidget() const
    {
        return m_operationLog;
    }
    QPlainTextEdit* alarmLogWidget() const
    {
        return m_alarmLog;
    }

    void appendOperationLog(const QString& line);
    void appendAlarmLog(const QString& line);
    void setStatusLine(const QString& text);

public slots:
    void onCameraManagerCountChanged();

private:
    void setupUi();

    MultiCameraView* m_multiView = nullptr;
    QPlainTextEdit* m_operationLog = nullptr;
    QPlainTextEdit* m_alarmLog = nullptr;
    QLabel* m_statusLine = nullptr;
    QSplitter* m_splitter = nullptr;
};
} // namespace HMVision
