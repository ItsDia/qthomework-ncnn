#cmake_minimum_required(VERSION 3.29)
#project(untitled3)
#
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)
#
#
#find_package(Qt6 COMPONENTS
#        Core
#        REQUIRED)
#
#add_executable(untitled3 main.cpp)
#target_link_libraries(untitled3
#        Qt::Core
#)
#
#if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#    set(DEBUG_SUFFIX)
#    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
#        set(DEBUG_SUFFIX "d")
#    endif ()
#    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
#    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
#        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
#        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
#            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
#        endif ()
#    endif ()
#    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E make_directory
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy
#                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
#    endif ()
#    foreach (QT_LIB Core)
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy
#                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
#    endforeach (QT_LIB)
#endif ()
cmake_minimum_required(VERSION 3.29)
project(yolov8-pose)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 项目路径设置
set(project_name yolov8-pose)
set(project_root ${PROJECT_SOURCE_DIR})
set(project_src ${project_root}/src)
set(project_include ${project_root}/include)
set(EXECUTABLE_OUTPUT_PATH ${project_root}/bin)

# CUDA 选项
option(CUDA_USE_STATIC_CUDA_RUNTIME OFF)


# 查找 OpenMP
find_package(OpenMP REQUIRED)
if(OpenMP_FOUND)
    message("OpenMP found")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# 设置 OpenCV 路径并查找
set(OpenCV_DIR "D:/Opencv/opencv/build")
find_package(OpenCV REQUIRED)

# 设置 Eigen 路径
set(Eigen_DIR "cmakefiles/eigen-3.4.0")

# 设置 ncnn 路径并查找
set(ncnn_DIR "cmakefiles/ncnn-20240820-windows-vs2019/x64/lib/cmake/ncnn")
find_package(ncnn REQUIRED)
if(NOT TARGET ncnn)
    message(WARNING "ncnn NOT FOUND! Please set ncnn_DIR environment variable")
else()
    message("ncnn found")
endif()

# 设置 Qt 路径并查找
set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2019_64")
find_package(Qt6 COMPONENTS Core Network Gui WebSockets Sql REQUIRED)
# 设置构建类型
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# 包含目录
include_directories(
    ${project_include}
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Eigen_DIR}
)

# 添加可执行文件
add_executable(
    yolov8-pose
    src/BYTETracker.cpp
    src/kalmanFilter.cpp
    src/lapjv.cpp
    src/utils.cpp
    src/STrack.cpp
    src/yolov8_pose.cpp
    src/main.cpp
)

# 链接库
target_link_libraries(
    yolov8-pose
    Qt6::Core
    ${OpenCV_LIBS}
    ncnn
    Qt6::Network
    Qt6::Gui
    Qt6::WebSockets
    Qt6::Sql
    OpenMP::OpenMP_CXX
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")

    # 确保 QT_INSTALL_PATH 指向正确的 bin 目录
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    # 复制平台插件
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()

    # 复制 Qt 核心库
    foreach (QT_LIB Core Network Gui)
        if (EXISTS "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endif ()
    endforeach (QT_LIB)
endif ()

