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

#include <thread>
#include "MxBase/ErrorCode/ErrorCodes.h"
#include "MxBase/Log/Log.h"
#include "opencv2/opencv.hpp"
#include "VideoProcess.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
namespace {
    static AVFormatContext *formatContext = nullptr; // 视频流信息
    static int _videoIndex = -1;
    const uint32_t VIDEO_WIDTH = {1920};
    const uint32_t VIDEO_HEIGHT = {1080};
    const uint32_t MAX_QUEUE_LENGHT = 1000;
    const uint32_t QUEUE_POP_WAIT_TIME = 10;
    const uint32_t YUV_BYTE_NU = 3;
    const uint32_t YUV_BYTE_DE = 2;
}
    static int vSock = -1; // socket to send video data to client
    static int iSock = -1;
static int keypointConnectMatrix[5][5] = {
        {0, 1, 2, 3, 4}, 
        {0, 5, 6, 7, 8}, 
        {0, 9, 10, 11, 12}, 
        {0, 13, 14, 15, 16}, 
        {0, 17, 18, 19, 20}
    };
static std::string _clientIp="192.168.30.36";
APP_ERROR VideoProcess::StreamInit(const std::string &rtspUrl,const std::string &clientIp)
{
    avformat_network_init();

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "3000000", 0);
    // ffmpeg打开流媒体-视频流
    APP_ERROR ret = avformat_open_input(&formatContext, rtspUrl.c_str(), nullptr, &options);
    if (options != nullptr) {
        av_dict_free(&options);
    }
    if(ret != APP_ERR_OK){
        LogError << "Couldn't open input stream " << rtspUrl.c_str() <<  " ret = " << ret;
        return APP_ERR_STREAM_NOT_EXIST;
    }
    // 获取视频的相关信息
    ret = avformat_find_stream_info(formatContext, nullptr);
    if(ret != APP_ERR_OK){
        LogError << "Couldn't find stream information";
        return APP_ERR_STREAM_NOT_EXIST;
    }

    // formatContext 是一个表示视频文件格式的结构体，它存储了视频文件中所有的流（轨道）信息，包括视频流、音频流等
    // _videoIndex 初始值为 -1，表示尚未找到视频流
    // nb_streams 表示视频文件中包含的流的个数
    for (int i = 0; (i < formatContext->nb_streams) && (_videoIndex == -1); i++)
    // 遍历所有的multimedia container中的streams
    {
        switch (formatContext->streams[i]->codec->codec_type)
        // 第i个流的type（audio, video, subtitle，等）
        {
        case AVMEDIA_TYPE_VIDEO:
            _videoIndex = i; //找到流
            formatContext->streams[i]->discard = AVDISCARD_NONE; // 不丢弃
            break;
        default:
            formatContext->streams[i]->discard = AVDISCARD_ALL; //丢弃
            break;
        }
    }    

    // 打印视频信息
    av_dump_format(formatContext, 0, rtspUrl.c_str(), 0);
    vSock = socket(AF_INET, SOCK_DGRAM, 0);
    iSock = socket(AF_INET, SOCK_DGRAM, 0);
    _clientIp=clientIp;
    return APP_ERR_OK;
}

APP_ERROR VideoProcess::StreamDeInit()
{
    avformat_close_input(&formatContext);
    return APP_ERR_OK;
}

// 每进行一次视频帧解码会调用一次该函数，将解码后的帧信息存入对列中
APP_ERROR VideoProcess::VideoDecodeCallback(std::shared_ptr<void> buffer, MxBase::DvppDataInfo &inputDataInfo, 
                                            void *userData)
{
    auto deleter = [] (MxBase::MemoryData *mempryData) {
        if (mempryData == nullptr) {
            LogError << "MxbsFree failed";
            return;
        }
        APP_ERROR ret = MxBase::MemoryHelper::MxbsFree(*mempryData);
        delete mempryData;
        if (ret != APP_ERR_OK) {
            LogError << GetError(ret) << " MxbsFree failed";
            return;
        }
        LogInfo << "MxbsFree successfully";
    };
    // 解码后的视频信息
    auto output = std::shared_ptr<MxBase::MemoryData>(new MxBase::MemoryData(buffer.get(),
                     (size_t)inputDataInfo.dataSize, MxBase::MemoryData::MEMORY_DVPP, inputDataInfo.frameId), deleter);

    if (userData == nullptr) {
        LogError << "userData is nullptr";
        return APP_ERR_COMM_INVALID_POINTER;
    }
    auto *queue = (BlockingQueue<std::shared_ptr<void>>*)userData;
    queue->Push(output);
    return APP_ERR_OK;
}

APP_ERROR VideoProcess::VideoDecodeInit()
{
    MxBase::VdecConfig vdecConfig;
    // 将解码函数的输入格式设为H264
    vdecConfig.inputVideoFormat = MxBase::MXBASE_STREAM_FORMAT_H264_MAIN_LEVEL;
    // 将解码函数的输出格式设为YUV420
    vdecConfig.outputImageFormat = MxBase::MXBASE_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    vdecConfig.deviceId = DEVICE_ID;
    vdecConfig.channelId = CHANNEL_ID;
    vdecConfig.callbackFunc = VideoDecodeCallback;
    vdecConfig.outMode = 1;

    vDvppWrapper = std::make_shared<MxBase::DvppWrapper>();
    if (vDvppWrapper == nullptr) {
        LogError << "Failed to create dvppWrapper";
        return APP_ERR_COMM_INIT_FAIL;
    }
    APP_ERROR ret = vDvppWrapper->InitVdec(vdecConfig);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to initialize dvppWrapper";
        return ret;
    }
    return APP_ERR_OK;
}

APP_ERROR VideoProcess::VideoDecodeDeInit()
{
    APP_ERROR ret = vDvppWrapper->DeInitVdec();
    if (ret != APP_ERR_OK) {
        LogError << "Failed to deinitialize dvppWrapper";
        return ret;
    }
    return APP_ERR_OK;
}

APP_ERROR VideoProcess::VideoDecode(MxBase::MemoryData &streamData, const uint32_t &height, 
                                    const uint32_t &width, void *userData)
{
    static uint32_t frameId = 0;
    // 将帧数据从Host侧移到Device侧
    MxBase::MemoryData dvppMemory((size_t)streamData.size,
                                  MxBase::MemoryData::MEMORY_DVPP, DEVICE_ID);
    APP_ERROR ret = MxBase::MemoryHelper::MxbsMallocAndCopy(dvppMemory, streamData);
    if (ret != APP_ERR_OK) {
        LogError << "Failed to MxbsMallocAndCopy";
        return ret;
    }
    // 构建DvppDataInfo结构体以便解码
    MxBase::DvppDataInfo inputDataInfo;
    inputDataInfo.dataSize = dvppMemory.size;
    inputDataInfo.data = (uint8_t *)dvppMemory.ptrData;
    inputDataInfo.height = VIDEO_HEIGHT;
    inputDataInfo.width = VIDEO_WIDTH;
    inputDataInfo.channelId = CHANNEL_ID;
    inputDataInfo.frameId = frameId;
    ret = vDvppWrapper->DvppVdec(inputDataInfo, userData);

    if (ret != APP_ERR_OK) {
        LogError << "DvppVdec Failed";
        MxBase::MemoryHelper::MxbsFree(dvppMemory);
        return ret;
    }
    frameId++;
    return APP_ERR_OK;
}

// 获取视频帧
void VideoProcess::GetFrames(std::shared_ptr<BlockingQueue<std::shared_ptr<void>>>  blockingQueue, 
                            std::shared_ptr<VideoProcess> videoProcess)
{
    MxBase::DeviceContext device;
    device.devId = DEVICE_ID;
    APP_ERROR ret = MxBase::DeviceManager::GetInstance()->SetDevice(device);
    if (ret != APP_ERR_OK) {
        LogError << "SetDevice failed";
        return;
    }

    AVPacket pkt;
    while(!stopFlag){
        av_init_packet(&pkt);
        // 读取视频帧
        APP_ERROR ret = av_read_frame(formatContext, &pkt);
        if(ret != APP_ERR_OK){
            LogError << "Read frame failed, continue";
            if(ret == AVERROR_EOF){
                LogError << "StreamPuller is EOF, over!";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if(pkt.stream_index!=_videoIndex){
            av_packet_unref(&pkt);
            continue;
        }

        // 原始帧数据被存储在Host侧
        MxBase::MemoryData streamData((void *)pkt.data, (size_t)pkt.size,
                                      MxBase::MemoryData::MEMORY_HOST_NEW, DEVICE_ID);
        ret = videoProcess->VideoDecode(streamData, VIDEO_HEIGHT, VIDEO_WIDTH, (void*)blockingQueue.get());
        if (ret != APP_ERR_OK) {
            LogError << "VideoDecode failed";
            return;
        }
        {
         struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(6071);
            addr.sin_addr.s_addr = inet_addr(_clientIp.c_str());
       char buf[1440];
            buf[0]=0x55;
            buf[1]=0xaa;
            buf[2]=0x55;
            buf[3]=0xaa;
            int offset = 0;
            int cnt =(pkt.size+1399)/1400;
            int i = 0;
            for(i=0;i<pkt.size/1400;i++){
                memcpy(buf+4,&cnt,4);
                memcpy(buf+8,&i,4);
                memcpy(buf+40,pkt.data+offset,1400);
                sendto(vSock, buf, 1440 , 0, (struct sockaddr *)&addr, sizeof(addr));            
                if(i%4==0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                offset+=1400;
            }
            if(offset<pkt.size){
                memcpy(buf+4,&cnt,4);
                memcpy(buf+8,&i,4);
                memcpy(buf+40,pkt.data+offset,pkt.size-offset);
                sendto(vSock, buf,pkt.size-offset+40, 0, (struct sockaddr *)&addr, sizeof(addr));            
            }            
        }
        av_packet_unref(&pkt);
    }
    av_packet_unref(&pkt);
}

APP_ERROR VideoProcess::SaveResult(std::shared_ptr<MxBase::MemoryData> resultInfo, const uint32_t frameId,
                     const std::vector<MxBase::ObjectInfo>& objInfos,
                     const std::vector<MxBase::TensorBase>& keyPointInfos)
{
    // 将推理结果从Device侧移到Host侧
    MxBase::MemoryData memoryDst(resultInfo->size,MxBase::MemoryData::MEMORY_HOST_NEW);
    APP_ERROR ret = MxBase::MemoryHelper::MxbsMallocAndCopy(memoryDst, *resultInfo);
    if(ret != APP_ERR_OK){
        LogError << "Fail to malloc and copy host memory.";
        return ret;
    }
    // 初始化OpenCV图像信息矩阵
    cv::Mat imgYuv = cv::Mat(VIDEO_HEIGHT* YUV_BYTE_NU / YUV_BYTE_DE, VIDEO_WIDTH, CV_8UC1, memoryDst.ptrData);
    cv::Mat imgBgr = cv::Mat(VIDEO_HEIGHT, VIDEO_WIDTH, CV_8UC3);
    // 颜色空间转换
    cv::cvtColor(imgYuv, imgBgr, cv::COLOR_YUV2BGR_NV12);
    for (uint32_t i = 0; i < objInfos.size(); i++) {
        MxBase::ObjectInfo oi = objInfos[i];
        MxBase::TensorBase kpi = keyPointInfos[i];
        int ow = oi.x1 - oi.x0;
        int oh = oi.y1 - oi.y0;        

        const cv::Scalar green = cv::Scalar(0, 255, 0);
        const cv::Scalar red = cv::Scalar(0, 0, 255);
        const uint32_t thickness = 4;
        const uint32_t xOffset = 10;
        const uint32_t yOffset = 10;
        const uint32_t lineType = 8;
        const float fontScale = 1.0;

        // 绘制矩形
        cv::rectangle(imgBgr,cv::Rect(oi.x0, oi.y0, ow, oh),green, thickness);
        
        float* ptr = (float*)kpi.GetBuffer();            
        int arrSize = kpi.GetSize()/2;
        int x[21],y[21];
        for(int j=0;j<arrSize;j++){
            float fx = *(ptr++);
            float fy = *(ptr++);
            x[j] = (fx*ow)+oi.x0;
            y[j] = (fy*oh)+oi.y0;
            LogInfo << "keypoints[ " << j << "]:" << x << "," << y ;
        }
        for(int m=0;m<5;m++){
            int from = 0;
            int to = m*4+1;
            cv::line(imgBgr,cv::Point(x[from],y[from]),cv::Point(x[to],y[to]),red);
            for(int n=0;n<4;n++){
                from=to;
                to++;
                cv::line(imgBgr,cv::Point(x[from],y[from]),cv::Point(x[to],y[to]),red);
            }
        }

    }
    std::string fileName = "./result/result" + std::to_string(frameId+1) + ".jpg";
    cv::imwrite(fileName, imgBgr);
    ret = MxBase::MemoryHelper::MxbsFree(memoryDst);
    if(ret != APP_ERR_OK){
        LogError << "Fail to MxbsFree memory.";
        return ret;
    }
    return APP_ERR_OK;
}

void VideoProcess::GetResults(std::shared_ptr<BlockingQueue<std::shared_ptr<void>>> blockingQueue, 
                              std::shared_ptr<Yolov3Detection> yolov3Detection,
                              std::shared_ptr<ResnetDetector> resnetDetection, 
                              std::shared_ptr<VideoProcess> videoProcess)
{
    uint32_t frameId = 0;
    MxBase::DeviceContext device;
    device.devId = DEVICE_ID;
    APP_ERROR ret = MxBase::DeviceManager::GetInstance()->SetDevice(device);
    if (ret != APP_ERR_OK) {
        LogError << "SetDevice failed";
        return;
    }
    int noObjCnt = 0;
    struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(6072);
            addr.sin_addr.s_addr = inet_addr(_clientIp.c_str());
 
    while (!stopFlag) {
        std::shared_ptr<void> data = nullptr;
        // 从队列中去出解码后的帧数据
        APP_ERROR ret = blockingQueue->Pop(data, QUEUE_POP_WAIT_TIME);
        if (ret != APP_ERR_OK) {
            LogError << "Pop failed";
            return;
        }
        struct timeval tv0,tv1;
         gettimeofday(&tv0,NULL);
        LogInfo << "get result:";

        MxBase::TensorBase resizeFrame;
        auto result = std::make_shared<MxBase::MemoryData>();
        result = std::static_pointer_cast<MxBase::MemoryData>(data);

        // 图像缩放
        ret = yolov3Detection->ResizeFrame(result, VIDEO_HEIGHT, VIDEO_WIDTH,resizeFrame);
        if (ret != APP_ERR_OK) {
            LogError << "Resize failed";
            return;
        }

        std::vector<MxBase::TensorBase> inputs = {};
        std::vector<MxBase::TensorBase> outputs = {};
        inputs.push_back(resizeFrame);
        // 推理
        ret = yolov3Detection->Inference(inputs, outputs);
        if (ret != APP_ERR_OK) {
            LogError << "Inference failed, ret=" << ret << ".";
            return;
        }

        std::vector<std::vector<MxBase::ObjectInfo>> objInfos;
        // 后处理
        ret = yolov3Detection->PostProcess(outputs, VIDEO_HEIGHT, VIDEO_WIDTH, objInfos);
        if (ret != APP_ERR_OK) {
            LogError << "PostProcess failed, ret=" << ret << ".";
            return;
        }

        if(objInfos.size()==0){
            frameId++;
            return;
        }



        // model infer
        std::vector<MxBase::ObjectInfo> info;
        std::vector<MxBase::TensorBase> keyPointInfos;    
        int maxConfIdx = -1;  
        float maxConfidenceGlobal = 0;      
        for (uint32_t i = 0; i < objInfos.size(); i++) {
            if(objInfos[i].size()==0) continue;
            float maxConfidence = 0;
            uint32_t index;
            for (uint32_t j = 0; j < objInfos[i].size(); j++) {
                if (objInfos[i][j].confidence > maxConfidence) {
                    maxConfidence = objInfos[i][j].confidence;
                    index = j;
                    
                }
            }

            info.push_back(objInfos[i][index]);
            // 打印置信度最大推理结果
            LogInfo << "id: " << info[i].classId << "; lable: " << info[i].className
                << "; confidence: " << info[i].confidence
                << "; box: [ (" << info[i].x0 << "," << info[i].y0 << ") "
                << "(" << info[i].x1 << "," << info[i].y1 << ") ]";
        }
        for (uint32_t i = 0; i < info.size(); i++){
            if(info[i].confidence>maxConfidenceGlobal){
                maxConfidenceGlobal = info[i].confidence;
                maxConfIdx = i;
            } 
        }
        if(maxConfIdx==-1) {
            noObjCnt++;
            if(noObjCnt>10){
               char buf[1040];
                memset(buf,0,40);
                sendto(iSock, buf, 40 , 0, (struct sockaddr *)&addr, sizeof(addr));
                noObjCnt = 0;
            }

            continue;
        }
        noObjCnt = 0;
        MxBase::ObjectInfo obj = info[maxConfIdx];
        int w = obj.x1 - obj.x0;
        int h = obj.y1 - obj.y0;
        obj.x0 = (obj.x1+obj.x0)/2 - w*3/4;
        obj.x1 = (obj.x1+obj.x0)/2 + w*3/4;
        obj.y0 = (obj.y1+obj.y0)/2 - h*3/4;
        obj.y1 = (obj.y1+obj.y0)/2 + h*3/4;
        if(obj.x0<0) obj.x0 = 0;
        if(obj.y0<0) obj.y0 = 0;
        if(obj.x1>=VIDEO_WIDTH-1) obj.x1 = VIDEO_WIDTH-1;
        if(obj.y1>=VIDEO_HEIGHT-1) obj.y1 = VIDEO_HEIGHT-1;
        MxBase::TensorBase keypoint;

        MxBase::TensorBase cropFrame;
        ret = resnetDetection->CropAndResizeFrame(result, VIDEO_HEIGHT, VIDEO_WIDTH, obj.x0, obj.y0, obj.x1, obj.y1, cropFrame);
        if (ret != APP_ERR_OK)
        {
            LogError << "Resize failed";
            return;
        }
        std::vector<MxBase::TensorBase> rinputs = {};
        std::vector<MxBase::TensorBase> routputs = {};
        rinputs.push_back(cropFrame);

        LogInfo << "resnet input tensor" << cropFrame.GetDesc();
        ret = resnetDetection->Inference(rinputs, routputs);
        LogInfo << "resnet output tensor" << routputs[0].GetDesc();


        // 结果可视化
        /*
        ret = videoProcess->SaveResult(result, frameId, info,keyPointInfos);
        if (ret != APP_ERR_OK) {
            LogError << "Save result failed, ret=" << ret << ".";
            return;
        }
        */

        {
           char buf[1040];
            int offset = 40;
            {
                MxBase::TensorBase tensor = routputs[0];
                int x0 = obj.x0;
                int x1 = obj.x1;
                int y0 = obj.y0;
                int y1 = obj.y1;
                memcpy(buf+offset,&x0,4);
                offset+=4;
                memcpy(buf+offset,&y0,4);
                offset+=4;
                memcpy(buf+offset,&x1,4);
                offset+=4;
                memcpy(buf+offset,&y1,4);
                offset+=4;
                float* ptr = (float*)tensor.GetBuffer();
                short ow = x1 - x0;
                short oh = y1 - y0;
                for(int j=0;j<tensor.GetSize()/2;j++){
                    float fx = *(ptr++);
                    float fy = *(ptr++);
                    short x = (fx*ow)+x0;
                    short y = (fy*oh)+y0;
                    memcpy(buf+offset,&x,2);
                    offset+=2;
                    memcpy(buf+offset,&y,2);
                    offset+=2;
                }
            }
            sendto(iSock, buf, offset , 0, (struct sockaddr *)&addr, sizeof(addr));            
        }
        frameId++;
        gettimeofday(&tv1,NULL);
        LogInfo << "got hand cost:" << (tv1.tv_sec-tv0.tv_sec)*1000000+(tv1.tv_usec-tv0.tv_usec);
    }
}
