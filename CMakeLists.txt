cmake_minimum_required(VERSION 3.20)

project(HM_VISION LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt 6.3+
find_package(Qt6 6.3 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

# OpenCV：先静默查找；找不到则仅编译 Qt 界面（避免 Qt Creator 无法完成 CMake 配置）
option(HMVISION_FORCE_MINIMAL "仅编译 Qt 界面，不链接 OpenCV（调试用）" OFF)

# 默认使用本机安装（可被 Qt Creator 中已设置的 OpenCV_DIR 覆盖）
set(HMVISION_OPENCV_ROOT "D:/opencv_install/opencv_4.10.0" CACHE PATH "OpenCV 根目录（内含 OpenCVConfig.cmake）")
if(NOT OpenCV_DIR AND EXISTS "${HMVISION_OPENCV_ROOT}/OpenCVConfig.cmake")
    set(OpenCV_DIR "${HMVISION_OPENCV_ROOT}" CACHE PATH "OpenCVConfig.cmake 所在目录")
endif()

find_package(OpenCV QUIET COMPONENTS core dnn imgproc videoio)

if(HMVISION_FORCE_MINIMAL)
    set(HMVISION_HAVE_OPENCV FALSE)
elseif(OpenCV_FOUND)
    set(HMVISION_HAVE_OPENCV TRUE)
else()
    message(STATUS "")
    message(STATUS "========== HM_VISION ==========")
    message(STATUS "未检测到 OpenCV，将使用【最小构建】：仅 Qt 主界面，可正常配置/运行。")
    message(STATUS "要使用相机/算法等完整功能，请在 Qt Creator：")
    message(STATUS "  项目 → CMake → 添加变量 OpenCV_DIR")
    message(STATUS "  指向含 OpenCVConfig.cmake 的目录，然后重新运行 CMake。")
    message(STATUS "================================")
    message(STATUS "")
    set(HMVISION_HAVE_OPENCV FALSE)
endif()

# nlohmann/json：完整构建需要；本机未安装时用 FetchContent 自动拉取（解决 C1083 找不到 json.hpp）
if(HMVISION_HAVE_OPENCV)
    find_package(nlohmann_json 3.11 QUIET CONFIG)
    if(NOT nlohmann_json_FOUND)
        message(STATUS "nlohmann_json 未在系统中找到，使用 FetchContent 获取（首次需联网）…")
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        set(JSON_Install OFF CACHE INTERNAL "")
        include(FetchContent)
        FetchContent_Declare(
            hmvision_nlohmann_json
            URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(hmvision_nlohmann_json)
    endif()
endif()

# ---------- 所有机器上都能通过配置的最小源（仅 Qt）----------
set(HMVISION_UI_SOURCES
    main.cpp
    include/UI/MainWindow.h
    src/UI/MainWindow.cpp
    include/UI/CameraPage.h
    src/UI/CameraPage.cpp
    include/UI/AlgorithmPage.h
    src/UI/AlgorithmPage.cpp
    include/UI/MultiCameraView.h
    include/UI/CameraWidget.h
)

# ---------- 完整功能源（依赖 OpenCV 等）----------
set(HMVISION_FULL_EXTRA_SOURCES
    Common/Types.h
    Core/Logger.h
    Core/Logger.cpp
    Core/ConfigManager.h
    Core/ConfigManager.cpp
    Core/VisionSystem.h
    Core/VisionSystem.cpp
    Device/ICamera.h
    Device/IPLCInterface.h
    Device/CameraManager.h
    Device/CameraManager.cpp
    Device/ModbusPLC.h
    Device/ModbusPLC.cpp
    Device/USB_Camera.h
    Device/USB_Camera.cpp
    Device/GigE_Camera.h
    Device/GigE_Camera.cpp
    Common/AlgorithmResult.h
    Common/AlgorithmParams.h
    Algorithm/IVisionAlgorithm.h
    Algorithm/AlgorithmFactory.h
    Algorithm/AlgorithmFactory.cpp
    Algorithm/AlgorithmScheduler.h
    Algorithm/AlgorithmScheduler.cpp
    Algorithm/DetectionAlgorithm.h
    Algorithm/DetectionAlgorithm.cpp
    Algorithm/HalconAlgorithm.h
    Algorithm/HalconAlgorithm.cpp
    Algorithm/YOLODetector.h
    Algorithm/YOLODetector.cpp
    Algorithm/OCRProcessor.h
    Algorithm/OCRProcessor.cpp
    Algorithm/ThreadPool.h
    Communication/MESClient.h
    Communication/MESClient.cpp
    Data/ResultRepository.h
    Data/ResultRepository.cpp
)

if(HMVISION_HAVE_OPENCV)
    set(HMVISION_SOURCES ${HMVISION_UI_SOURCES} ${HMVISION_FULL_EXTRA_SOURCES})
else()
    set(HMVISION_SOURCES ${HMVISION_UI_SOURCES})
endif()

add_executable(HM_VISION ${HMVISION_SOURCES})

target_include_directories(
    HM_VISION
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(HMVISION_HAVE_OPENCV)
    target_include_directories(HM_VISION PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(HM_VISION PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(HM_VISION PRIVATE HMVISION_HAVE_OPENCV=1)
else()
    target_compile_definitions(HM_VISION PRIVATE HMVISION_HAVE_OPENCV=0)
endif()

target_link_libraries(
    HM_VISION
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

if(HMVISION_HAVE_OPENCV)
    target_link_libraries(HM_VISION PRIVATE nlohmann_json::nlohmann_json)

    find_path(LIBMODBUS_INCLUDE_DIR modbus/modbus.h)
    find_library(LIBMODBUS_LIBRARY modbus)
    if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
        target_include_directories(HM_VISION PRIVATE ${LIBMODBUS_INCLUDE_DIR})
        target_link_libraries(HM_VISION PRIVATE ${LIBMODBUS_LIBRARY})
    endif()

    find_path(SNAP7_INCLUDE_DIR snap7.h)
    find_library(SNAP7_LIBRARY snap7)
    if(SNAP7_INCLUDE_DIR AND SNAP7_LIBRARY)
        target_include_directories(HM_VISION PRIVATE ${SNAP7_INCLUDE_DIR})
        target_link_libraries(HM_VISION PRIVATE ${SNAP7_LIBRARY})
    endif()

    find_path(PADDLEOCR_INCLUDE_DIR paddleocr.h)
    find_library(PADDLEOCR_LIB paddleocr)
    if(PADDLEOCR_INCLUDE_DIR AND PADDLEOCR_LIB)
        target_include_directories(HM_VISION PRIVATE ${PADDLEOCR_INCLUDE_DIR})
        target_link_libraries(HM_VISION PRIVATE ${PADDLEOCR_LIB})
    endif()

    find_path(TENSORRT_INCLUDE_DIR NvInfer.h)
    find_library(TENSORRT_NVINFER_LIB nvinfer)
    find_library(TENSORRT_NVONNXPARSER_LIB nvonnxparser)
    find_library(CUDART_LIB cudart)
    if(TENSORRT_INCLUDE_DIR AND TENSORRT_NVINFER_LIB AND CUDART_LIB)
        target_include_directories(HM_VISION PRIVATE ${TENSORRT_INCLUDE_DIR})
        target_link_libraries(HM_VISION PRIVATE ${TENSORRT_NVINFER_LIB} ${CUDART_LIB})
        if(TENSORRT_NVONNXPARSER_LIB)
            target_link_libraries(HM_VISION PRIVATE ${TENSORRT_NVONNXPARSER_LIB})
        endif()
    endif()

    find_path(HALCON_INCLUDE_DIR HalconCpp.h)
    find_library(HALCONCPP_LIB HalconCpp)
    if(HALCON_INCLUDE_DIR AND HALCONCPP_LIB)
        target_include_directories(HM_VISION PRIVATE ${HALCON_INCLUDE_DIR})
        target_link_libraries(HM_VISION PRIVATE ${HALCONCPP_LIB})
    endif()
endif()
