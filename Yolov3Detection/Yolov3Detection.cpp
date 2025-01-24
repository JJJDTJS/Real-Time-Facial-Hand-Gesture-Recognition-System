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
#include "MxBase/ErrorCode/ErrorCodes.h"
#include "MxBase/Log/Log.h"
#include "Yolov3Detection.h"

namespace {
    const uint32_t YUV_BYTE_NU = 3;
    const uint32_t YUV_BYTE_DE = 2;
    const uint32_t VPC_H_ALIGN = 2;
}

// 加载标签文件
APP_ERROR Yolov3Detection::LoadLabels(const std::string &labelPath, std::map<int, std::string> &labelMap)
{
    std::ifstream infile;
    // open label file
    infile.open(labelPath, std::ios_base::in);
    std::string s;
    // check label file validity
    if (infile.fail()) {
        LogError << "Failed to open label file: " << labelPath << ".";
        return APP_ERR_COMM_OPEN_FAIL;
    }
    labelMap.clear();
    // construct label map
    int count = 0;
    while (std::getline(infile, s)) {
        if (s.find('#') <= 1) {
            continue;
        }
        size_t eraseIndex = s.find_last_not_of("\r\n\t");
        if (eraseIndex != std::string::npos) {
            s.erase(eraseIndex + 1, s.size() - eraseIndex);
        }
        labelMap.insert(std::pair<int, std::string>(count, s));
        count++;
    }
    infile.close();
    return APP_ERR_OK;
}

// 设置配置参数
void Yolov3Detection::SetYolov3PostProcessConfig(const InitParam &initParam, std::map<std::string, 
                                                std::shared_ptr<void>> &config)
{

    MxBase::ConfigData configData;
    const std::string checkTensor = initParam.checkTensor ? "true" : "false";
    configData.SetJsonValue("CLASS_NUM", std::to_string(initParam.classNum));
    configData.SetJsonValue("BIASES_NUM", std::to_string(initParam.biasesNum));
    configData.SetJsonValue("BIASES", initParam.biases);
    configData.SetJsonValue("OBJECTNESS_THRESH", initParam.objectnessThresh);
    configData.SetJsonValue("IOU_THRESH", initParam.iouThresh);
    configData.SetJsonValue("SCORE_THRESH", initParam.scoreThresh);
    configData.SetJsonValue("YOLO_TYPE", std::to_string(initParam.yoloType));
    configData.SetJsonValue("MODEL_TYPE", std::to_string(initParam.modelType));
    configData.SetJsonValue("INPUT_TYPE", std::to_string(initParam.inputType));
    configData.SetJsonValue("ANCHOR_DIM", std::to_string(initParam.anchorDim));
    configData.SetJsonValue("CHECK_MODEL", checkTensor);

    auto jsonStr = configData.GetCfgJson().serialize();
    config["postProcessConfigContent"] = std::make_shared<std::string>(jsonStr);
    config["labelPath"] = std::make_shared<std::string>(initParam.labelPath);
}

APP_ERROR Yolov3Detection::FrameInit(const InitParam &initParam)
{
    deviceId = initParam.deviceId;
    APP_ERROR ret = MxBase::DeviceManager::GetInstance()->InitDevices();
    if (ret != APP_ERR_OK) {
        LogError << "Init devices failed, ret=" << ret << ".";
        return ret;
    }
    ret = MxBase::TensorContext::GetInstance()->SetContext(initParam.deviceId);
    if (ret != APP_ERR_OK) {
        LogError << "Set context failed, ret=" << ret << ".";
        return ret;
    }
    yDvppWrapper = std::make_shared<MxBase::DvppWrapper>();
    ret = yDvppWrapper->Init();
    if (ret != APP_ERR_OK) {
        LogError << "DvppWrapper init failed, ret=" << ret << ".";
        return ret;
    }
    model = std::make_shared<MxBase::ModelInferenceProcessor>();
    LogInfo << "model path: " << initParam.modelPath;
    ret = model->Init(initParam.modelPath, modelDesc);
    if (ret != APP_ERR_OK) {
        LogError << "ModelInferenceProcessor init failed, ret=" << ret << ".";
        return ret;
    }

    std::map<std::string, std::shared_ptr<void>> config;
    SetYolov3PostProcessConfig(initParam, config);
    post = std::make_shared<MxBase::Yolov3PostProcess>();
    ret = post->Init(config);
    if (ret != APP_ERR_OK) {
        LogError << "Yolov3PostProcess init failed, ret=" << ret << ".";
        return ret;
    }
    // load labels from file
    ret = LoadLabels(initParam.labelPath, labelMap);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to load labels, ret=" << ret << ".";
        return ret;
    }
    return APP_ERR_OK;
}

APP_ERROR Yolov3Detection::FrameDeInit()
{
    yDvppWrapper->DeInit();
    model->DeInit();
    post->DeInit();
    MxBase::DeviceManager::GetInstance()->DestroyDevices();
    return APP_ERR_OK;
}

APP_ERROR Yolov3Detection::ResizeFrame(const std::shared_ptr<MxBase::MemoryData> frameInfo, const uint32_t &height,
                                       const uint32_t &width, MxBase::TensorBase &tensor)
{
    // 视频帧的原始数据
    MxBase::DvppDataInfo input = {};
    input.height = height;
    input.width = width;
    input.heightStride = height;
    input.widthStride = width;
    input.dataSize = frameInfo->size;
    input.data = (uint8_t*)frameInfo->ptrData;

    const uint32_t resizeHeight = 416;
    const uint32_t resizeWidth = 416;
    MxBase::ResizeConfig resize = {};
    resize.height = resizeHeight;
    resize.width = resizeWidth;

    MxBase::DvppDataInfo output = {};
    // 图像缩放
    APP_ERROR ret = yDvppWrapper->VpcResize(input, output, resize);
    if(ret != APP_ERR_OK){
        LogError << GetError(ret) << "VpcResize failed.";
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

APP_ERROR Yolov3Detection::Inference(const std::vector<MxBase::TensorBase> &inputs,
                                  std::vector<MxBase::TensorBase> &outputs)
{
    auto dtypes = model->GetOutputDataType();
    for (size_t i = 0; i < modelDesc.outputTensors.size(); ++i) {
        std::vector<uint32_t> shape = {};
        // yolov3模型3个检测特征图尺寸（13 * 13 26 * 26 52 *52）
        for (size_t j = 0; j < modelDesc.outputTensors[i].tensorDims.size(); ++j) {
            shape.push_back((uint32_t)modelDesc.outputTensors[i].tensorDims[j]);
        }
        // 用3个检测特征图尺寸分别构建3个数据为空的tensor
        MxBase::TensorBase tensor(shape, dtypes[i], MxBase::MemoryData::MemoryType::MEMORY_DVPP, deviceId);
        APP_ERROR ret = MxBase::TensorBase::TensorBaseMalloc(tensor);
        if (ret != APP_ERR_OK) {
            LogError << "TensorBaseMalloc failed, ret=" << ret << ".";
            return ret;
        }
        outputs.push_back(tensor);
    }

    MxBase::DynamicInfo dynamicInfo = {};
    // 设置类型为静态batch
    dynamicInfo.dynamicType = MxBase::DynamicType::STATIC_BATCH;
    // 记录推理操作的开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    if(inputs[0].GetBuffer() == nullptr){
        LogError << "input is null";
        return APP_ERR_FAILURE;
    }
    APP_ERROR ret = model->ModelInference(inputs, outputs, dynamicInfo);
    // 记录推理操作的结束时间
    auto endTime = std::chrono::high_resolution_clock::now();
    double costMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    g_inferCost.push_back(costMs);
    if (ret != APP_ERR_OK) {
        LogError << "ModelInference failed, ret=" << ret << ".";
        return ret;
    }
    return APP_ERR_OK;
}

APP_ERROR Yolov3Detection::PostProcess(const std::vector<MxBase::TensorBase> &outputs,const uint32_t &height,
                                       const uint32_t &width, std::vector<std::vector<MxBase::ObjectInfo>> &objInfos)
{
    // 构建ResizedImageInfo
    MxBase::ResizedImageInfo imgInfo;
    imgInfo.widthOriginal = width;
    imgInfo.heightOriginal = height;
    imgInfo.widthResize = 416;
    imgInfo.heightResize = 416;
    imgInfo.resizeType = MxBase::RESIZER_STRETCHING;
    std::vector<MxBase::ResizedImageInfo> imageInfoVec = {};
    imageInfoVec.push_back(imgInfo);
    APP_ERROR ret = post->Process(outputs, objInfos, imageInfoVec);
    if (ret != APP_ERR_OK) {
        LogError << "Process failed, ret=" << ret << ".";
        return ret;
    }
    return APP_ERR_OK;
}