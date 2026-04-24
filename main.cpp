#include <QApplication>

#include "Algorithm/AlgorithmFactory.h"
#include "Algorithm/DetectionAlgorithm.h"
#include "Algorithm/HalconAlgorithm.h"
#include "Algorithm/OCRProcessor.h"
#include "Device/CameraFactory.h"
#include "Device/GigE_Camera.h"
#include "Device/HikGigECamera.h"
#include "Device/HikUSBCamera.h"
#include "Device/ICamera.h"
#include "Device/USB_Camera.h"
#include "Device/VirtualCamera.h"
#include "UI/MainWindow.h"

static void registerVisionComponentFactories()
{
    auto& cameras = HMVision::CameraFactory::getInstance();
    cameras.registerCamera(
        HMVision::CameraType::OPENCV_USB,
        [](const HMVision::CameraConfig& c) { return std::make_shared<HMVision::USB_Camera>(c); });
    cameras.registerCamera(
        HMVision::CameraType::OPENCV_GIGE,
        [](const HMVision::CameraConfig& c) { return std::make_shared<HMVision::GigE_Camera>(c); });
    cameras.registerCamera(
        HMVision::CameraType::HIKVISION_GIGE,
        [](const HMVision::CameraConfig& c) { return std::make_shared<HMVision::HikGigECamera>(c); });
    cameras.registerCamera(
        HMVision::CameraType::HIKVISION_USB,
        [](const HMVision::CameraConfig& c) { return std::make_shared<HMVision::HikUSBCamera>(c); });
    cameras.registerCamera(
        HMVision::CameraType::VIRTUAL,
        [](const HMVision::CameraConfig& c) { return std::make_shared<HMVision::VirtualCamera>(c); });

    auto& algorithms = HMVision::AlgorithmFactory::getInstance();
    algorithms.registerCreator(
        HMVision::AlgorithmType::Detection,
        [] { return std::make_shared<HMVision::DetectionAlgorithm>(); });
    algorithms.registerCreator(
        HMVision::AlgorithmType::OCR, [] { return std::make_shared<HMVision::OCRProcessor>(); });
    algorithms.registerCreator(
        HMVision::AlgorithmType::Traditional,
        [] { return std::make_shared<HMVision::HalconAlgorithm>(); });
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("HM_VISION");
    app.setOrganizationName("HMVision");

    registerVisionComponentFactories();

    HMVision::MainWindow window;
    window.show();

    return app.exec();
}
