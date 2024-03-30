from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtGui import QPixmap
import serial.tools.list_ports
import sys
import cv2
import serial
import numpy as np
import rpc
from datetime import datetime
import ctypes


# Dictionary to store colors for each topic
topic_colors = {
    "head": (233, 48, 60),    # Red
    "middle": (48, 97, 233),  # Blue
    "tail": (85, 233, 48)     # Green
}

# # List to store parsed data
# bbox_list = []

class EspCamWidget(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.ser_ports = [port for (port, desc, hwid) in serial.tools.list_ports.comports()]
        self.populate_ui()
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.grab_image)
        self.rpc_master = None

    def populate_ui(self):
        self.main_layout = QtWidgets.QHBoxLayout(self)
        self.populate_ui_image()
        self.populate_ui_ctrl()
        self.main_layout.addLayout(self.image_layout)
        self.main_layout.addLayout(self.ctrl_layout)

    def populate_ui_image(self):
        self.image_layout = QtWidgets.QVBoxLayout()
        self.image_layout.setAlignment(QtCore.Qt.AlignTop)
        self.preview_img = QtWidgets.QLabel("Preview Image")
        self.preview_img.setAlignment(QtCore.Qt.AlignCenter)
        self.preview_img.setFixedSize(240, 240)
        self.image_layout.addWidget(self.preview_img)

    def populate_ui_ctrl(self):
        self.ctrl_layout = QtWidgets.QFormLayout()
        self.ctrl_layout.setAlignment(QtCore.Qt.AlignTop)

        self.esp32_port = QtWidgets.QComboBox()
        self.esp32_port.addItems(self.ser_ports)
        self.ctrl_layout.addRow("ESP32 Port", self.esp32_port)

        self.esp32_button = QtWidgets.QPushButton("Connect")
        self.esp32_button.clicked.connect(self.connect_esp32)
        self.ctrl_layout.addRow(self.esp32_button)

    def connect_esp32(self):
        port = self.esp32_port.currentText()
        self.rpc_master = rpc.rpc_usb_vcp_master(port)
        self.esp32_button.setText("Grab")
        self.esp32_button.clicked.disconnect()
        self.esp32_button.clicked.connect(self.grab_image)
        self.esp32_button.repaint()
        
    # def connect_esp32(self):
    #     port = self.esp32_port.currentText()
    #     self.rpc_master = rpc.rpc_usb_vcp_master(port)
    #     self.esp32_button.setText("Start Monitoring")
    #     print("start Monitoring")
    #     self.esp32_button.clicked.disconnect()
    #     self.esp32_button.clicked.connect(self.monitor_stream)
    #     self.esp32_button.repaint()
    #     self.timer.start(5000)


    def monitor_stream(self):
        pass
            
    def grab_image(self):
        self.preview_img.clear()
        bbox_list = []
        Tstart = datetime.now()
        print("Snapshot start...")
        result = self.rpc_master.call("jpeg_image_snapshot", recv_timeout=1000)
        dd=""
        dd = self.rpc_master.call("sent_information", recv_timeout=1000)
        print(result)
        print(str(datetime.now() - Tstart) + ': ' + str(result))
        
        if result is not None:
            jpg_sz = int.from_bytes(result.tobytes(), "little")
            print("Image size: ", jpg_sz);
            buf = bytearray(b'\x00' * jpg_sz)
            Tstart = datetime.now()
            print("Grab start...")
            result = self.rpc_master.call("jpeg_image_read", recv_timeout=1000)
            self.rpc_master.get_bytes(buf, jpg_sz)
            print(str(datetime.now() - Tstart))
            img = cv2.imdecode(np.frombuffer(buf, dtype=np.uint8), cv2.IMREAD_COLOR)
            self.update_image(img.copy())
            
            
            # After grabbing the image, call the sent_information callback
            # result = self.rpc_master.call("sent_information", recv_timeout=1000)
            print("Result from sent_information callback:")
            if dd is not None:
                # Convert the ctypes object to a bytearray
                data = bytearray(dd)
                # bounding_boxes = data.split('\x00')

                # Print each bounding box
                # for bbox in bounding_boxes:
                #     print(bbox)
                # Print the data
                data_str = data.decode('latin-1')
                bounding_boxes = data_str.split(" ]")
                
                for bbox_str in bounding_boxes:
                    bbox_str = bbox_str.strip()  # Remove leading/trailing whitespace
                    # print(bbox_str)
                    if not bbox_str:
                        continue  # Skip empty strings
                    parts = bbox_str.split(" [ ")
                    box_parts = [part.replace('\x00', '') for part in parts]
                    topic = box_parts[0].split(" (")[0].strip()

                    if topic in ["head", "middle", "tail"]:   
                        confidence = float(box_parts[0].split("(")[1].split(")")[0])
                        attributes = box_parts[1].replace(",", "").split()
                        
                        x = int(attributes[1])
                        y = int(attributes[3])
                        width = int(attributes[5])
                        height = int(attributes[7])
                        # Create a dictionary for the bounding box and append it to the list
                        

                        if topic == "head" :
                              x += 30
                              y += 55
                        if  topic == "middle":
                              x += 50
                              y += 55
                              pass
                        elif  topic == "tail":
                              x += 80
                              y += 65

                              pass
                        bbox = {"topic": topic, "confidence": confidence, "x": x, "y": y, "width": width, "height": height}
                        
                        
                        bbox_list.append(bbox)
                        print(bbox)

                        for bbox in bbox_list:

                            topic = bbox["topic"]
                            x, y, width, height = bbox["x"], bbox["y"], bbox["width"], bbox["height"]
                            color = topic_colors[topic]
                            cv2.rectangle(img, (x, y), (x + width, y + height), color, 2) 

            self.update_image(img.copy())
            

    def update_image(self, img):
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        h, w, c = img.shape
        img = QtGui.QImage(img.data, w, h, QtGui.QImage.Format_RGB888)
        pixmap = QPixmap(img)
        self.preview_img.setPixmap(pixmap)




if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    widget = EspCamWidget()
    widget.resize(640, 480)
    widget.show()
    sys.exit(app.exec())
