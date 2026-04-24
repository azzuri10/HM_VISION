#pragma once

#include "IVisionAlgorithm.h"

#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <string>

namespace HMVision
{
enum class AlgorithmType
{
    Unknown = 0,
    Detection,
    OCR,
    Classification,
    Traditional
};

class AlgorithmFactory
{
public:
    using Creator = std::function<std::shared_ptr<IVisionAlgorithm>()>;

    static AlgorithmFactory& getInstance();

    void registerCreator(AlgorithmType type, Creator creator);
    void registerCreator(const std::string& typeName, Creator creator);

    std::shared_ptr<IVisionAlgorithm> create(AlgorithmType type) const;
    std::shared_ptr<IVisionAlgorithm> create(const std::string& typeName) const;

private:
    AlgorithmFactory() = default;
    AlgorithmFactory(const AlgorithmFactory&) = delete;
    AlgorithmFactory& operator=(const AlgorithmFactory&) = delete;

    static std::string normalize(const std::string& value);

    mutable std::mutex m_mutex;
    std::map<AlgorithmType, Creator> m_typeCreators;
    std::map<std::string, Creator> m_nameCreators;
};
} // namespace HMVision
