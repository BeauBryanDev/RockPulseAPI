import cv2
import numpy as np

# Generate marker
dictionary   = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
marker_size_px = 400   # large enough for clean printing

marker_image = cv2.aruco.generateImageMarker(dictionary, 0, marker_size_px)

# Add white quiet zone border - minimum 1 cell = marker_size/6
# For a 4x4 marker the cell size is marker_size / 6
# (4 data cells + 2 border cells = 6 total)
cell_px    = marker_size_px // 6
border_px  = cell_px * 2   # 2 cells of quiet zone - safe margin

canvas_size = marker_size_px + (border_px * 2)
canvas      = np.ones((canvas_size, canvas_size), dtype=np.uint8) * 255

# Place marker centered on white canvas
canvas[border_px:border_px + marker_size_px,
       border_px:border_px + marker_size_px] = marker_image

cv2.imwrite("aruco_ID0_print_5cm.png", canvas)
print(f"Canvas size : {canvas_size} x {canvas_size} px")
print(f"Marker size : {marker_size_px} x {marker_size_px} px")
print(f"Border      : {border_px} px on each side")
print("Saved: aruco_ID0_print_5cm.png")
