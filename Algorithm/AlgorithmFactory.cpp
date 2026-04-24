#include "AlgorithmFactory.h"
#include "DetectionAlgorithm.h"
#include "HalconAlgorithm.h"
#include "OCRProcessor.h"
#include "YOLODetector.h"

namespace HMVision
{
std::shared_ptr<IVisionAlgorithm> AlgorithmFactory::create(AlgorithmType type)
{
    switch (type)
    {
    case AlgorithmType::Detection:
        return std::make_shared<DetectionAlgorithm>();
    case AlgorithmType::OCR:
        return std::make_shared<OCRProcessor>();
    case AlgorithmType::Halcon:
        return std::make_shared<HalconAlgorithm>();
    default:
        return nullptr;
    }
}

std::shared_ptr<IVisionAlgorithm> AlgorithmFactory::create(const QString& typeName)
{
    const QString normalized = typeName.trimmed().toLower();
    if (normalized == "detection")
    {
        return create(AlgorithmType::Detection);
    }
    if (normalized == "yolov8" || normalized == "yolov26" || normalized == "yolodetector")
    {
        return std::make_shared<YOLODetector>();
    }
    if (normalized == "ocr" || normalized == "paddleocr" || normalized == "text")
    {
        return create(AlgorithmType::OCR);
    }
    if (normalized == "halcon" || normalized == "traditional")
    {
        return create(AlgorithmType::Halcon);
    }

    return nullptr;
}
} // namespace HMVision
