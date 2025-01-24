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

#ifndef VIDEOGESTURERECOGNITION_RESNET_DETECTOR_H
#define VIDEOGESTURERECOGNITION_RESNET_DETECTOR_H

#include "MxBase/ErrorCode/ErrorCode.h"
#include "MxBase/DvppWrapper/DvppWrapper.h"
#include "MxBase/ModelInfer/ModelInferenceProcessor.h"
#include "ClassPostProcessors/Resnet50PostProcess.h"
#include "../BlockingQueue/BlockingQueue.h"


struct ResnetInitParam {
    uint32_t deviceId = 0;
    std::string modelPath;
    uint32_t classNum = 0;
};

class ResnetDetector {
public:
    ResnetDetector() = default;
    ~ResnetDetector() = default;

    APP_ERROR Init(const ResnetInitParam & initParam);
    APP_ERROR DeInit();
    APP_ERROR Process();
    APP_ERROR CropAndResizeFrame(const std::shared_ptr<MxBase::MemoryData> frameInfo,
                                    const uint32_t &height,const uint32_t &width, 
                                    const uint32_t &x0,const uint32_t &y0,const uint32_t &x1,const uint32_t &y1,
                                    MxBase::TensorBase &tensor);
    APP_ERROR Inference(const std::vector<MxBase::TensorBase> &inputs, std::vector<MxBase::TensorBase> &outputs);
private:
    APP_ERROR InitModel(const ResnetInitParam &initParam);
private:
    // model
    std::shared_ptr<MxBase::ModelInferenceProcessor> model;
    // infer result post process
    MxBase::ModelDesc modelDesc = {};
    // device id
    uint32_t deviceId = 1;
    // network width
    uint32_t const netWidth = 256;
    // network height
    uint32_t const netHeight = 256;

    std::shared_ptr<MxBase::DvppWrapper> rDvppWrapper;
};

#endif // VIDEOGESTURERECOGNITION_RESNET_DETECTOR_H
