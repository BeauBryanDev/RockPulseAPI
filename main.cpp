#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

int main() {
    const string modelPath  = "model/yolo_v8_nano_seg.onnx";
    const string imagePath  = "images/rock_03.png";
    const string outputPath = "output_detections.png";

    const float CONF_THRESH = 0.80f;
    const float NMS_THRESH  = 0.20f;
    const int   INPUT_W     = 640;
    const int   INPUT_H     = 640;

    // 1. Load image
    Mat frame = imread(imagePath);
    if (frame.empty()) {
        cerr << "Error: Unable to load image: " << imagePath << endl;
        return -1;
    }
    cout << "Image loaded: " << frame.cols << "x" << frame.rows << endl;

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

        //  Extraer los 32 coeficientes de la máscara
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

    // 7. Preparar Prototipos (Output1)
    float* data1 = outputs[1].GetTensorMutableData<float>();
    // Convertir el tensor [1, 32, 160, 160] a una matriz 2D [32, 25600]
    Mat protoMap(32, 160 * 160, CV_32F, data1);

    // 8. Mask Assembly & Draw results
    for (int idx : indices) {
        const Rect& box  = boxes[idx];
        float conf = scores[idx];

        // --- INICIO CALCULO DE MASCARA ---
        // Crear matriz [1, 32] con los coeficientes de esta roca
        Mat coeff(1, 32, CV_32F, maskCoeffs[idx].data());

        // Producto matricial: [1, 32] * [32, 25600] = [1, 25600]
        Mat maskMat = coeff * protoMap; 

        // Reformatear a [160, 160]
        maskMat = maskMat.reshape(1, 160);

        // Aplicar Sigmoide
        cv::exp(-maskMat, maskMat);
        maskMat = 1.0f / (1.0f + maskMat);

        // Redimensionar la máscara de 160x160 al tamaño original de la imagen
        Mat maskOriginal;
        resize(maskMat, maskOriginal, Size(frame.cols, frame.rows));

        // Recortar la máscara solo al bounding box
        Mat cropMask = maskOriginal(box);

        // Binarizar la máscara (umbral de 0.5)
        Mat binMask;
        threshold(cropMask, binMask, 0.5, 255, THRESH_BINARY);
        binMask.convertTo(binMask, CV_8U);

        // --- CALCULAR AREA ---
        int areaPixels = countNonZero(binMask);

        // --- DIBUJAR ---
        // Pintar la máscara de azul sobre la imagen original
        Mat coloredMask = Mat::zeros(box.size(), CV_8UC3);
        coloredMask.setTo(Scalar(255, 0, 0), binMask); 
        addWeighted(frame(box), 1.0, coloredMask, 0.5, 0.0, frame(box));

        // Dibujar Box
        rectangle(frame, box, Scalar(0, 255, 0), 2);

        // Añadir texto con la confianza y el AREA
        string label = "Area: " + to_string(areaPixels) + " px";
        int baseline = 0;
        Size textSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.55, 1, &baseline);

        rectangle(frame,
                  Point(box.x, box.y - textSize.height - 6),
                  Point(box.x + textSize.width, box.y),
                  Scalar(0, 255, 0), FILLED);

        putText(frame, label,
                Point(box.x, box.y - 4),
                FONT_HERSHEY_SIMPLEX, 0.55,
                Scalar(0, 0, 0), 1);
    }

    imwrite(outputPath, frame);
    cout << "Annotated image saved: " << outputPath << endl;

    return 0;
}