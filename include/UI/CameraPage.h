#pragma once

#include "UI/MultiCameraView.h"

#include "Device/ICamera.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <string>

namespace HMVision
{
class CameraPage : public QWidget
{
    Q_OBJECT
public:
    /// \p preview 与主界面共用；若为空则内部创建（仅调试）
    explicit CameraPage(
        MultiCameraView* preview,
        QPlainTextEdit* operationLogSink,
        QPlainTextEdit* alarmLogSink,
        QWidget* parent = nullptr);
signals:
    void camerasChanged();

private slots:
    void onRefreshList();
    void onAddAndConnect();
    void onDisconnectRemove();
    void onStartGrab();
    void onStopGrab();
    void onApplyParameters();
    void onPollFrames();
    void onClearLog();
    void onSaveConfig();
    void onLoadConfig();
    void onSelectionChanged();

private:
    static QImage matToQImage(const cv::Mat& mat);
    CameraConfig buildConfigFromUi() const;
    void appendLog(const QString& line);
    void syncParametersFromCamera();

    MultiCameraView* m_multiView = nullptr;
    bool m_ownsMultiView{false};
    QPlainTextEdit* m_sharedOpLog = nullptr;
    QPlainTextEdit* m_sharedAlarmLog = nullptr;
    QListWidget* m_list = nullptr;
    QPlainTextEdit* m_infoLog = nullptr;
    QPlainTextEdit* m_statusLog = nullptr;

    QLineEdit* m_idEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_connectionEdit = nullptr;
    QSpinBox* m_deviceIndexSpin = nullptr;

    QDoubleSpinBox* m_exposureSpin = nullptr;
    QDoubleSpinBox* m_gainSpin = nullptr;
    QDoubleSpinBox* m_fpsSpin = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QCheckBox* m_triggerCheck = nullptr;
    QSpinBox* m_triggerSourceSpin = nullptr;
    QSpinBox* m_triggerDelaySpin = nullptr;

    QPushButton* m_addConnectBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_saveCfgBtn = nullptr;
    QPushButton* m_loadCfgBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_clearLogBtn = nullptr;

    QTimer* m_frameTimer = nullptr;
    std::string m_lastSelectedId;
};
} // namespace HMVision
