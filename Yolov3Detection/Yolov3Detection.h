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

#ifndef STREAM_PULL_SAMPLE_YOLOV3DETECTION_H
#define STREAM_PULL_SAMPLE_YOLOV3DETECTION_H

#include "MxBase/DvppWrapper/DvppWrapper.h"
#include "MxBase/MemoryHelper/MemoryHelper.h"
#include "MxBase/DeviceManager/DeviceManager.h"
#include "MxBase/Tensor/TensorContext/TensorContext.h"
#include "MxBase/ModelInfer/ModelInferenceProcessor.h"
#include "ObjectPostProcessors/Yolov3PostProcess.h"
#include "opencv2/opencv.hpp"

extern std::vector<double> g_inferCost;

struct InitParam {
    uint32_t deviceId;
    std::string labelPath;
    bool checkTensor;
    std::string modelPath;
    uint32_t classNum;
    uint32_t biasesNum;
    std::string biases;
    std::string objectnessThresh;
    std::string iouThresh;
    std::string scoreThresh;
    uint32_t yoloType;
    uint32_t modelType;
    uint32_t inputType;
    uint32_t anchorDim;
};

class Yolov3Detection {
protected:
    APP_ERROR LoadLabels(const std::string &labelPath, std::map<int, std::string> &labelMap);
    void SetYolov3PostProcessConfig(const InitParam &initParam, std::map<std::string, std::shared_ptr<void>> &config);
public:
    APP_ERROR FrameInit(const InitParam &initParam);
    APP_ERROR FrameDeInit();
    APP_ERROR ResizeFrame(const std::shared_ptr<MxBase::MemoryData> frameInfo, const uint32_t &height,
                          const uint32_t &width, MxBase::TensorBase &tensor);
    APP_ERROR Inference(const std::vector<MxBase::TensorBase> &inputs, std::vector<MxBase::TensorBase> &outputs);
    APP_ERROR PostProcess(const std::vector<MxBase::TensorBase> &outputs,const uint32_t &height,
                          const uint32_t &width, std::vector<std::vector<MxBase::ObjectInfo>> &objInfos);
private:
    std::shared_ptr<MxBase::DvppWrapper> yDvppWrapper;
    std::shared_ptr<MxBase::ModelInferenceProcessor> model;
    std::shared_ptr<MxBase::Yolov3PostProcess> post;
    MxBase::ModelDesc modelDesc = {};
    std::map<int, std::string> labelMap = {};
    uint32_t deviceId = 0;
};
#endif //STREAM_PULL_SAMPLE_YOLOV3DETECTION_H