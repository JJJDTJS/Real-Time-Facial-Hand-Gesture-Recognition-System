# Real-Time Facial & Hand Gesture Recognition System

[![License: Custom License](https://img.shields.io/badge/License-Custom-red)]()
[![Professional Project](https://img.shields.io/badge/Project-Professional-green)](https://www.topmoo-tech.com/)

## 📌 Overview

This repository contains the implementation of a **real-time facial and hand gesture recognition system**, developed as part of a professional project at **Topmoo Tech, LTD**. The system is built on the **Atlas 200 DK** platform using advanced AI technologies like **YOLOv3** and **ResNet**, achieving exceptional performance for facial and gesture detection tasks.

---

## 🎯 Core Features

- **Real-Time Processing**  
  Achieves sub-0.5s end-to-end latency with optimized multi-threaded pipelines for RTSP streaming.

- **Facial & Hand Gesture Recognition**  
  Utilizes **YOLOv3** for detection and **ResNet** for keypoint estimation, delivering high accuracy and efficiency.

- **High-Volume Frame Decoding**  
  Implements **BlockingQueue** and **MxBase dvpp** for smooth video decoding with minimal overhead.

- **Modular & Scalable Design**  
  A production-ready codebase, easily extendable for other AI-based detection tasks.

---


## 🔄 Demo and Visuals

### 1. Real-Time Gesture Detection
Below are sample images showcasing the system's gesture recognition capabilities:


| ![Gesture 1](https://github.com/user-attachments/assets/cf5db15b-7bf4-427b-9b9b-02fc93ba8e1b) | ![Gesture 2](https://github.com/user-attachments/assets/55a330c9-7320-43bd-a66b-2e4b9402529a) | ![Gesture 3](https://github.com/user-attachments/assets/c0f6b03c-a892-4fbd-b6e4-ad94feb5f1a0) |
|-------------------------------------|-------------------------------------|-------------------------------------|
| **Thumbs Up**                      | **Open Hand**                      | **Pointing**                        |

Each image demonstrates the bounding box (green) and keypoint skeleton (red) detected in real time.

---

## 🚀 System Highlights

1. **Hardware Optimization**  
   Fine-tuned inference pipelines and environment variables for **Ascend hardware acceleration**, achieving a **30% boost** in workflow efficiency.

2. **Streaming Capabilities**  
   - Supports **RTSP input** and real-time video frame analysis.  
   - Designed to handle high-volume data streams without performance degradation.

3. **Key Technologies**  
   - **Detection Models**: YOLOv3 and ResNet for multi-task learning.  
   - **Multithreading**: Efficiently handles video streams and inference.  
   - **OpenCV Integration**: Enables result visualization and rendering.  

---

## 📋 Project Goals

1. **Achieve Real-Time Performance**  
   Deliver sub-500ms latency for seamless user interaction.

2. **Optimize Hardware Utilization**  
   Maximize resource efficiency on Atlas 200 DK for scalable deployment.

3. **Enhance Modular Design**  
   Build reusable components for integration into diverse AI workflows.

---

## 🔍 Project Structure

```plaintext
📦 Real-Time Facial & Hand Gesture Recognition System
🔶 BlockingQueue                # Multi-threaded queue implementation
🔶 ResnetDetector               # ResNet-based keypoint detection module
🔶 VideoProcess                 # Video stream decoding and processing
🔶 Yolov3Detection              # YOLOv3-based object detection module
🔶 model                        # Pre-trained YOLOv3 and ResNet models
🔶 result                       # Inference result images
🔶 build                        # Build directory for compiled binaries
🔶 main.cpp                     # Main application entry point
🔶 CMakeLists.txt               # CMake build configuration
🔶 README.md                    # Project documentation
🔶 run.sh                       # Shell script for running the application
🔶 test.sh                      # Shell script for testing
```

---


## 📁 Data Files
Due to size constraints, the data folder `OSMDataExports` is hosted externally. You can download it using the link below:
🔗 [Download model Folder](https://utoronto-my.sharepoint.com/:f:/g/personal/jeremyjianzi_zhang_mail_utoronto_ca/Ej1ZS-h3mKVNgD0xM3NHJlkBdb-TLiPkReVVeEOM8aaM_g?e=G6fnCG)


---

## 🔧 Setup and Usage

### Prerequisites
- **Hardware**: Atlas 200 DK
- **Dependencies**: OpenCV, MxBase, C++17, FFmpeg

### Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/real-time-gesture-recognition.git
   cd real-time-gesture-recognition
   ```

2. Install required libraries:
   ```bash
   sudo apt-get install libopencv-dev ffmpeg
   ```

3. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

### Running the Application
1. Full code and documentation will be given upon request

---

## ⚖️ Custom License

**Copyright © 2025 Topmoo Tech, LTD. All rights reserved.**

This project is licensed under a **Custom License**. The following restrictions apply:

1. **No Copying**: Unauthorized copying, redistribution, or reproduction of the source code or its components in any form is prohibited.
2. **No Commercial Use**: The software may not be used for commercial purposes.
3. **No Modification**: Modifications to the source code or its components are not allowed without explicit authorization.


---

## 🔗 References

1. [YOLOv3 Paper](https://arxiv.org/abs/1804.02767)  
2. [ResNet Paper](https://arxiv.org/abs/1512.03385)  
3. [MxBase Documentation](https://www.hiascend.com/en/software/mindx-sdk/community/)
