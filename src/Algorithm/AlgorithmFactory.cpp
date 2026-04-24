#include "../../include/Algorithm/AlgorithmFactory.h"

#include <algorithm>
#include <cctype>

namespace HMVision
{
AlgorithmFactory& AlgorithmFactory::getInstance()
{
    static AlgorithmFactory factory;
    return factory;
}

void AlgorithmFactory::registerCreator(AlgorithmType type, Creator creator)
{
    if (!creator)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_typeCreators[type] = std::move(creator);
}

void AlgorithmFactory::registerCreator(const std::string& typeName, Creator creator)
{
    if (!creator || typeName.empty())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_nameCreators[normalize(typeName)] = std::move(creator);
}

std::shared_ptr<IVisionAlgorithm> AlgorithmFactory::create(AlgorithmType type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_typeCreators.find(type);
    if (it == m_typeCreators.end() || !it->second)
    {
        return nullptr;
    }
    return it->second();
}

std::shared_ptr<IVisionAlgorithm> AlgorithmFactory::create(const std::string& typeName) const
{
    if (typeName.empty())
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_nameCreators.find(normalize(typeName));
    if (it == m_nameCreators.end() || !it->second)
    {
        return nullptr;
    }
    return it->second();
}

std::string AlgorithmFactory::normalize(const std::string& value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}
} // namespace HMVision
