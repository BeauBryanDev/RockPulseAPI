import cv2

img  = cv2.imread("your_rock_photo.jpg")
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
params     = cv2.aruco.DetectorParameters()
detector   = cv2.aruco.ArucoDetector(dictionary, params)

corners, ids, rejected = detector.detectMarkers(gray)

if ids is not None:
    print(f"Marker detected. ID: {ids[0][0]}")
    c = corners[0][0]
    side = ((cv2.norm(c[0]-c[1]) +
             cv2.norm(c[1]-c[2]) +
             cv2.norm(c[2]-c[3]) +
             cv2.norm(c[3]-c[0])) / 4.0)
    print(f"Marker side in pixels: {side:.1f} px")
    print(f"k = 5.0 / {side:.1f} = {5.0/side:.5f} cm/px")

    cv2.aruco.drawDetectedMarkers(img, corners, ids)
    cv2.imwrite("validation_result.jpg", img)
    print("Saved validation_result.jpg - check the drawn marker")
else:
    print(f"No marker detected. Rejected candidates: {len(rejected)}")