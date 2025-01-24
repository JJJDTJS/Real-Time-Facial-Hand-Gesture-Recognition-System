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

#include "ResnetDetector.h"
#include "MxBase/Tensor/TensorContext/TensorContext.h"


namespace {
    const uint32_t YUV_BYTE_NU = 3;
    const uint32_t YUV_BYTE_DE = 2;
    const uint32_t VPC_H_ALIGN = 2;
}

APP_ERROR ResnetDetector::Init(const ResnetInitParam &initParam)
{
    LogDebug << "ResnetDetector init start.";
    this->deviceId = initParam.deviceId;

    // set tensor context
    APP_ERROR ret = MxBase::TensorContext::GetInstance()->SetContext(initParam.deviceId);
    if (ret != APP_ERR_OK) {
        LogError << "Set tensor context failed, ret = " << ret << ".";
        return ret;
    }
    rDvppWrapper = std::make_shared<MxBase::DvppWrapper>();
    ret = rDvppWrapper->Init();
    if (ret != APP_ERR_OK) {
        LogError << "DvppWrapper init failed, ret=" << ret << ".";
        return ret;
    }
    // Init Resnet model
    ret = InitModel(initParam);
    if (ret != APP_ERR_OK) {
        LogError << "init model failed.";
        return ret;
    }
    LogDebug << "ResnetDetector init successful.";
    return APP_ERR_OK;
}

APP_ERROR ResnetDetector::DeInit()
{
    LogDebug << "ResnetDetector deinit start.";

    APP_ERROR ret = model->DeInit();
    if (ret != APP_ERR_OK) {
        LogError << "deinit model failed";
        return ret;
    }
    rDvppWrapper->DeInit();
    LogDebug << "ResnetDetector deinit successful.";
    return APP_ERR_OK;
}

APP_ERROR ResnetDetector::Process()
{
    return APP_ERR_OK;
}

/// ========== private Method ========== ///

APP_ERROR ResnetDetector::InitModel(const ResnetInitParam &initParam)
{
    LogDebug << "ResnetDetector init model start.";
    model = std::make_shared<MxBase::ModelInferenceProcessor>();
    LogInfo << "model path: " << initParam.modelPath;

    APP_ERROR ret = model->Init(initParam.modelPath, modelDesc);
    if (ret != APP_ERR_OK) {
        LogError << "ModelInferenceProcessor init failed, ret=" << ret << ".";
        return ret;
    }

    LogDebug << "ResnetDetector init model successfully.";
    return APP_ERR_OK;
}


APP_ERROR ResnetDetector::Inference(const std::vector<MxBase::TensorBase> &inputs,
                                    std::vector<MxBase::TensorBase> &outputs)
{
    APP_ERROR ret;

    // check input
    if (inputs.empty() || inputs[0].GetBuffer() == nullptr) {
        LogError << "input is null";
        return APP_ERR_FAILURE;
    }

    auto dtypes = model->GetOutputDataType();
    for (size_t i = 0; i < modelDesc.outputTensors.size(); ++i) {
        std::vector<uint32_t> shape = {};
        for (size_t j = 0; j < modelDesc.outputTensors[i].tensorDims.size(); ++j) {
            shape.push_back((uint32_t)modelDesc.outputTensors[i].tensorDims[j]);
        }

        MxBase::TensorBase tensor(shape, dtypes[i], MxBase::MemoryData::MemoryType::MEMORY_DVPP, deviceId);
        ret = MxBase::TensorBase::TensorBaseMalloc(tensor);
        if (ret != APP_ERR_OK) {
            LogError << "TensorBaseMalloc failed, ret = " << ret << ".";
            return ret;
        }

        outputs.push_back(tensor);
    }

    LogInfo << outputs[0].GetDesc();

    MxBase::DynamicInfo dynamicInfo = {};
    dynamicInfo.dynamicType = MxBase::DynamicType::STATIC_BATCH;

    // model infer
    auto startTime = std::chrono::high_resolution_clock::now();
    ret = model->ModelInference(inputs, outputs, dynamicInfo);
    auto endTime = std::chrono::high_resolution_clock::now();
    double costMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    LogInfo << "model inference time: " << costMs;

    if (ret != APP_ERR_OK) {
        LogError << "ModelInference failed, ret=" << ret << ".";
        return ret;
    }

    return APP_ERR_OK;
}




APP_ERROR ResnetDetector::CropAndResizeFrame(const std::shared_ptr<MxBase::MemoryData> frameInfo,
                                    const uint32_t &height,const uint32_t &width, 
                                    const uint32_t &x0,const uint32_t &y0,const uint32_t &x1,const uint32_t &y1,
                                    MxBase::TensorBase &tensor)
{
    // 视频帧的原始数据
    MxBase::DvppDataInfo input = {};
    input.height = height;
    input.width = width;
    input.heightStride = height;
    input.widthStride = width;
    input.dataSize = frameInfo->size;
    input.data = (uint8_t*)frameInfo->ptrData;

    const uint32_t resizeHeight = 256;
    const uint32_t resizeWidth = 256;
    MxBase::CropRoiConfig crop = {};
    crop.x0=x0;
    crop.x1=x1;
    crop.y0=y0;
    crop.y1=y1;
    MxBase::ResizeConfig resize = {};
    resize.height = resizeHeight;
    resize.width = resizeWidth;
    MxBase::DvppDataInfo tmp;
    MxBase::DvppDataInfo output;


    // 图像缩放
    APP_ERROR ret = rDvppWrapper->VpcCrop(input, tmp,crop);
    if(ret != APP_ERR_OK){
        LogError << GetError(ret) << "VpcCrop failed.";
        return ret;
    }
    ret = rDvppWrapper->VpcResize(tmp, output,resize);
    if(ret != APP_ERR_OK){
        LogError << GetError(ret) << "VpcCrop failed.";
        return ret;
    }    

    // 缩放后的图像信息
    MxBase::MemoryData outMemoryData((void*)output.data, output.dataSize, MxBase::MemoryData::MEMORY_DVPP, deviceId);
    // 对缩放后图像对齐尺寸进行判定
    if (output.heightStride % VPC_H_ALIGN != 0) {
        LogError << "Output data height(" << output.heightStride << ") can't be divided by " << VPC_H_ALIGN << ".";
        MxBase::MemoryHelper::MxbsFree(outMemoryData);
        return APP_ERR_COMM_INVALID_PARAM;
    }
    std::vector<uint32_t> shape = {output.heightStride * YUV_BYTE_NU / YUV_BYTE_DE, output.widthStride};
    tensor = MxBase::TensorBase(outMemoryData, false, shape, MxBase::TENSOR_DTYPE_UINT8);

    return APP_ERR_OK;
}