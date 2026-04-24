#pragma once

#include "Logger.h"

#define LOG_DEBUG(message) \
    ::HMVision::Logger::getInstance().log(::HMVision::Logger::Level::Debug, (message), __FILE__, __LINE__)

#define LOG_INFO(message) \
    ::HMVision::Logger::getInstance().log(::HMVision::Logger::Level::Info, (message), __FILE__, __LINE__)

#define LOG_WARNING(message) \
    ::HMVision::Logger::getInstance().log(::HMVision::Logger::Level::Warning, (message), __FILE__, __LINE__)

#define LOG_ERROR(message) \
    ::HMVision::Logger::getInstance().log(::HMVision::Logger::Level::Error, (message), __FILE__, __LINE__)

#define LOG_FATAL(message) \
    ::HMVision::Logger::getInstance().log(::HMVision::Logger::Level::Fatal, (message), __FILE__, __LINE__)
