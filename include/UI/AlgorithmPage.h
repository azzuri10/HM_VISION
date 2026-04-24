#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace HMVision
{
class AlgorithmPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlgorithmPage(QWidget* parent = nullptr);

private:
    void refreshAlgorithmDetail();

    QListWidget* m_algorithmList = nullptr;
    QLineEdit* m_modelPathEdit = nullptr;
    QDoubleSpinBox* m_confSpin = nullptr;
    QDoubleSpinBox* m_nmsSpin = nullptr;
    QComboBox* m_deviceCombo = nullptr;
    QPushButton* m_chooseModelButton = nullptr;
    QPushButton* m_testButton = nullptr;
    QTableWidget* m_perfTable = nullptr;
    QLabel* m_statusLabel = nullptr;
};
} // namespace HMVision
