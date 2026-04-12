# What happened, what was wrong, and how it was fixed
                                                                                
  1. The first error — stale binary
                                                                                
  imread_('images/test_rocks.png'): can't open/read file                        
                                                                                
  Your main.cpp had already been updated to use images/rock_03.png, but the     
  executable in build/ was compiled from an older version of the code that still
   referenced images/test_rocks.png. The binary was out of sync with the source.
                                          
  Fix: make — recompiled the binary.                                            
   
  ---                                                                           
  2. The second error — OpenCV 4.6.0 can't run YOLOv8-seg

  OpenCV(4.6.0) shape_utils.hpp:170: Assertion failed: start <= shape.size() ...
                                                                                
  This is the real problem. Here's the chain:                                   
                                                                                
  - Ubuntu 24.04 only ships OpenCV 4.6.0 via apt.                               
  - You have OpenCV 4.13.0 in Anaconda, but that is Python-only — no C++ headers
   or .so files for linking.                                                    
  - So your C++ build was forced to use system OpenCV 4.6.0.
  - YOLOv8-seg ONNX models use ONNX opset operators and internal graph          
  structures that OpenCV's DNN module only gained proper support for in 4.8+. In
   4.6.0, the DNN module crashes during net.forward() while trying to compute   
  tensor shapes internally — it hits an assertion because it doesn't understand 
  the graph layout of YOLOv8-seg.         
  - The model itself was perfectly valid — confirmed by running it with Python's
   onnxruntime 1.24.4 which had no issues.                                      
   
  Fix: Stopped using OpenCV DNN for inference entirely. Downloaded the ONNX     
  Runtime C++ SDK v1.21.0 (a ~7MB tarball from GitHub releases) into
  RockPulsePoC/ort/. This gives proper C++ headers and a .so that handles modern
   ONNX models correctly. OpenCV is kept but only for what it's good at: loading
   and resizing the image.

  CMakeLists.txt was updated to include the ORT headers and link against        
  ort/lib/libonnxruntime.so.
                                                                                
  main.cpp was rewritten to use the Ort::Session API instead of                 
  cv::dnn::readNetFromONNX.
                                                                                
  ---                                     
  Summary table

  ┌──────────────┬───────────────────┬──────────────────────────────────────┐
  │     Step     │      Problem      │                 Fix                  │
  ├──────────────┼───────────────────┼──────────────────────────────────────┤   
  │ imread       │ Binary compiled   │                                      │
  │ failure      │ against old image │ make to recompile                    │   
  │              │  path             │                                      │   
  ├──────────────┼───────────────────┼──────────────────────────────────────┤
  │ shape_utils  │ OpenCV 4.6.0 DNN  │ Replaced OpenCV DNN with ONNX        │   
  │ crash        │ can't process     │ Runtime C++ SDK                      │
  │              │ YOLOv8-seg ONNX   │                                      │   
  ├──────────────┼───────────────────┼──────────────────────────────────────┤   
  │ No ORT C++   │ Anaconda only     │ Downloaded                           │
  │ headers      │ ships Python      │ onnxruntime-linux-x64-1.21.0.tgz     │   
  │              │ bindings          │ from GitHub                          │
  └──────────────┴───────────────────┴──────────────────────────────────────┘

 Background command "Check if libonnxruntime available via conda-forge"        
completed (exit code 0)
                                                                                
● That's just the leftover background conda --dry-run command I started earlier
  finally finishing — it's irrelevant now, the problem is already solved without
   touching conda.                                                              
  
  You can safely ignore it.  


● PoC Status: PASSED ✓

  Yes. The model ran successfully end-to-end:
  - Image loaded correctly
  - Both ONNX outputs produced expected shapes ([1,37,8400] and [1,32,160,160])
  - 494 raw detections above 0.25 confidence (these need NMS to become final
  results — that's the next step)

  ---
  Tech Stack for the Full REST API

  [ Client ] → HTTP POST (image) → [ Crow REST API ] → [ ORT Inference ] → JSON
  response

  Core Components

  ┌────────────────────────┬───────────────┬───────────────────────────────┐
  │         Layer          │    Library    │              Why              │
  ├────────────────────────┼───────────────┼───────────────────────────────┤
  │                        │               │ Header-only, fast, simple     │
  │ HTTP server            │ Crow          │ routing, handles multipart    │
  │                        │               │ uploads                       │
  ├────────────────────────┼───────────────┼───────────────────────────────┤
  │ Inference              │ ONNX Runtime  │ Proven working in your PoC —  │
  │                        │ C++           │ do NOT use OpenCV DNN         │
  ├────────────────────────┼───────────────┼───────────────────────────────┤
  │ Image I/O &            │ OpenCV        │ Decode JPEG/PNG, resize, draw │
  │ pre/post-processing    │               │  boxes                        │
  ├────────────────────────┼───────────────┼───────────────────────────────┤
  │                        │               │ Crow has basic JSON but       │
  │ JSON                   │ nlohmann/json │ nlohmann is more complete for │
  │                        │               │  structured responses         │
  ├────────────────────────┼───────────────┼───────────────────────────────┤
  │ Build                  │ CMake         │ Already using it              │
  └────────────────────────┴───────────────┴───────────────────────────────┘

  Critical Architecture Points

  1. Load the model once at startup, not per request
  // In main(), before crow app starts:
  Ort::Session session(env, "model/yolo_v8_nano_seg.onnx", sessionOpts);
  Pass it (thread-safe) into your route handlers.

  2. NMS is mandatory — you saw 494 raw detections. You need cv::dnn::NMSBoxes()
   (this function works fine in OpenCV 4.6, the DNN inference is what's broken,
  not the utilities).

  3. Request flow per endpoint:
  POST /detect  (multipart image)
    → cv::imdecode()          // decode bytes to Mat
    → resize + normalize      // preprocess to 640x640 float NCHW
    → Ort::Session::Run()     // inference
    → parse output0           // extract boxes + confidence + mask coeffs
    → NMSBoxes()              // filter to real detections
    → return JSON             // { detections: [{bbox, confidence, class}] }

  4. Thread safety: Ort::Session::Run() is thread-safe — Crow's multi-threaded
  request handling will work out of the box.

  Recommended JSON response shape

  {
    "detections": [
      {
        "bbox": { "x": 120, "y": 45, "w": 200, "h": 180 },
        "confidence": 0.87,
        "class": "rock"
      }
    ],
    "inference_ms": 42,
    "image_size": { "w": 800, "h": 600 }
  }

  What to avoid

  - OpenCV DNN for inference — proven broken on your system for this model
  - Loading the session per request — expensive, will kill performance
  - Returning raw 494 detections — always apply NMS first

  Next steps in order

  1. Add NMS + box parsing to the current main.cpp (finish the PoC)
  2. Wrap it in a Crow route
  3. Add model session as a shared resource
  4. Test with real mining images



Bash(make 2>&1)                 
  ⎿  [ 50%] Building CXX object CMakeFiles/RockPulsePoC.dir/main.cpp.o        
     [100%] Linking CXX executable RockPulsePoC
     [100%] Built target RockPulsePoC                                           
                                                                                
● Bash(LD_LIBRARY_PATH=../ort/lib:$LD_LIBRARY_PATH ./RockPulsePoC 2>&1)
  ⎿  Image loaded: 800x600                                                      
     Raw candidates above 0.25: 494                                             
     Detections after NMS: 55                                                   
     Annotated image saved: output_detections.png                               
                                                                              
● 494 raw → 55 after NMS. That's a realistic number for a dense rock/mining     
  image.                                                                        
                                                                                
  Here's what the code does now:                                                
                                                                                
  Load image (800x600)                                                          
    ↓                                                                           
  Preprocess → 640×640 NCHW float tensor                                        
    ↓                                                                           
  ORT inference → output0 [1, 37, 8400]                                         
    ↓                                                                           
  Parse 8400 anchors → keep conf > 0.25  →  494 candidates
    ↓                                                                           
  NMSBoxes (IoU thresh 0.45)             →   55 final detections
    ↓                                                                           
  Draw boxes + "rock XX%" labels
    ↓                                                                           
  Save → output_detections.png                                                  
   
  Open the annotated result to visually verify:                                 
  eog output_detections.png
  # or                     
  xdg-open output_detections.png                                                
                                
  If 55 still looks like too many or too few, you can tune:                     
  - CONF_THRESH = 0.25 — raise it (e.g. 0.4) to filter out weak detections      
  - NMS_THRESH  = 0.45 — lower it (e.g. 0.35) to suppress more overlapping boxes
                                                                                




--------------------------------------------------------------------------------

~/RockPulsePoC/build$ ./RockPulsePoC 
Image loaded: 800x600
Input : images
Output0: output0
Output1: output1

--- Inferencia Completada con Exito ---
Salida 0 shape: [1, 37, 8400]
Salida 1 shape: [1, 32, 160, 160]
Detecciones con confianza > 0.25: 494

--------------------------------------------------------------------------------

Resume this session with:───────────────────────────────────────────────────────
claude --resume 4d93c293-b5f4-40c5-a03f-6026db8194ff   

