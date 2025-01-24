cmake_minimum_required(VERSION 3.10)
project(stream_pull_sample)

add_compile_options(-std=c++11 -fPIC -fstack-protector-all -g -Wl,-z,relro,-z,now,-z -pie -Wall)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0 -Dgoogle=mindxsdk_private)

set(OUTPUT_NAME "stream_pull_test")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set(MX_SDK_HOME "/usr/local/Ascend/mxVision")
set(FFMPEG_PATH "/data/local/ffmpeg")

include_directories(
        ${MX_SDK_HOME}/include
        ${MX_SDK_HOME}/opensource/include
        ${MX_SDK_HOME}/opensource/include/opencv4
        ${MX_SDK_HOME}/include/MxBase/postprocess/include
        ${FFMPEG_PATH}/include
)

link_directories(
        ${MX_SDK_HOME}/lib
        ${MX_SDK_HOME}/opensource/lib
        ${MX_SDK_HOME}/lib/modelpostprocessors
        ${FFMPEG_PATH}/lib
        /usr/local/Ascend/ascend-toolkit/latest/acllib/lib64
)

add_executable(${OUTPUT_NAME} main.cpp VideoProcess/VideoProcess.cpp VideoProcess/VideoProcess.h
        Yolov3Detection/Yolov3Detection.cpp Yolov3Detection/Yolov3Detection.h
        ResnetDetector/ResnetDetector.cpp ResnetDetector/ResnetDetector.cpp)
target_link_libraries(${OUTPUT_NAME}
        avcodec
        avdevice
        avfilter
        avformat
        avcodec
        avutil
        swscale
        swresample
        glog
        mxbase
        cpprest
        opencv_world
        pthread m
        yolov3postprocess
        )

