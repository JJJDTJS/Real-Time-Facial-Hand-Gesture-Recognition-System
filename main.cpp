/*
 * Copyright(C) 2021. Huawei Technologies Co.,Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <csignal>
#include <unistd.h>
#include "MxBase/ErrorCode/ErrorCodes.h"
#include "MxBase/Log/Log.h"
#include "MxBase/DvppWrapper/DvppWrapper.h"
#include "MxBase/MemoryHelper/MemoryHelper.h"
#include "MxBase/DeviceManager/DeviceManager.h"
#include "BlockingQueue/BlockingQueue.h"
#include "VideoProcess/VideoProcess.h"
#include "Yolov3Detection/Yolov3Detection.h"
#include "ResnetDetector/ResnetDetector.h"

bool VideoProcess::stopFlag = false;
std::vector<double> g_inferCost;
namespace {
    const uint32_t MAX_QUEUE_LENGHT = 1000;
}

static void SigHandler(int signal)
{
    if (signal == SIGINT) {
        VideoProcess::stopFlag = true;
    }
}

    /*
CLASS_NUM=1
BIASES_NUM=18
BIASES=10,13,16,30,33,23,30,61,62,45,59,119,116,90,156,198,373,326
SCORE_THRESH=0.31
OBJECTNESS_THRESH=0.3
IOU_THRESH=0.45
YOLO_TYPE=3
ANCHOR_DIM=3
MODEL_TYPE=1
RESIZE_FLAG=0    
    */
void InitYolov3Param(InitParam &initParam, const uint32_t deviceID)
{
    initParam.deviceId = deviceID;
    initParam.labelPath = "./model/coco.names";
    initParam.checkTensor = true;
    initParam.modelPath = "./model/hand.om";
    initParam.classNum = 1;
    initParam.biasesNum = 18;
    initParam.biases = "10,13,16,30,33,23,30,61,62,45,59,119,116,90,156,198,373,326";
    initParam.objectnessThresh = "0.3";
    initParam.iouThresh = "0.45";
    initParam.scoreThresh = "0.31";
    initParam.yoloType = 3;
    initParam.modelType = 1;
    initParam.inputType = 0;
    initParam.anchorDim = 3;
}
void InitResnetParam(ResnetInitParam &initParam, const uint32_t deviceID)
{
    initParam.deviceId = deviceID;
    initParam.modelPath = "./model/hand_keypoint.om";
    initParam.classNum = 21;
}

int main(int argc, char* argv[]) {
    std::string streamName = "rtsp://192.168.30.20/";
    std::string clientIp = "rtsp://192.168.30.36/";
    if(argc>1){
        streamName = argv[1];
    }
    if(argc>2){
    	clientIp = argv[2];
    }
    LogInfo << "begin hand detect process on :" << streamName;
    APP_ERROR ret = MxBase::DeviceManager::GetInstance()->InitDevices();
    if (ret != APP_ERR_OK) {
        LogError << "InitDevices failed";
        return ret;
    }
    LogInfo << "InitDevices done";

    auto videoProcess = std::make_shared<VideoProcess>();
    auto yolov3 = std::make_shared<Yolov3Detection>();
    auto resnet = std::make_shared<ResnetDetector>();

    InitParam initParam;
    InitYolov3Param(initParam, videoProcess->DEVICE_ID);
    ResnetInitParam resInitParam;
    InitResnetParam(resInitParam, videoProcess->DEVICE_ID);
    // 初始化模型推理所需的配置信息
    yolov3->FrameInit(initParam);
    LogInfo << "Init yolo done";
    resnet->Init(resInitParam);
    LogInfo << "Init resnet done";
    MxBase::DeviceContext device;
    device.devId = videoProcess->DEVICE_ID;
    ret = MxBase::DeviceManager::GetInstance()->SetDevice(device);
    if (ret != APP_ERR_OK) {
        LogError << "SetDevice failed";
        return ret;
    }
    // 视频流处理
    ret = videoProcess->StreamInit(streamName,clientIp);
    if (ret != APP_ERR_OK) {
        LogError << "StreamInit failed";
        return ret;
    }

    // 解码模块功能初始化
    ret = videoProcess->VideoDecodeInit();
    if (ret != APP_ERR_OK) {
        LogError << "VideoDecodeInit failed";
        MxBase::DeviceManager::GetInstance()->DestroyDevices();
        return ret;
    }

    auto blockingQueue = std::make_shared<BlockingQueue<std::shared_ptr<void>>>(MAX_QUEUE_LENGHT);
    std::thread getFrame(videoProcess->GetFrames, blockingQueue, videoProcess);
    std::thread getResult(videoProcess->GetResults, blockingQueue, yolov3,resnet, videoProcess);

    if (signal(SIGINT, SigHandler) == SIG_ERR) {
        LogError << "can not catch SIGINT";
        return APP_ERR_COMM_FAILURE;
    }

    while (!videoProcess->stopFlag) {
        sleep(10);
    }
    getFrame.join();
    getResult.join();

    blockingQueue->Stop();
    blockingQueue->Clear();

    ret = yolov3->FrameDeInit();
    if (ret != APP_ERR_OK) {
        LogError << "FrameInit failed";
        return ret;
    }
    ret = videoProcess->StreamDeInit();
    if (ret != APP_ERR_OK) {
        LogError << "StreamDeInit failed";
        return ret;
    }
    ret = videoProcess->VideoDecodeDeInit();
    if (ret != APP_ERR_OK) {
        LogError << "VideoDecodeDeInit failed";
        return ret;
    }
    ret = MxBase::DeviceManager::GetInstance()->DestroyDevices();
    if (ret != APP_ERR_OK) {
        LogError << "DestroyDevices failed";
        return ret;
    }
    return 0;
}
