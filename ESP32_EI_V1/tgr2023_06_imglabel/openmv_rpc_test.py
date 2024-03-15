import cv2
import numpy as np
import json, rpc, serial, serial.tools.list_ports, struct, sys
from datetime import datetime
import time

rpc_master = rpc.rpc_usb_vcp_master('COM7')

# OpenCV setup for video streaming
cv2.namedWindow("Video Stream", cv2.WINDOW_NORMAL)
cv2.resizeWindow("Video Stream", 240, 240)

try:
    while True:
        # Capture a JPEG frame
        result = rpc_master.call("jpeg_image_snapshot", recv_timeout=10000)
        jpg_sz = int.from_bytes(result.tobytes(), "little")

        # Read the JPEG image
        buf = bytearray(b'\x00'*jpg_sz)
        result = rpc_master.call("jpeg_image_read", recv_timeout=10000)
        rpc_master.get_bytes(buf, jpg_sz)

        # Directly decode JPEG data into a grayscale image
        gray_frame = cv2.imdecode(np.frombuffer(buf, dtype=np.uint8), cv2.IMREAD_GRAYSCALE)
        #gray_frame = cv2.imdecode(np.frombuffer(buf, dtype=np.uint8), cv2.IMREAD_COLOR)

        # Display the grayscale frame
        cv2.imshow("Video Stream", gray_frame)

        # Introduce a delay to control the frame rate (adjust as needed)
        time.sleep(0.3)  # 0.1 seconds delay

        # Exit the loop when 'q' key is pressed
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    pass
finally:
    # Release resources
    cv2.destroyAllWindows()
