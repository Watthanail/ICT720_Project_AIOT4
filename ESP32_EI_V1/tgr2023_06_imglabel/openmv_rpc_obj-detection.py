import cv2
import numpy as np
import json, rpc, serial, serial.tools.list_ports, struct, sys
from datetime import datetime
import time

# Import Edge Impulse model variables
import object_detection_inferencing

# Initialize Edge Impulse classifier
classifier = ei_default_impulse.make_classifier()

rpc_master = rpc.rpc_usb_vcp_master('COM6')

# OpenCV setup for video streaming
cv2.namedWindow("Object Detection Stream", cv2.WINDOW_NORMAL)
cv2.resizeWindow("Object Detection Stream", 640, 480)

try:
    while True:
        # Capture a JPEG frame
        result = rpc_master.call("jpeg_image_snapshot", recv_timeout=10000)
        jpg_sz = int.from_bytes(result.tobytes(), "little")

        # Read the JPEG image
        buf = bytearray(b'\x00'*jpg_sz)
        result = rpc_master.call("jpeg_image_read", recv_timeout=10000)
        rpc_master.get_bytes(buf, jpg_sz)

        # Directly decode JPEG data into an RGB image
        rgb_frame = cv2.imdecode(np.frombuffer(buf, dtype=np.uint8), cv2.IMREAD_COLOR)

        # Run the classifier
        signal = {
            "total_length": EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT,
            "get_data": lambda: rgb_frame.flatten().tobytes()
        }

        result = classifier.classify(signal)

        # Display the original frame with bounding boxes for detected objects
        for label, score, (startX, startY, endX, endY) in result:
            cv2.rectangle(rgb_frame, (startX, startY), (endX, endY), (0, 255, 0), 2)
            cv2.putText(rgb_frame, f"{label}: {score:.2f}", (startX, startY - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

        # Display the frame with object detection
        cv2.imshow("Object Detection Stream", rgb_frame)

        # Introduce a delay to control the frame rate (adjust as needed)
        time.sleep(0.1)  # 0.1 seconds delay

        # Exit the loop when 'q' key is pressed
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    pass
finally:
    # Release resources
    cv2.destroyAllWindows()
