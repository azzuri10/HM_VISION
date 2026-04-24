cmake_minimum_required(VERSION 3.20)

project(HM_VISION LANGUAGES CXX)

if(NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Default dependency roots (override in Qt Creator or -D on command line).
# All user-facing FATAL/STATUS use ASCII English to avoid mojibake in MSVC/Qt logs.
# ---------------------------------------------------------------------------
set(HMVISION_QT_ROOT "C:/Qt/6.5.3" CACHE PATH "Qt root (contains msvc*_64, Qt6Config.cmake)")
set(HMVISION_OPENCV_ROOT "D:/opencv_install/opencv_4.10.0" CACHE PATH "OpenCV root (OpenCVConfig.cmake or x64/.../lib)")
set(
    HMVISION_CUDA_ROOT
    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.6"
    CACHE PATH
    "CUDA Toolkit root (bin/nvcc.exe)"
)
set(
    HMVISION_TENSORRT_ROOT
    "D:/TensorRT-8.6.1.6"
    CACHE PATH
    "TensorRT root (include/NvInfer.h, lib/nvinfer.lib)"
)
set(HMVISION_PADDLE_ROOT "D:/paddle_inference_2.6" CACHE PATH "Paddle Inference root")
set(
    HMVISION_MVS_ROOT
    ""
    CACHE PATH
    "Optional. Extra MVS search root; standard locations (thirdparty/mvs, /opt/MVS/include, Program Files) are used first"
)
set(
    HMVISION_HALCON_ROOT
    "C:/Program Files/MVTec/HALCON-18.11-Progress"
    CACHE PATH
    "HALCON 18.11 root"
)

# Qt: if CMAKE_PREFIX_PATH is empty, try common kits under HMVISION_QT_ROOT
if(
    (NOT DEFINED CMAKE_PREFIX_PATH)
    OR ("x${CMAKE_PREFIX_PATH}" STREQUAL "x")
)
    set(_hmvision_qt_found_prefix "")
    foreach(
        _kit
        IN
        ITEMS
        msvc2019_64
        msvc2022_64
    )
        if(EXISTS "${HMVISION_QT_ROOT}/${_kit}/lib/cmake/Qt6/Qt6Config.cmake")
            set(_hmvision_qt_found_prefix "${HMVISION_QT_ROOT}/${_kit}")
            break()
        endif()
    endforeach()
    if(_hmvision_qt_found_prefix STREQUAL "")
        message(
            FATAL_ERROR
            "Could not find Qt6. Set CMAKE_PREFIX_PATH to your Qt6 prefix, e.g.:\n"
            "  ${HMVISION_QT_ROOT}/msvc2019_64\n"
            "or fix HMVISION_QT_ROOT: ${HMVISION_QT_ROOT}"
        )
    endif()
    set(
        CMAKE_PREFIX_PATH
        "${_hmvision_qt_found_prefix}"
        CACHE
        PATH
        "Qt6 prefix (from HMVISION_QT_ROOT autodetect)"
        FORCE
    )
endif()

find_package(
    Qt6
    6.3
    REQUIRED
    COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
)

# CUDA: required for this project (and for GPU OpenCV if OpenCV was built with CUDA)
if(NOT EXISTS "${HMVISION_CUDA_ROOT}/bin/nvcc.exe")
    message(
        FATAL_ERROR
        "CUDA Toolkit not found (need nvcc). Check HMVISION_CUDA_ROOT:\n"
        "  ${HMVISION_CUDA_ROOT}\n"
        "Expected: ${HMVISION_CUDA_ROOT}/bin/nvcc.exe"
    )
endif()

# If OpenCV was built with CUDA, FindCUDA in OpenCVConfig expects the same major.minor Toolkit.
# Set HMVISION_OPENCV_CUDA_ROOT if it differs from HMVISION_CUDA_ROOT. If empty, use HMVISION_CUDA_ROOT.
set(
    HMVISION_OPENCV_CUDA_ROOT
    ""
    CACHE
    PATH
    "CUDA root used when OpenCV was built (e.g. .../CUDA/v11.6). Leave empty to use HMVISION_CUDA_ROOT."
)
set(_hmvision_opencv_cuda_for_config "${HMVISION_OPENCV_CUDA_ROOT}")
if(_hmvision_opencv_cuda_for_config STREQUAL "" AND EXISTS "${HMVISION_CUDA_ROOT}/bin/nvcc.exe")
    set(_hmvision_opencv_cuda_for_config "${HMVISION_CUDA_ROOT}")
    message(
        STATUS
        "HM_VISION: using HMVISION_CUDA_ROOT for OpenCV CUDA check: ${_hmvision_opencv_cuda_for_config} (set HMVISION_OPENCV_CUDA_ROOT to override)"
    )
endif()
if(
    _hmvision_opencv_cuda_for_config
    AND EXISTS "${_hmvision_opencv_cuda_for_config}/bin/nvcc.exe"
)
    set(ENV{CUDA_PATH} "${_hmvision_opencv_cuda_for_config}")
    set(ENV{CUDA_PATH_V1_0} "${_hmvision_opencv_cuda_for_config}")
    set(ENV{CUDA_PATH_V1_1} "${_hmvision_opencv_cuda_for_config}")
    set(ENV{CUDA_BIN_PATH} "${_hmvision_opencv_cuda_for_config}/bin")
    set(
        CUDA_TOOLKIT_ROOT_DIR
        "${_hmvision_opencv_cuda_for_config}"
        CACHE
        PATH
        "CUDA Toolkit (OpenCV + this project)"
        FORCE
    )
    if(POLICY CMP0104)
        cmake_policy(SET CMP0104 NEW)
    endif()
    set(
        CUDAToolkit_ROOT
        "${_hmvision_opencv_cuda_for_config}"
        CACHE
        PATH
        "CUDAToolkit root (must match prebuilt OpenCV, e.g. 11.6)"
        FORCE
    )
    find_package(CUDAToolkit 11.6 REQUIRED MODULE)
    if(NOT CUDAToolkit_FOUND)
        message(
            FATAL_ERROR
            "CUDAToolkit not found at HMVISION path; OpenCV 4.10+CUDA needs Toolkit 11.6.\n"
            "  Set HMVISION_CUDA_ROOT (or HMVISION_OPENCV_CUDA_ROOT) to .../CUDA/v11.6"
        )
    endif()
    set(CUDA_FOUND TRUE)
    set(CUDA_VERSION "11.6" CACHE STRING "Must match prebuilt OpenCV (OpenCV_CUDA_VERSION)" FORCE)
    set(
        CUDA_VERSION_STRING
        "11.6"
        CACHE
        STRING
        "OpenCV 4.10 config compares VERSION_EQUAL to this (see OpenCVConfig.cmake)"
        FORCE
    )
    set(
        CUDA_TOOLKIT_ROOT_DIR
        "${CUDAToolkit_ROOT}"
        CACHE
        PATH
        "Legacy FindCUDA (OpenCV)"
        FORCE
    )
    if(DEFINED CUDAToolkit_INCLUDE_DIR)
        set(CUDA_INCLUDE_DIRS "${CUDAToolkit_INCLUDE_DIR}")
    else()
        set(CUDA_INCLUDE_DIRS "${CUDAToolkit_INCLUDE_DIRS}")
    endif()
    set(
        CUDA_NVCC_EXECUTABLE
        "${CUDAToolkit_ROOT}/bin/nvcc.exe"
        CACHE
        FILEPATH
        "nvcc"
        FORCE
    )
    if(TARGET CUDA::cudart)
        get_target_property(_hmvision_cudart_lib CUDA::cudart IMPORTED_IMPLIB_RELEASE)
        if(NOT _hmvision_cudart_lib)
            get_target_property(_hmvision_cudart_lib CUDA::cudart IMPORTED_IMPLIB)
        endif()
        if(_hmvision_cudart_lib)
            set(
                CUDA_CUDART_LIBRARY
                "${_hmvision_cudart_lib}"
                CACHE
                FILEPATH
                "cudart (from CUDAToolkit target)"
                FORCE
            )
        endif()
    endif()
    if(NOT CUDA_CUDART_LIBRARY)
        find_library(
            CUDA_CUDART_LIBRARY
            NAMES
            cudart
            cudart_static
            HINTS
            "${CUDAToolkit_ROOT}/lib/x64"
            "${CUDAToolkit_ROOT}/lib"
        )
    endif()
    set(CUDA_USE_STATIC_CUDA_RUNTIME OFF)
    set(CUDA_LIBRARIES "${CUDA_CUDART_LIBRARY}")
    macro(hmvision_find_cuda_helper_lib _n)
        find_library(
            CUDA_${_n}_LIBRARY
            NAMES
            ${_n}
            HINTS
            "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64"
            "${CUDA_TOOLKIT_ROOT_DIR}/lib"
        )
        mark_as_advanced(CUDA_${_n}_LIBRARY)
    endmacro()
    macro(find_cuda_helper_libs _n)
        hmvision_find_cuda_helper_lib(${_n})
    endmacro()
    hmvision_find_cuda_helper_lib(nppc)
    hmvision_find_cuda_helper_lib(nppi)
    hmvision_find_cuda_helper_lib(npps)
    hmvision_find_cuda_helper_lib(cufft)
    hmvision_find_cuda_helper_lib(cublas)
    hmvision_find_cuda_helper_lib(cublas_device)
    if(CUDA_cufft_LIBRARY)
        set(
            CUDA_CUFFT_LIBRARIES
            "${CUDA_cufft_LIBRARY}"
            CACHE
            STRING
            "cufft"
            FORCE
        )
    endif()
    if(CUDA_cublas_LIBRARY)
        if(CUDA_cublas_device_LIBRARY)
            set(
                CUDA_CUBLAS_LIBRARIES
                "${CUDA_cublas_LIBRARY};${CUDA_cublas_device_LIBRARY}"
                CACHE
                STRING
                "cublas"
                FORCE
            )
        else()
            set(
                CUDA_CUBLAS_LIBRARIES
                "${CUDA_cublas_LIBRARY}"
                CACHE
                STRING
                "cublas"
                FORCE
            )
        endif()
    endif()
    message(
        STATUS
        "HM_VISION: legacy CUDA variables filled from CUDAToolkit for OpenCV: toolkit=${CUDAToolkit_ROOT} NVCC=${CUDA_NVCC_EXECUTABLE}"
    )
endif()

# OpenCV: OpenCV_DIR / HMVISION_OPENCV_ROOT
if(NOT OpenCV_DIR)
    if(DEFINED ENV{OpenCV_DIR} AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{OpenCV_DIR}" _e)
        set(OpenCV_DIR "${_e}" CACHE PATH "OpenCV_DIR from environment")
    endif()
endif()
if(NOT OpenCV_DIR)
    if(DEFINED ENV{OPENCV_DIR} AND NOT "$ENV{OPENCV_DIR}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{OPENCV_DIR}" _e)
        set(OpenCV_DIR "${_e}" CACHE PATH "OpenCV from OPENCV_DIR env")
    endif()
endif()
if(OpenCV_DIR AND (OpenCV_DIR MATCHES ".*NOTFOUND.*" OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake"))
    unset(OpenCV_DIR CACHE)
endif()
if(NOT OpenCV_DIR AND EXISTS "${HMVISION_OPENCV_ROOT}/OpenCVConfig.cmake")
    set(OpenCV_DIR "${HMVISION_OPENCV_ROOT}" CACHE PATH "OpenCVConfig.cmake (root tree)")
endif()
if(NOT OpenCV_DIR AND EXISTS "${HMVISION_OPENCV_ROOT}/build/OpenCVConfig.cmake")
    set(OpenCV_DIR "${HMVISION_OPENCV_ROOT}/build" CACHE PATH "OpenCVConfig.cmake in build tree")
endif()
if(NOT OpenCV_DIR)
    foreach(
        _vc
        IN
        ITEMS
        vc19
        vc18
        vc17
        vc16
        vc15
    )
        if(EXISTS "${HMVISION_OPENCV_ROOT}/x64/${_vc}/lib/OpenCVConfig.cmake")
            set(
                OpenCV_DIR
                "${HMVISION_OPENCV_ROOT}/x64/${_vc}/lib"
                CACHE
                PATH
                "OpenCVConfig (x64/${_vc}/lib)"
            )
            break()
        endif()
        if(EXISTS "${HMVISION_OPENCV_ROOT}/build/x64/${_vc}/lib/OpenCVConfig.cmake")
            set(
                OpenCV_DIR
                "${HMVISION_OPENCV_ROOT}/build/x64/${_vc}/lib"
                CACHE
                PATH
                "OpenCVConfig (build/x64/${_vc}/lib)"
            )
            break()
        endif()
    endforeach()
endif()
# (Additional MSVC pack layouts: OpenCV-4.x.x/x64/vc16/lib/… are covered by GLOB **/OpenCVConfig.cmake below)
# e.g. <root>/something/build/x64/vc16/lib/OpenCVConfig.cmake (one level under root)
if(NOT OpenCV_DIR)
    file(
        GLOB
        _hmvision_glob_opencv
        "${HMVISION_OPENCV_ROOT}/*/x64/vc17/lib/OpenCVConfig.cmake"
        "${HMVISION_OPENCV_ROOT}/*/x64/vc16/lib/OpenCVConfig.cmake"
        "${HMVISION_OPENCV_ROOT}/*/x64/vc15/lib/OpenCVConfig.cmake"
    )
    if(_hmvision_glob_opencv)
        list(SORT _hmvision_glob_opencv)
        list(GET _hmvision_glob_opencv 0 _hmvision_first_ocv_cfg)
        get_filename_component(_hmvision_ocv_dir "${_hmvision_first_ocv_cfg}" DIRECTORY)
        set(OpenCV_DIR "${_hmvision_ocv_dir}" CACHE PATH "OpenCVConfig (auto glob under HMVISION_OPENCV_ROOT)")
    endif()
endif()
# Deep search under HMVISION_OPENCV_ROOT (CMake 3.5+ ** matches subdirs; keeps working for odd unzip layouts)
if(
    (NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
    AND IS_DIRECTORY "${HMVISION_OPENCV_ROOT}"
)
    set(_hmvision_opencv_recurse "")
    file(
        GLOB_RECURSE
        _hmvision_opencv_recurse
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES
        false
        "${HMVISION_OPENCV_ROOT}/**/OpenCVConfig.cmake"
    )
    set(_hmvision_opencv_cfg_pick "")
    if(_hmvision_opencv_recurse)
        foreach(
            _c
            IN
            LISTS
            _hmvision_opencv_recurse
        )
            get_filename_component(_d "${_c}" DIRECTORY)
            string(TOLOWER _dl "${_d}")
            if(
                _dl MATCHES "x64"
                AND (_dl MATCHES "vc[0-9]+" OR _dl MATCHES "share[\\\\/]opencv" OR _dl MATCHES "\\\\lib" OR _dl MATCHES "/lib")
            )
                set(_hmvision_opencv_cfg_pick "${_c}")
                break()
            endif()
        endforeach()
    endif()
    if(_hmvision_opencv_cfg_pick STREQUAL "" AND _hmvision_opencv_recurse)
        list(LENGTH _hmvision_opencv_recurse _n_ocv)
        if(_n_ocv GREATER 0)
            list(GET _hmvision_opencv_recurse 0 _hmvision_opencv_cfg_pick)
        endif()
    endif()
    if(NOT _hmvision_opencv_cfg_pick STREQUAL "")
        get_filename_component(_hmvision_ocv_dir2 "${_hmvision_opencv_cfg_pick}" DIRECTORY)
        if(EXISTS "${_hmvision_ocv_dir2}/OpenCVConfig.cmake")
            set(
                OpenCV_DIR
                "${_hmvision_ocv_dir2}"
                CACHE
                PATH
                "OpenCVConfig (GLOB ** under HMVISION_OPENCV_ROOT)"
                FORCE
            )
        endif()
    endif()
endif()

if(NOT EXISTS "${HMVISION_OPENCV_ROOT}")
    message(
        STATUS
        "HM_VISION: HMVISION_OPENCV_ROOT is not a directory (OpenCV not searched there): ${HMVISION_OPENCV_ROOT}"
    )
endif()
if(NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
    message(
        FATAL_ERROR
        "OpenCV not found (need OpenCVConfig.cmake). Fix one of:\n"
        "  1) Set cache OpenCV_DIR to the folder that CONTAINS OpenCVConfig.cmake (e.g. .../x64/vc16/lib), OR\n"
        "  2) Set HMVISION_OPENCV_ROOT to the top of your OpenCV install AND ensure that tree exists.\n"
        "Current values: OpenCV_DIR='${OpenCV_DIR}'  HMVISION_OPENCV_ROOT='${HMVISION_OPENCV_ROOT}'\n"
        "Tip: In Qt Creator, clear CMake cache (or delete the build dir) if OpenCV_DIR is stale, then reconfigure."
    )
endif()

find_package(
    OpenCV
    REQUIRED
    COMPONENTS
        core
        dnn
        imgproc
        videoio
)

# CUDAToolkit: already found above for OpenCV; re-sync prefix if the cache changed HMVISION_CUDA_ROOT
if(NOT TARGET CUDA::cudart)
    if(POLICY CMP0104)
        cmake_policy(SET CMP0104 NEW)
    endif()
    set(CUDAToolkit_ROOT "${HMVISION_CUDA_ROOT}" CACHE PATH "CUDA root for this target" FORCE)
    if(EXISTS "${HMVISION_CUDA_ROOT}/bin/nvcc.exe")
        set(ENV{CUDA_PATH} "${HMVISION_CUDA_ROOT}")
        set(ENV{CUDA_PATH_V1_0} "${HMVISION_CUDA_ROOT}")
        set(
            CUDA_TOOLKIT_ROOT_DIR
            "${HMVISION_CUDA_ROOT}"
            CACHE
            PATH
            "CUDA root"
            FORCE
        )
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${HMVISION_CUDA_ROOT}")
    find_package(CUDAToolkit REQUIRED MODULE)
endif()

# nlohmann_json: find_package, else FetchContent
find_package(nlohmann_json 3.11 QUIET CONFIG)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json: not in prefix path, FetchContent (network required) ...")
    set(JSON_BuildTests
        OFF
        CACHE INTERNAL
        ""
    )
    set(JSON_Install
        OFF
        CACHE INTERNAL
        ""
    )
    include(FetchContent)
    FetchContent_Declare(
        hmvision_nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP
            TRUE
    )
    FetchContent_MakeAvailable(hmvision_nlohmann_json)
endif()

# ---------------------------------------------------------------------------
# Sources (full build)
# ---------------------------------------------------------------------------
set(
    HMVISION_UI_SOURCES
    main.cpp
    include/UI/MainWindow.h
    src/UI/MainWindow.cpp
    include/UI/MainInterfaceWidget.h
    src/UI/MainInterfaceWidget.cpp
    include/UI/CameraPage.h
    src/UI/CameraPage.cpp
    include/UI/AlgorithmPage.h
    src/UI/AlgorithmPage.cpp
    include/UI/MultiCameraView.h
    include/UI/CameraWidget.h
    src/UI/CameraWidget.cpp
)
set(
    HMVISION_ALL_SOURCES
    ${HMVISION_UI_SOURCES}
    include/Common/Types.h
    include/Core/Logger.h
    src/Core/Logger.cpp
    include/Core/ConfigManager.h
    src/Core/ConfigManager.cpp
    include/Core/VisionSystem.h
    src/Core/VisionSystem.cpp
    include/Device/ICamera.h
    include/Device/IPLCInterface.h
    include/Device/CameraFactory.h
    src/Device/CameraFactory.cpp
    include/Device/CameraManager.h
    src/Device/CameraManager.cpp
    include/Device/ModbusPLC.h
    src/Device/ModbusPLC.cpp
    include/Device/USB_Camera.h
    src/Device/USB_Camera.cpp
    include/Device/GigE_Camera.h
    src/Device/GigE_Camera.cpp
    include/Device/HikCameraBase.h
    src/Device/HikCameraBase.cpp
    include/Device/HikGigECamera.h
    src/Device/HikGigECamera.cpp
    include/Device/HikUSBCamera.h
    src/Device/HikUSBCamera.cpp
    include/Device/VirtualCamera.h
    src/Device/VirtualCamera.cpp
    include/Common/AlgorithmResult.h
    include/Common/AlgorithmParams.h
    include/Algorithm/IVisionAlgorithm.h
    include/Algorithm/AlgorithmFactory.h
    src/Algorithm/AlgorithmFactory.cpp
    include/Algorithm/AlgorithmScheduler.h
    src/Algorithm/AlgorithmScheduler.cpp
    include/Algorithm/DetectionAlgorithm.h
    src/Algorithm/DetectionAlgorithm.cpp
    include/Algorithm/HalconAlgorithm.h
    src/Algorithm/HalconAlgorithm.cpp
    include/Algorithm/YOLODetector.h
    src/Algorithm/YOLODetector.cpp
    include/Algorithm/OCRProcessor.h
    src/Algorithm/OCRProcessor.cpp
    include/Algorithm/ThreadPool.h
    src/Algorithm/ThreadPool.cpp
    include/Communication/MESClient.h
    src/Communication/MESClient.cpp
    include/Data/ResultRepository.h
    src/Data/ResultRepository.cpp
)

add_executable(HM_VISION ${HMVISION_ALL_SOURCES})

option(
    HMVISION_BUILD_TESTS
    "Build HM_VISION tests"
    ON
)

if(MSVC)
    target_compile_options(HM_VISION PRIVATE /utf-8)
endif()

target_include_directories(
    HM_VISION
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(
    _hmvision_opencv_includes
    "${OpenCV_INCLUDE_DIRS}"
)
if(NOT _hmvision_opencv_includes)
    foreach(
        _try
        IN
        ITEMS
        "${OpenCV_DIR}/include"
        "${OpenCV_DIR}/../include"
        "${OpenCV_DIR}/../../include"
        "${OpenCV_DIR}/../../../include"
        "${HMVISION_OPENCV_ROOT}/include"
        "${HMVISION_OPENCV_ROOT}/build/include"
    )
        if(EXISTS "${_try}/opencv2/core.hpp")
            set(_hmvision_opencv_includes "${_try}")
            message(STATUS "HM_VISION: OpenCV include dir (heuristic): ${_hmvision_opencv_includes}")
            break()
        endif()
    endforeach()
endif()
if(_hmvision_opencv_includes)
    target_include_directories(HM_VISION PRIVATE ${_hmvision_opencv_includes})
else()
    message(
        FATAL_ERROR
        "OpenCV include directory not found (expected opencv2/core.hpp). Fix OpenCV install or OpenCV_DIR (current: ${OpenCV_DIR})."
    )
endif()

target_link_libraries(HM_VISION PRIVATE ${OpenCV_LIBS} nlohmann_json::nlohmann_json)
target_compile_definitions(
    HM_VISION
    PRIVATE
        HMVISION_HAVE_OPENCV=1
)

# 海康 MVS: MvCameraControl.h + libMvCameraControl.so (Linux) / MvCameraControl.lib (Windows)
# Search order: MVS_ROOT env, HMVISION_MVS_ROOT, thirdparty/ (next to top CMake dir or repo root
# when opening VisionSystem/ only), then standard Hik install paths. NO_DEFAULT_PATH needs explicit
# candidates: without both header+lib, HMVISION_HAVE_MVS=0 and GigE/Hik is stubbed at runtime.
set(_mvs_paths_include
    ${CMAKE_SOURCE_DIR}/thirdparty/mvs/include
    ${CMAKE_SOURCE_DIR}/../thirdparty/mvs/include
    /opt/MVS/include
    "C:/Program Files/MVS/include"
    "C:/Program Files/MVS/Development/Includes"
    "C:/Program Files (x86)/MVS/include"
    "C:/Program Files (x86)/MVS/Development/Includes"
    "C:/MVS/include"
    "C:/MVS/Development/Includes"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Includes"
    "${CMAKE_SOURCE_DIR}/../MVS/Development/Includes"
)
set(_mvs_paths_lib
    ${CMAKE_SOURCE_DIR}/thirdparty/mvs/lib
    ${CMAKE_SOURCE_DIR}/../thirdparty/mvs/lib
    /opt/MVS/lib
    "C:/Program Files/MVS/lib"
    "C:/Program Files/MVS/Development/Libraries/win64"
    "C:/Program Files/MVS/Development/Libraries/Win64"
    "C:/Program Files (x86)/MVS/lib"
    "C:/Program Files (x86)/MVS/Development/Libraries/win64"
    "C:/Program Files (x86)/MVS/Development/Libraries/Win64"
    "C:/MVS/lib"
    "C:/MVS/Development/Libraries/win64"
    "C:/MVS/Development/Libraries/Win64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/win64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/amd64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/x64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/aarch64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/lin64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/lin/64"
    "${CMAKE_SOURCE_DIR}/MVS/Development/Libraries/lin/ARM64"
    "${CMAKE_SOURCE_DIR}/../MVS/Development/Libraries/win64"
    "${CMAKE_SOURCE_DIR}/../MVS/Development/Libraries/Win64"
)
if(DEFINED ENV{MVS_ROOT} AND NOT "x$ENV{MVS_ROOT}" STREQUAL "x")
    file(TO_CMAKE_PATH "$ENV{MVS_ROOT}" _hmvision_mvs_from_env)
    list(
        APPEND
        _mvs_paths_include
        "${_hmvision_mvs_from_env}/include"
        "${_hmvision_mvs_from_env}/Development/Includes"
    )
    list(
        APPEND
        _mvs_paths_lib
        "${_hmvision_mvs_from_env}/lib"
        "${_hmvision_mvs_from_env}/Development/Libraries/win64"
        "${_hmvision_mvs_from_env}/Development/Libraries/Win64"
    )
    message(STATUS "HM_VISION: MVS_ROOT from environment: ${_hmvision_mvs_from_env}")
    unset(_hmvision_mvs_from_env)
endif()
if(HMVISION_MVS_ROOT
   AND NOT "x${HMVISION_MVS_ROOT}" STREQUAL "x"
  )
    list(
        APPEND
        _mvs_paths_include
        "${HMVISION_MVS_ROOT}/include"
        "${HMVISION_MVS_ROOT}/Development/Includes"
    )
    list(
        APPEND
        _mvs_paths_lib
        "${HMVISION_MVS_ROOT}/lib"
        "${HMVISION_MVS_ROOT}/Development/Libraries/win64"
        "${HMVISION_MVS_ROOT}/Development/Libraries/Win64"
        "${HMVISION_MVS_ROOT}/Development/Libraries/64"
        "${HMVISION_MVS_ROOT}/Development/Libraries/amd64"
        "${HMVISION_MVS_ROOT}/Development/Libraries/x64"
        "${HMVISION_MVS_ROOT}/Development/Libraries/aarch64"
        "${HMVISION_MVS_ROOT}/lib/64"
        "${HMVISION_MVS_ROOT}/lib/x64"
        "${HMVISION_MVS_ROOT}/lib/lin64"
    )
endif()
find_path(
    MVS_INCLUDE_DIR
    NAMES MvCameraControl.h
    PATHS
        ${_mvs_paths_include}
    NO_DEFAULT_PATH
)
find_library(
    MVS_LIBRARY
    NAMES MvCameraControl
    PATHS
        ${_mvs_paths_lib}
    NO_DEFAULT_PATH
)
unset(_mvs_paths_include)
unset(_mvs_paths_lib)
# If only the library is found, try header next to typical SDK tree
if(MVS_LIBRARY AND NOT MVS_INCLUDE_DIR)
    get_filename_component(_mvs_libdir "${MVS_LIBRARY}" DIRECTORY)
    find_path(
        MVS_INCLUDE_DIR
        NAMES MvCameraControl.h
        HINTS
        "${_mvs_libdir}/../include"
        "${_mvs_libdir}/../Includes"
        "${_mvs_libdir}/../../include"
        "${_mvs_libdir}/../../Development/Includes"
        NO_DEFAULT_PATH
    )
    unset(_mvs_libdir)
endif()
# If header is found but lib is not, search typical layout: Development/Includes -> Development/Libraries/win64
if(MVS_INCLUDE_DIR AND NOT MVS_LIBRARY)
    find_library(
        MVS_LIBRARY
        NAMES MvCameraControl
        HINTS
        "${MVS_INCLUDE_DIR}/../lib"
        "${MVS_INCLUDE_DIR}/../Libraries/win64"
        "${MVS_INCLUDE_DIR}/../Libraries/Win64"
        "${MVS_INCLUDE_DIR}/../../lib"
        "${MVS_INCLUDE_DIR}/../../Development/Libraries/win64"
        "${MVS_INCLUDE_DIR}/../../Development/Libraries/Win64"
        NO_DEFAULT_PATH
    )
endif()
if(MVS_INCLUDE_DIR AND MVS_LIBRARY)
    target_include_directories(HM_VISION PRIVATE ${MVS_INCLUDE_DIR})
    target_link_libraries(HM_VISION PRIVATE ${MVS_LIBRARY})
    target_compile_definitions(
        HM_VISION
        PRIVATE
            HMVISION_HAVE_MVS=1
    )
    message(STATUS "Found MVS SDK: ${MVS_INCLUDE_DIR}  lib=${MVS_LIBRARY}")
    if(UNIX
       AND NOT APPLE
      )
        get_filename_component(_mvs_rpath_dir "${MVS_LIBRARY}" DIRECTORY)
        set_property(
            TARGET HM_VISION
            APPEND
            PROPERTY
            BUILD_RPATH
            "${_mvs_rpath_dir}"
        )
        set_property(
            TARGET HM_VISION
            APPEND
            PROPERTY
            INSTALL_RPATH
            "${_mvs_rpath_dir}"
        )
        unset(_mvs_rpath_dir)
    endif()
else()
    target_compile_definitions(
        HM_VISION
        PRIVATE
            HMVISION_HAVE_MVS=0
            DISABLE_HIKVISION=1
    )
    message(
        WARNING
        "MVS SDK not found; Hikvision camera support will be disabled. "
        "Install the Hikvision MVS SDK, then either: (1) set cache HMVISION_MVS_ROOT to the SDK root, "
        "(2) set environment MVS_ROOT to the same, or (3) copy MvCameraControl.h + MvCameraControl.lib "
        "into ${CMAKE_SOURCE_DIR}/thirdparty/mvs (or ${CMAKE_SOURCE_DIR}/../thirdparty/mvs when opening VisionSystem only). "
        "CMake also checks C:/Program Files/MVS, C:/Program Files (x86)/MVS, C:/MVS, and /opt/MVS."
    )
endif()

# HALCON (MVTec layout: include/halconcpp/HalconCpp.h, lib/x64-win64/... )
find_path(
    HALCON_INCLUDE_DIR
    HalconCpp.h
    HINTS
    "${HMVISION_HALCON_ROOT}"
    "${HMVISION_HALCON_ROOT}/include"
    "${HMVISION_HALCON_ROOT}/include/halconcpp"
    "${HMVISION_HALCON_ROOT}/halcon/include"
    PATH_SUFFIXES
    include
    include/halconcpp
    halcon/include
    halconcpp
)
find_library(
    HALCONCPP_LIB
    HalconCpp
    HINTS
    "${HMVISION_HALCON_ROOT}/lib/x64-win64/Release/Shared"
    "${HMVISION_HALCON_ROOT}/lib/x64-win64/Release"
    "${HMVISION_HALCON_ROOT}/lib/x64-win64/Shared/Release"
    "${HMVISION_HALCON_ROOT}/lib/x64-win64"
    "${HMVISION_HALCON_ROOT}/lib"
)
if(NOT HALCON_INCLUDE_DIR)
    message(
        FATAL_ERROR
        "HalconCpp.h not found. Set HMVISION_HALCON_ROOT or HALCON_INCLUDE_DIR, e.g.:\n"
        "  ${HMVISION_HALCON_ROOT}/include  ${HMVISION_HALCON_ROOT}/include/halconcpp"
    )
endif()
if(NOT HALCONCPP_LIB)
    message(
        FATAL_ERROR
        "HalconCpp import library not found. Check HMVISION_HALCON_ROOT lib/x64-win64 (or .../Release/Shared), or set HALCONCPP_LIB to the .lib full path."
    )
endif()
target_include_directories(HM_VISION PRIVATE ${HALCON_INCLUDE_DIR})
target_link_libraries(HM_VISION PRIVATE ${HALCONCPP_LIB})

# Paddle Inference (OCR) — common layouts: .../paddle, .../third_party, .../third_party/install
set(_hmvision_paddle_search_prefixes
    "${HMVISION_PADDLE_ROOT}"
    "${HMVISION_PADDLE_ROOT}/include"
    "${HMVISION_PADDLE_ROOT}/paddle"
    "${HMVISION_PADDLE_ROOT}/paddle/include"
    "${HMVISION_PADDLE_ROOT}/third_party"
    "${HMVISION_PADDLE_ROOT}/third_party/include"
    "${HMVISION_PADDLE_ROOT}/third_party/install"
    "${HMVISION_PADDLE_ROOT}/third_party/install/include"
    "${HMVISION_PADDLE_ROOT}/third_party/paddle"
    "${HMVISION_PADDLE_ROOT}/paddle_inference"
)
find_path(
    PADDLEOCR_INCLUDE_DIR
    NAMES
        paddleocr.h
    HINTS
        ${_hmvision_paddle_search_prefixes}
    PATH_SUFFIXES
        ""
        "include"
        "paddle"
        "paddle/include"
        "paddle_c/include"
        "fluid/inference/api"
        "install/include"
    NO_DEFAULT_PATH
)
if(NOT PADDLEOCR_INCLUDE_DIR)
    find_path(
        PADDLEOCR_INCLUDE_DIR
        NAMES
            paddle_c_api.h
        HINTS
            ${_hmvision_paddle_search_prefixes}
        PATH_SUFFIXES
            ""
            "include"
            "paddle"
            "paddle_c/include"
            "fluid/inference/api"
            "install/include"
        NO_DEFAULT_PATH
    )
endif()
# Official C++ API (optional compile path in OCRProcessor.h)
if(NOT PADDLEOCR_INCLUDE_DIR)
    find_path(
        PADDLEOCR_INCLUDE_DIR
        NAMES
            paddle_inference_api.h
        HINTS
            ${_hmvision_paddle_search_prefixes}
        PATH_SUFFIXES
            ""
            "include"
            "paddle"
            "fluid/inference/api"
    )
endif()
# Last resort: locate any of the above under paddle/ and third_party/
if(
    (NOT PADDLEOCR_INCLUDE_DIR)
    AND IS_DIRECTORY "${HMVISION_PADDLE_ROOT}"
)
    file(
        GLOB_RECURSE
        _hmvision_paddle_h_glob
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES
        false
        "${HMVISION_PADDLE_ROOT}/**/paddleocr.h"
        "${HMVISION_PADDLE_ROOT}/**/paddle_c_api.h"
        "${HMVISION_PADDLE_ROOT}/**/paddle_inference_api.h"
    )
    if(_hmvision_paddle_h_glob)
        list(LENGTH _hmvision_paddle_h_glob _np)
        if(_np GREATER 0)
            list(GET _hmvision_paddle_h_glob 0 _h0)
            get_filename_component(PADDLEOCR_INCLUDE_DIR "${_h0}" DIRECTORY)
        endif()
    endif()
endif()
if(NOT PADDLEOCR_INCLUDE_DIR)
    message(
        FATAL_ERROR
        "Paddle headers not found (paddleocr.h, paddle_c_api.h, or paddle_inference_api.h). Set HMVISION_PADDLE_ROOT or PADDLEOCR_INCLUDE_DIR.\n"
        "  HMVISION_PADDLE_ROOT = ${HMVISION_PADDLE_ROOT}\n"
        "Expected e.g. .../paddle and .../third_party (with include tree under your SDK)."
    )
endif()
target_include_directories(HM_VISION PRIVATE ${PADDLEOCR_INCLUDE_DIR})

set(_hmvision_paddle_lib_names
    paddleocr
    paddle_inference
    libpaddle_inference
    paddle
    libpaddle
)
set(_hmvision_paddle_lib_hints
    "${HMVISION_PADDLE_ROOT}/lib"
    "${HMVISION_PADDLE_ROOT}/third_party/lib"
    "${HMVISION_PADDLE_ROOT}/third_party/install/lib"
    "${HMVISION_PADDLE_ROOT}/third_party/paddle"
    "${HMVISION_PADDLE_ROOT}/third_party/paddle/paddle"
    "${HMVISION_PADDLE_ROOT}/paddle/lib"
    "${HMVISION_PADDLE_ROOT}/paddle/build"
    "${HMVISION_PADDLE_ROOT}/build/paddle"
)
find_library(
    PADDLEOCR_LIB
    NAMES
        ${_hmvision_paddle_lib_names}
    HINTS
        ${_hmvision_paddle_lib_hints}
    PATH_SUFFIXES
        ""
        "Release"
        "Debug"
        "x64/Release"
        "x64/Debug"
    NO_DEFAULT_PATH
)
if(NOT PADDLEOCR_LIB)
    find_library(
        PADDLEOCR_LIB
        NAMES
            ${_hmvision_paddle_lib_names}
    )
endif()
# Deep search for *.lib under paddle/ and third_party/ (e.g. custom out-dir)
if(NOT PADDLEOCR_LIB AND IS_DIRECTORY "${HMVISION_PADDLE_ROOT}")
    file(
        GLOB_RECURSE
        _hmvision_paddle_l_glob
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES
        false
        "${HMVISION_PADDLE_ROOT}/**/paddle_inference.lib"
        "${HMVISION_PADDLE_ROOT}/**/libpaddle_inference.lib"
    )
    if(_hmvision_paddle_l_glob)
        list(SORT _hmvision_paddle_l_glob)
        list(GET _hmvision_paddle_l_glob 0 PADDLEOCR_LIB)
    endif()
endif()
if(NOT PADDLEOCR_LIB)
    message(
        FATAL_ERROR
        "Paddle inference library not found. Check HMVISION_PADDLE_ROOT/lib/ or set PADDLEOCR_LIB to the full .lib path.\n"
        "  ${HMVISION_PADDLE_ROOT}\n"
        "Tried: paddleocr, paddle_inference, libpaddle_inference"
    )
endif()
target_link_libraries(HM_VISION PRIVATE ${PADDLEOCR_LIB})

# Optional: Modbus / S7
find_path(
    LIBMODBUS_INCLUDE_DIR
    modbus/modbus.h
)
find_library(
    LIBMODBUS_LIBRARY
    modbus
)
if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
    target_include_directories(HM_VISION PRIVATE ${LIBMODBUS_INCLUDE_DIR})
    target_link_libraries(HM_VISION PRIVATE ${LIBMODBUS_LIBRARY})
endif()

find_path(
    SNAP7_INCLUDE_DIR
    snap7.h
)
find_library(
    SNAP7_LIBRARY
    snap7
)
if(SNAP7_INCLUDE_DIR AND SNAP7_LIBRARY)
    target_include_directories(HM_VISION PRIVATE ${SNAP7_INCLUDE_DIR})
    target_link_libraries(HM_VISION PRIVATE ${SNAP7_LIBRARY})
endif()

# TensorRT (required; HMVISION_TENSORRT_ROOT)
find_path(
    TENSORRT_INCLUDE_DIR
    NvInfer.h
    HINTS
    "${HMVISION_TENSORRT_ROOT}/include"
    "${HMVISION_TENSORRT_ROOT}/samples"
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH
)
if(NOT TENSORRT_INCLUDE_DIR)
    find_path(
        TENSORRT_INCLUDE_DIR
        NvInfer.h
        HINTS
        "${HMVISION_TENSORRT_ROOT}"
        PATH_SUFFIXES
        include
    )
endif()
find_library(
    TENSORRT_NVINFER_LIB
    nvinfer
    HINTS
    "${HMVISION_TENSORRT_ROOT}/lib"
    "${HMVISION_TENSORRT_ROOT}/lib/Release"
    NO_DEFAULT_PATH
)
if(NOT TENSORRT_NVINFER_LIB)
    find_library(
        TENSORRT_NVINFER_LIB
        nvinfer
    )
endif()
find_library(
    TENSORRT_NVONNXPARSER_LIB
    nvonnxparser
    HINTS
    "${HMVISION_TENSORRT_ROOT}/lib"
    "${HMVISION_TENSORRT_ROOT}/lib/Release"
    NO_DEFAULT_PATH
)
if(NOT TENSORRT_NVONNXPARSER_LIB)
    find_library(
        TENSORRT_NVONNXPARSER_LIB
        nvonnxparser
    )
endif()
if(
    TENSORRT_INCLUDE_DIR
    AND TENSORRT_NVINFER_LIB
    AND TARGET CUDA::cudart
)
    target_include_directories(HM_VISION PRIVATE ${TENSORRT_INCLUDE_DIR})
    target_link_libraries(
        HM_VISION
        PRIVATE
            ${TENSORRT_NVINFER_LIB}
    )
    if(TENSORRT_NVONNXPARSER_LIB)
        target_link_libraries(
            HM_VISION
            PRIVATE
                ${TENSORRT_NVONNXPARSER_LIB}
        )
    endif()
else()
    message(
        FATAL_ERROR
        "TensorRT not found. Set HMVISION_TENSORRT_ROOT (default D:/TensorRT-8.6.1.6). Need include/NvInfer.h and lib/nvinfer.lib. Current root: ${HMVISION_TENSORRT_ROOT}"
    )
endif()

target_link_libraries(
    HM_VISION
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        CUDA::cudart
)

# On Windows, copy Qt6 runtime DLLs next to HM_VISION.exe so the app runs outside Qt Creator
# (avoids: Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll / Qt6Network.dll not found).
if(WIN32)
    find_program(
        HMVISION_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS
            "${Qt6_DIR}/../../../bin"
            "$ENV{QTDIR}/bin"
    )
    if(HMVISION_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(
            TARGET HM_VISION
            POST_BUILD
            COMMAND
                "${HMVISION_WINDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE:HM_VISION>"
                --no-translations
            COMMENT
                "windeployqt: deploy Qt6 DLLs beside HM_VISION.exe"
        )
    else()
        message(
            WARNING
            "windeployqt not found (expect under Qt kit bin, e.g. ${Qt6_DIR}/../../../bin). "
            "Run HM_VISION from Qt Creator, or add that bin folder to PATH, or add Qt to the system PATH."
        )
    endif()
endif()

if(HMVISION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
