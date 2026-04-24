#include "UI/CameraPage.h"

#include "Device/CameraManager.h"

#include <QDateTime>
#include <QFile>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include <variant>

namespace HMVision
{
namespace
{
QString typeToLabel(CameraType t)
{
    switch (t)
    {
    case CameraType::HIKVISION_GIGE:
        return "Hikvision GigE";
    case CameraType::HIKVISION_USB:
        return "Hikvision USB";
    case CameraType::OPENCV_USB:
        return "OpenCV USB";
    case CameraType::OPENCV_GIGE:
        return "OpenCV GigE (sim)";
    case CameraType::VIRTUAL:
        return "Virtual";
    default:
        return "Unknown";
    }
}

CameraType labelToType(const QString& label)
{
    if (label == "Hikvision GigE" || label == "HIKVISION_GIGE")
    {
        return CameraType::HIKVISION_GIGE;
    }
    if (label == "Hikvision USB" || label == "HIKVISION_USB")
    {
        return CameraType::HIKVISION_USB;
    }
    if (label == "OpenCV USB" || label == "OPENCV_USB")
    {
        return CameraType::OPENCV_USB;
    }
    if (label == "OpenCV GigE (sim)" || label == "OPENCV_GIGE")
    {
        return CameraType::OPENCV_GIGE;
    }
    if (label == "Virtual" || label == "VIRTUAL")
    {
        return CameraType::VIRTUAL;
    }
    return CameraType::UNKNOWN;
}
} // namespace

QImage CameraPage::matToQImage(const cv::Mat& mat)
{
    if (mat.empty())
    {
        return {};
    }
    cv::Mat rgb;
    if (mat.channels() == 1)
    {
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    }
    else if (mat.channels() == 3)
    {
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    }
    else if (mat.channels() == 4)
    {
        cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGB);
    }
    else
    {
        return {};
    }
    return QImage(
               rgb.data,
               rgb.cols,
               rgb.rows,
               static_cast<int>(rgb.step),
               QImage::Format_RGB888)
        .copy();
}

CameraConfig CameraPage::buildConfigFromUi() const
{
    CameraConfig c;
    c.id = m_idEdit->text().toStdString();
    c.name = m_nameEdit->text().isEmpty() ? c.id : m_nameEdit->text().toStdString();
    c.type = labelToType(m_typeCombo->currentText());
    c.connectionString = m_connectionEdit->text().toStdString();
    c.deviceIndex = m_deviceIndexSpin->value();
    c.exposureTime = m_exposureSpin->value();
    c.gain = m_gainSpin->value();
    c.frameRate = m_fpsSpin->value();
    c.width = m_widthSpin->value();
    c.height = m_heightSpin->value();
    c.triggerMode = m_triggerCheck->isChecked();
    c.triggerSource = m_triggerSourceSpin->value();
    c.triggerDelay = m_triggerDelaySpin->value();
    c.customParams["packet_size"] = 1500;
    return c;
}

CameraPage::CameraPage(
    MultiCameraView* preview,
    QPlainTextEdit* operationLogSink,
    QPlainTextEdit* alarmLogSink,
    QWidget* parent)
    : QWidget(parent)
    , m_sharedOpLog(operationLogSink)
    , m_sharedAlarmLog(alarmLogSink)
{
    if (preview != nullptr)
    {
        m_multiView = preview;
    }
    else
    {
        m_multiView = new MultiCameraView(this);
        m_ownsMultiView = true;
    }

    auto* root = new QSplitter(Qt::Horizontal, this);
    auto* left = new QWidget(root);
    auto* leftLay = new QVBoxLayout(left);

    m_list = new QListWidget(left);
    m_infoLog = new QPlainTextEdit(left);
    m_infoLog->setReadOnly(true);
    m_infoLog->setMaximumBlockCount(200);
    m_infoLog->setPlaceholderText("Camera info / selection...");

    auto* addRow = new QHBoxLayout();
    m_addConnectBtn = new QPushButton("Add and connect", left);
    m_removeBtn = new QPushButton("Remove", left);
    m_refreshBtn = new QPushButton("Refresh", left);
    addRow->addWidget(m_addConnectBtn);
    addRow->addWidget(m_removeBtn);
    addRow->addWidget(m_refreshBtn);

    auto* streamRow = new QHBoxLayout();
    m_startBtn = new QPushButton("Start grab", left);
    m_stopBtn = new QPushButton("Stop grab", left);
    m_saveCfgBtn = new QPushButton("Save json", left);
    m_loadCfgBtn = new QPushButton("Load json", left);
    streamRow->addWidget(m_startBtn);
    streamRow->addWidget(m_stopBtn);
    streamRow->addWidget(m_saveCfgBtn);
    streamRow->addWidget(m_loadCfgBtn);

    leftLay->addWidget(new QLabel("Camera list (connected)", left));
    leftLay->addWidget(m_list, 1);
    leftLay->addLayout(addRow);
    leftLay->addLayout(streamRow);
    leftLay->addWidget(m_infoLog, 1);

    auto* right = new QTabWidget(root);

    // Tab: new camera form
    auto* newCamTab = new QWidget;
    auto* nForm = new QFormLayout(newCamTab);
    m_idEdit = new QLineEdit(newCamTab);
    m_idEdit->setPlaceholderText("camera_1");
    m_nameEdit = new QLineEdit(newCamTab);
    m_typeCombo = new QComboBox(newCamTab);
    m_typeCombo->addItems(
        { "Hikvision GigE", "Hikvision USB", "OpenCV USB", "OpenCV GigE (sim)", "Virtual" });
    m_connectionEdit = new QLineEdit(newCamTab);
    m_connectionEdit->setPlaceholderText(
        "GigE: 与 MVS 中 IP 或序列号完全一致 (例 169.254.140.44)；USB/虚拟可空");
    m_deviceIndexSpin = new QSpinBox(newCamTab);
    m_deviceIndexSpin->setRange(0, 15);
    m_deviceIndexSpin->setValue(0);
    nForm->addRow("ID *", m_idEdit);
    nForm->addRow("Name", m_nameEdit);
    nForm->addRow("Type *", m_typeCombo);
    nForm->addRow("IP / sn / conn.", m_connectionEdit);
    nForm->addRow("OpenCV index", m_deviceIndexSpin);
    right->addTab(newCamTab, "New / identity");

    // Parameters tab
    auto* paramTab = new QWidget;
    auto* pLayout = new QVBoxLayout(paramTab);
    auto* basic = new QGroupBox("Basic", paramTab);
    auto* basicForm = new QFormLayout(basic);
    m_exposureSpin = new QDoubleSpinBox(basic);
    m_exposureSpin->setRange(0, 1e6);
    m_exposureSpin->setDecimals(1);
    m_exposureSpin->setValue(10000);
    m_gainSpin = new QDoubleSpinBox(basic);
    m_gainSpin->setRange(0, 100);
    m_gainSpin->setDecimals(2);
    m_fpsSpin = new QDoubleSpinBox(basic);
    m_fpsSpin->setRange(0.1, 500);
    m_fpsSpin->setValue(30);
    m_widthSpin = new QSpinBox(basic);
    m_widthSpin->setRange(32, 10000);
    m_widthSpin->setValue(1920);
    m_heightSpin = new QSpinBox(basic);
    m_heightSpin->setRange(32, 10000);
    m_heightSpin->setValue(1080);
    basicForm->addRow("Exposure (us)", m_exposureSpin);
    basicForm->addRow("Gain", m_gainSpin);
    basicForm->addRow("Frame rate (Hz)", m_fpsSpin);
    basicForm->addRow("Width", m_widthSpin);
    basicForm->addRow("Height", m_heightSpin);
    pLayout->addWidget(basic);
    auto* trigger = new QGroupBox("Trigger", paramTab);
    auto* trForm = new QFormLayout(trigger);
    m_triggerCheck = new QCheckBox(trigger);
    m_triggerSourceSpin = new QSpinBox(trigger);
    m_triggerSourceSpin->setRange(0, 16);
    m_triggerDelaySpin = new QSpinBox(trigger);
    m_triggerDelaySpin->setRange(0, 1e6);
    trForm->addRow("Trigger mode", m_triggerCheck);
    trForm->addRow("Trigger source", m_triggerSourceSpin);
    trForm->addRow("Trigger delay (us)", m_triggerDelaySpin);
    pLayout->addWidget(trigger);
    m_applyBtn = new QPushButton("Apply to selected", paramTab);
    pLayout->addWidget(m_applyBtn);
    pLayout->addStretch(1);
    right->addTab(paramTab, "Parameters");

    // Image: preview 在主界面「主界面」页；此处仅说明
    auto* viewTab = new QWidget;
    auto* vLay = new QVBoxLayout(viewTab);
    auto* imgHint = new QLabel(
        viewTab);
    imgHint->setWordWrap(true);
    imgHint->setText(
        "多路实时画面在窗口顶部「主界面」标签页中显示（2 列网格）。\n"
        "本页只负责设备列表、参数与连接。");
    vLay->addWidget(imgHint);
    vLay->addStretch(1);
    right->addTab(viewTab, "Image");

    // Status
    auto* stTab = new QWidget;
    auto* stLayout = new QVBoxLayout(stTab);
    m_statusLog = new QPlainTextEdit(stTab);
    m_statusLog->setReadOnly(true);
    m_statusLog->setMaximumBlockCount(500);
    m_clearLogBtn = new QPushButton("Clear log", stTab);
    stLayout->addWidget(m_statusLog, 1);
    stLayout->addWidget(m_clearLogBtn);
    right->addTab(stTab, "Status");

    root->addWidget(left);
    root->addWidget(right);
    root->setStretchFactor(0, 1);
    root->setStretchFactor(1, 2);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->addWidget(root);

    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(33);
    connect(m_frameTimer, &QTimer::timeout, this, &CameraPage::onPollFrames);
    m_frameTimer->start();

    connect(m_addConnectBtn, &QPushButton::clicked, this, &CameraPage::onAddAndConnect);
    connect(m_removeBtn, &QPushButton::clicked, this, &CameraPage::onDisconnectRemove);
    connect(m_startBtn, &QPushButton::clicked, this, &CameraPage::onStartGrab);
    connect(m_stopBtn, &QPushButton::clicked, this, &CameraPage::onStopGrab);
    connect(m_applyBtn, &QPushButton::clicked, this, &CameraPage::onApplyParameters);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CameraPage::onRefreshList);
    connect(m_saveCfgBtn, &QPushButton::clicked, this, &CameraPage::onSaveConfig);
    connect(m_loadCfgBtn, &QPushButton::clicked, this, &CameraPage::onLoadConfig);
    connect(m_clearLogBtn, &QPushButton::clicked, this, &CameraPage::onClearLog);
    connect(
        m_list, &QListWidget::itemSelectionChanged, this, &CameraPage::onSelectionChanged);

    onRefreshList();
    appendLog("Camera page ready. Add && connect, then Start grab. Hik MVS: link SDK for real device.");
}

void CameraPage::appendLog(const QString& line)
{
    const QString ts
        = QDateTime::currentDateTime().toString("hh:mm:ss ");
    if (m_statusLog)
    {
        m_statusLog->appendPlainText(ts + line);
    }
    if (m_sharedOpLog)
    {
        m_sharedOpLog->appendPlainText(
            QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ") + line);
    }
}

void CameraPage::onClearLog()
{
    m_statusLog->clear();
}

void CameraPage::onRefreshList()
{
    m_list->clear();
    const std::vector<std::string> ids
        = CameraManager::getInstance().getCameraList();
    for (const auto& id : ids)
    {
        m_list->addItem(QString::fromStdString(id));
    }
    appendLog(
        QString("List refreshed (%1)").arg(static_cast<int>(ids.size())));
}

void CameraPage::onAddAndConnect()
{
    const CameraConfig c = buildConfigFromUi();
    if (c.id.empty())
    {
        QMessageBox::warning(this, "Camera", "Please set a camera id.");
        return;
    }
    if (!CameraManager::getInstance().addCamera(c))
    {
        const QString err = QString::fromStdString(
            CameraManager::getInstance().getCameraError(c.id));
        QMessageBox::warning(this, "Camera", "addCamera failed: " + err);
        appendLog("addCamera failed: " + err);
        if (m_sharedAlarmLog)
        {
            m_sharedAlarmLog->appendPlainText(
                QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss] ")
                + "addCamera: " + err);
        }
        return;
    }
    m_multiView->addCamera(c.id);
    onRefreshList();
    appendLog(QString("Connected: %1").arg(QString::fromStdString(c.id)));
    emit camerasChanged();
}

void CameraPage::onDisconnectRemove()
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
    {
        return;
    }
    const std::string id = items.front()->text().toStdString();
    CameraManager::getInstance().stopCamera(id);
    CameraManager::getInstance().removeCamera(id);
    m_multiView->removeCamera(id);
    onRefreshList();
    appendLog(QString("Removed: %1").arg(QString::fromStdString(id)));
    emit camerasChanged();
}

void CameraPage::onStartGrab()
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
    {
        QMessageBox::information(this, "Camera", "Select a camera in the list.");
        return;
    }
    for (QListWidgetItem* it : m_list->selectedItems())
    {
        const std::string id = it->text().toStdString();
        if (CameraManager::getInstance().startCamera(id))
        {
            appendLog(QString("Start grab: %1").arg(QString::fromStdString(id)));
        }
        else
        {
            appendLog(
                "start failed: "
                + QString::fromStdString(CameraManager::getInstance().getCameraError(id)));
        }
    }
}

void CameraPage::onStopGrab()
{
    for (QListWidgetItem* it : m_list->selectedItems())
    {
        const std::string id = it->text().toStdString();
        CameraManager::getInstance().stopCamera(id);
        appendLog(QString("Stop grab: %1").arg(QString::fromStdString(id)));
    }
}

void CameraPage::onApplyParameters()
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
    {
        return;
    }
    const std::string id = items.front()->text().toStdString();
    auto& m = CameraManager::getInstance();
    m.setCameraParameter(id, "exposure_time", m_exposureSpin->value());
    m.setCameraParameter(id, "gain", m_gainSpin->value());
    m.setCameraParameter(id, "width", m_widthSpin->value());
    m.setCameraParameter(id, "height", m_heightSpin->value());
    m.setCameraParameter(id, "frame_rate", m_fpsSpin->value());
    appendLog("Parameters applied: " + QString::fromStdString(id));
}

void CameraPage::onPollFrames()
{
    if (m_multiView == nullptr)
    {
        return;
    }
    for (int i = 0; i < m_list->count(); ++i)
    {
        const std::string id = m_list->item(i)->text().toStdString();
        CameraFrame f;
        if (CameraManager::getInstance().getLatestFrame(id, f) && !f.image.empty())
        {
            m_multiView->setPreviewForCamera(id, matToQImage(f.image));
        }
    }
}

void CameraPage::onSelectionChanged()
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
    {
        m_infoLog->clear();
        m_lastSelectedId.clear();
        return;
    }
    m_lastSelectedId = items.front()->text().toStdString();
    auto c = CameraManager::getInstance().getCamera(m_lastSelectedId);
    if (!c)
    {
        return;
    }
    const CameraInfo info = c->getCameraInfo();
    QString t;
    t += QString("ID: %1\n").arg(QString::fromStdString(info.id));
    t += QString("Name: %1\n").arg(QString::fromStdString(info.name));
    t += "Type: " + typeToLabel(info.type) + "\n";
    t += QString("IP: %1\n").arg(QString::fromStdString(info.ipAddress));
    t += QString("Status: %1\n").arg(static_cast<int>(info.status));
    t += "Error: " + QString::fromStdString(info.lastError) + "\n";
    m_infoLog->setPlainText(t);
    syncParametersFromCamera();
}

void CameraPage::syncParametersFromCamera()
{
    if (m_lastSelectedId.empty())
    {
        return;
    }
    auto& m = CameraManager::getInstance();
    const CameraParam e = m.getCameraParameter(m_lastSelectedId, "exposure_time");
    const CameraParam g = m.getCameraParameter(m_lastSelectedId, "gain");
    if (const double* p = std::get_if<double>(&e))
    {
        m_exposureSpin->setValue(*p);
    }
    if (const double* p2 = std::get_if<double>(&g))
    {
        m_gainSpin->setValue(*p2);
    }
}

void CameraPage::onSaveConfig()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Save cameras json", "cameras.json", "JSON (*.json);;All (*.*)");
    if (path.isEmpty())
    {
        return;
    }
    QJsonArray arr;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const std::string id = m_list->item(i)->text().toStdString();
        auto cam = CameraManager::getInstance().getCamera(id);
        if (!cam)
        {
            continue;
        }
        const CameraConfig c = cam->getCameraConfig();
        QJsonObject o;
        o["id"] = QString::fromStdString(c.id);
        o["name"] = QString::fromStdString(c.name);
        auto typeStr = [](CameraType t) -> QString {
            switch (t)
            {
            case CameraType::HIKVISION_GIGE:
                return "HIKVISION_GIGE";
            case CameraType::HIKVISION_USB:
                return "HIKVISION_USB";
            case CameraType::OPENCV_USB:
                return "OPENCV_USB";
            case CameraType::OPENCV_GIGE:
                return "OPENCV_GIGE";
            case CameraType::VIRTUAL:
                return "VIRTUAL";
            default:
                return "UNKNOWN";
            }
        };
        o["type"] = typeStr(c.type);
        QJsonObject conn;
        conn["ip_address"] = QString::fromStdString(c.connectionString);
        conn["device_index"] = c.deviceIndex;
        o["connection"] = conn;
        QJsonObject p;
        p["width"] = c.width;
        p["height"] = c.height;
        p["frame_rate"] = c.frameRate;
        p["exposure_time"] = c.exposureTime;
        p["gain"] = c.gain;
        p["trigger_mode"] = c.triggerMode;
        p["trigger_source"] = c.triggerSource;
        p["trigger_delay"] = c.triggerDelay;
        o["parameters"] = p;
        arr.append(o);
    }
    QJsonObject root;
    root["cameras"] = arr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "Save", f.errorString());
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendLog("Config saved: " + path);
}

void CameraPage::onLoadConfig()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Load cameras json", QString(), "JSON (*.json);;All (*.*)");
    if (path.isEmpty() || !QFile::exists(path))
    {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        return;
    }
    const QJsonArray arr = doc.object()["cameras"].toArray();
    for (const QJsonValue& v : arr)
    {
        if (!v.isObject())
        {
            continue;
        }
        const QJsonObject o = v.toObject();
        CameraConfig c;
        c.id = o["id"].toString().toStdString();
        c.name = o["name"].toString().toStdString();
        c.type = labelToType(o["type"].toString().trimmed());
        const QJsonObject co = o["connection"].toObject();
        c.connectionString = co["ip_address"].toString().toStdString();
        c.deviceIndex = co["device_index"].toInt(0);
        const QJsonObject p = o["parameters"].toObject();
        c.width = p["width"].toInt(1920);
        c.height = p["height"].toInt(1080);
        c.frameRate = p["frame_rate"].toDouble(30.0);
        c.exposureTime = p["exposure_time"].toDouble(10000.0);
        c.gain = p["gain"].toDouble(0.0);
        c.triggerMode = p["trigger_mode"].toBool(false);
        c.triggerSource = p["trigger_source"].toInt(0);
        c.triggerDelay = p["trigger_delay"].toInt(0);
        if (c.id.empty())
        {
            continue;
        }
        if (CameraManager::getInstance().addCamera(c))
        {
            m_multiView->addCamera(c.id);
        }
    }
    onRefreshList();
    appendLog("Config loaded: " + path);
    emit camerasChanged();
}
} // namespace HMVision
