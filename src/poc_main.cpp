#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>

using namespace cv;
using namespace std;

int main() {
    const string modelPath  = "model/yolo_v8_nano_seg.onnx";
    const string imagePath  = "images/rock_03.png";
    const string outputPath = "output_detections.png";
    const string txtPath    = "outputs.txt";

    const float CONF_THRESH = 0.80f;
    const float NMS_THRESH  = 0.20f;
    const int   INPUT_W     = 640;
    const int   INPUT_H     = 640;
    
    // SCALE FACTOR : 64cm / 640px = 0.1 cm/px
    const float K_SCALE     = 0.1f; 

    Mat frame = imread(imagePath);
    if (frame.empty()) {
        cerr << "Error: No se pudo cargar la imagen." << endl;
        return -1;
    }

    float scaleX = (float)frame.cols / INPUT_W;
    float scaleY = (float)frame.rows / INPUT_H;

   // 2. Preprocess: resize -> normalize -> HWC to NCHW
    Mat resized;
    resize(frame, resized, Size(INPUT_W, INPUT_H));
    resized.convertTo(resized, CV_32F, 1.0 / 255.0);

    vector<float> inputData(1 * 3 * INPUT_H * INPUT_W);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < INPUT_H; ++h)
            for (int w = 0; w < INPUT_W; ++w)
                inputData[c * INPUT_H * INPUT_W + h * INPUT_W + w] = resized.at<Vec3f>(h, w)[c];

    // 3. ONNX Runtime session
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "RockPulsePoC");
    Ort::SessionOptions sessionOpts;
    sessionOpts.SetIntraOpNumThreads(1);
    Ort::Session session(env, modelPath.c_str(), sessionOpts);
    Ort::AllocatorWithDefaultOptions allocator;

    auto inputName = session.GetInputNameAllocated(0, allocator);
    auto out0Name  = session.GetOutputNameAllocated(0, allocator);
    auto out1Name  = session.GetOutputNameAllocated(1, allocator);

    // 4. Run inference
    array<int64_t, 4> inputShape = {1, 3, INPUT_H, INPUT_W};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor  = Ort::Value::CreateTensor<float>(
        memInfo, inputData.data(), inputData.size(),
        inputShape.data(), inputShape.size());

    const char* inputNames[]  = { inputName.get() };
    const char* outputNames[] = { out0Name.get(), out1Name.get() };

    auto outputs = session.Run(Ort::RunOptions{nullptr},
                               inputNames, &inputTensor, 1,
                               outputNames, 2);

    // 5. Parse output0
    float* data0    = outputs[0].GetTensorMutableData<float>();
    auto    shape0   = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    int     numPreds = (int)shape0[2]; 

    vector<Rect>  boxes;
    vector<float> scores;
    vector<vector<float>> maskCoeffs; // NUEVO: Para guardar los 32 coeficientes

    for (int i = 0; i < numPreds; ++i) {
        float conf = data0[4 * numPreds + i];
        if (conf < CONF_THRESH) continue;

        float cx = data0[0 * numPreds + i];
        float cy = data0[1 * numPreds + i];
        float w  = data0[2 * numPreds + i];
        float h  = data0[3 * numPreds + i];

        int x1 = (int)((cx - w / 2.0f) * scaleX);
        int y1 = (int)((cy - h / 2.0f) * scaleY);
        int bw = (int)(w * scaleX);
        int bh = (int)(h * scaleY);

        x1 = max(0, min(x1, frame.cols - 1));
        y1 = max(0, min(y1, frame.rows - 1));
        bw = max(1, min(bw, frame.cols - x1));
        bh = max(1, min(bh, frame.rows - y1));

        boxes.push_back(Rect(x1, y1, bw, bh));
        scores.push_back(conf);

        // GET ALL 32 MASK COEFFICIENTS FOR THIS PREDICTION
        vector<float> coeffs(32);
        for (int c = 0; c < 32; ++c) {
            coeffs[c] = data0[(5 + c) * numPreds + i];
        }
        maskCoeffs.push_back(coeffs);
    }

    cout << "Raw candidates above " << CONF_THRESH << ": " << boxes.size() << endl;

    // 6. Non-Maximum Suppression
    vector<int> indices;
    dnn::NMSBoxes(boxes, scores, CONF_THRESH, NMS_THRESH, indices);
    cout << "Detections after NMS: " << indices.size() << endl;

    ofstream outFile(txtPath);
    if (!outFile.is_open()) {
        cerr << "Error al crear outputs.txt" << endl;
        return -1;
    }

    outFile << "ID,Area_px,L_px,W_px,Area_cm2,L_cm,W_cm,Sphericity,Volume_cm3" << endl;

    // REbuild protoMap from output1
    float* data1 = outputs[1].GetTensorMutableData<float>();
    Mat protoMap(32, 160 * 160, CV_32F, data1);

    int rockCount = 0;
    for (int idx : indices) {
        rockCount++;
        const Rect& box = boxes[idx];
        
        // --- MASK ASSEMBLY 
        Mat coeff(1, 32, CV_32F, maskCoeffs[idx].data());
        Mat maskMat = Mat(coeff * protoMap).reshape(1, 160);
        cv::exp(-maskMat, maskMat);
        maskMat = 1.0f / (1.0f + maskMat);

        Mat maskOriginal;
        resize(maskMat, maskOriginal, Size(frame.cols, frame.rows));
        Mat binMask;
        threshold(maskOriginal(box), binMask, 0.5, 255, THRESH_BINARY);
        binMask.convertTo(binMask, CV_8U);

        // --- GEOMETRÍA AVANZADA ---
        vector<vector<Point>> contours;
        findContours(binMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        if (contours.empty()) continue;

        // Encontrar el contorno más grande (la roca principal)
        auto it = max_element(contours.begin(), contours.end(), 
                  [](const vector<Point>& a, const vector<Point>& b) {
                      return contourArea(a) < contourArea(b);
                  });

        // 1. Rectángulo Rotado (OBB)
        RotatedRect rRect = minAreaRect(*it);
        float L_px = max(rRect.size.width, rRect.size.height);
        float W_px = min(rRect.size.width, rRect.size.height);

        // 2. Área y Perímetro en px
        double area_px = countNonZero(binMask);
        double peri_px = arcLength(*it, true);

        // 3. Conversión a CM
        float L_cm = L_px * K_SCALE;
        float W_cm = W_px * K_SCALE;
        float area_cm2 = area_px * (K_SCALE * K_SCALE);

        // 4. Esfericidad (Cox Index)
        double sphericity = (4 * CV_PI * area_px) / (peri_px * peri_px);

        // 5. Volumen Elipsoide (H = promedio de L y W)
        float H_cm = (L_cm + W_cm) / 2.0f;
        float volume_cm3 = (4.0f/3.0f) * CV_PI * (L_cm/2.0f) * (W_cm/2.0f) * (H_cm/2.0f);

        // --- GUARDAR RESULTADOS ---
        outFile << rockCount << "," << area_px << "," << L_px << "," << W_px << ","
                << area_cm2 << "," << L_cm << "," << W_cm << "," 
                << sphericity << "," << volume_cm3 << endl;

        // Dibujar en la imagen
        rectangle(frame, box, Scalar(0, 255, 0), 2);
        string info = "V: " + to_string((int)volume_cm3) + " cm3";
        putText(frame, info, Point(box.x, box.y - 5), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1);
    }

    outFile.close();
    imwrite(outputPath, frame);
    cout << "Proceso terminado. Datos guardados en " << txtPath << endl;

    return 0;
}