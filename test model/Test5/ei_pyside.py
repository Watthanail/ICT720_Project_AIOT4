# example code:
# https://github.com/openmv/openmv/blob/master/tools/rpc/rpc_image_transfer_jpg_as_the_controller_device.py

from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtGui import QPixmap
import serial.tools.list_ports
import sys
import cv2
import json
import serial
import numpy as np
import rpc
import requests
from datetime import datetime

import base64

class ImgLabel(QtWidgets.QLabel):
    clicked = QtCore.Signal()

    def mousePressEvent(self, ev: QtGui.QMouseEvent):
        self.status = 'CLICKED'
        self.pos_1st = ev.position()
        self.clicked.emit()
        return super().mousePressEvent(ev)
    
    def mouseReleaseEvent(self, ev: QtGui.QMouseEvent) -> None:
        self.status = 'RELEASED'
        self.pos_2nd = ev.position()
        self.clicked.emit()
        return super().mouseReleaseEvent(ev)

class EspCamWidget(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.ser_ports = [port for (port,desc,hwid) in serial.tools.list_ports.comports()]
        self.populate_ui()

    def populate_ui(self):
        self.main_layout = QtWidgets.QHBoxLayout(self)
        self.populate_ui_image()
        self.populate_ui_ctrl()
        self.main_layout.addLayout(self.image_layout)
        self.main_layout.addLayout(self.ctrl_layout)

    def populate_ui_image(self):
        self.image_layout = QtWidgets.QVBoxLayout()
        self.image_layout.setAlignment(QtCore.Qt.AlignTop)
        #self.preview_img = QtWidgets.QLabel("Preview Image")
        self.preview_img = ImgLabel("Preview Image")
        self.preview_img.resize(320, 240)
        self.preview_img.clicked.connect(self.label_image)
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

        self.ei_api = QtWidgets.QLineEdit()
        self.ctrl_layout.addRow("EI API key", self.ei_api)

        self.label_txt = QtWidgets.QLineEdit()
        self.ctrl_layout.addRow("Label", self.label_txt)

        self.label_button = QtWidgets.QPushButton("Upload")
        self.label_button.clicked.connect(self.upload_data)
        self.ctrl_layout.addRow(self.label_button)
    
    def connect_esp32(self):
        port = self.esp32_port.currentText()
        self.rpc_master = rpc.rpc_usb_vcp_master(port)
        self.esp32_button.setText("Grab")
        self.esp32_button.clicked.disconnect()
        self.esp32_button.clicked.connect(self.grab_image)
        self.esp32_button.repaint()

    def grab_image(self):
        # example code 
        # https://github.com/openmv/openmv/blob/master/tools/rpc/README.md
        Tstart = datetime.now()
        print("Snapshot start...")
        result = self.rpc_master.call("jpeg_image_snapshot", recv_timeout=1000)
        print(str(datetime.now() - Tstart)  + ': ' + str(result))

        if result is not None:
            jpg_sz = int.from_bytes(result.tobytes(), "little")
            print("Image size: ", jpg_sz)
            self.buf = bytearray(b'\x00'*jpg_sz)
            Tstart = datetime.now()
            print("Grab start...")
            result = self.rpc_master.call("jpeg_image_read", recv_timeout=1000)
            self.rpc_master.get_bytes(self.buf, jpg_sz)
            print(str(datetime.now() - Tstart))
            self.img = cv2.imdecode(np.frombuffer(self.buf, dtype=np.uint8), cv2.IMREAD_COLOR)
            self.update_image(self.img.copy())
        else:
            print("image is None")

        ##### test output
        results = self.rpc_master.call("sent_information", recv_timeout=1000)

        ### array of string
        if results is not None:
            # Convert ASCII numbers to characters and join them with '0' separator
            ascii_string = ''.join(chr(data) for data in results)

            # Split the resulting string by '0' and filter out empty strings and convert to list
            strings_list = list(filter(None, ascii_string.split('\x00')))

            # Print each string
            # for string in strings_list:
            #     print(string)
            
            print(strings_list)
            bounding_boxes = []
            for i in range(3, len(strings_list), 4):  # Iterate over every 4th element starting from index 3
                x = int(strings_list[i])
                y = int(strings_list[i+1])
                width = int(strings_list[i+2])
                height = int(strings_list[i+3])
                bounding_boxes.append((x, y, width, height))
            print(bounding_boxes)
            # # Drawing bounding boxes on the image
            for box in bounding_boxes:
                x, y, width, height = box
                cv2.rectangle(self.img, (x, y), (x + width, y + height), (0, 255, 0), 1)


            resized_img = cv2.resize(self.img, (1000, 1000))  # Specify the new width and height
            # Showing the resized image
            cv2.imshow("Resized Image with Bounding Boxes", resized_img)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
        else:
            print("No data received from RPC call")



    def update_image(self, img):
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        h,w,c = img.shape
        img = QtGui.QImage(img.data, w, h, QtGui.QImage.Format_RGB888)
        self.preview_img.setPixmap(QPixmap(img))
        
    def label_image(self):
        print(self.preview_img.status)
        if self.preview_img.status == 'RELEASED':
            print (self.preview_img.pos_1st, self.preview_img.pos_2nd)
            if self.preview_img.pos_1st.x() < self.preview_img.pos_2nd.x():
                x1 = int(self.preview_img.pos_1st.x())
                x2 = int(self.preview_img.pos_2nd.x())
                y1 = int(self.preview_img.pos_1st.y())
                y2 = int(self.preview_img.pos_2nd.y())
            else:
                x1 = int(self.preview_img.pos_2nd.x())
                y1 = int(self.preview_img.pos_2nd.y())
                x2 = int(self.preview_img.pos_1st.x())
                y2 = int(self.preview_img.pos_1st.y())
            print(x1,y1,x2,y2)
            self.bbox = (x1,y1,(x2-x1),(y2-y1))
            img = cv2.rectangle(self.img.copy(), self.bbox, (0,255,0), 1)
            self.update_image(img)

    def upload_data(self):
        bbox = {
            "version": 1,
            "type": "bounding-box-labels",
            "boundingBoxes": {
                "tmp.jpg": [
                    {
                    "label": self.label_txt.text(),
                    "x": self.bbox[0],
                    "y": self.bbox[1],
                    "width": self.bbox[2],
                    "height": self.bbox[3]
                    }
                ]
            }
        }
        bbox_label = json.dumps(bbox, separators=(',', ':'))
        headers = {'x-api-key': self.ei_api.text(),
                   'x-label': self.label_txt.text(),
                   'x-add-date-id': '1',
                   }
        print(headers)
        payload = (('data',('tmp.jpg', self.buf, 'image/jpeg')), ('data', ('bounding_boxes.labels', bbox_label)))
        #payload = (('data',('tmp.jpg', self.buf, 'image/jpeg')),)
        #payload = (('data',self.buf),)
        res = requests.post('https://ingestion.edgeimpulse.com/api/training/files',
                            headers=headers,
                            files=payload)
        print('Uploaded file(s) to Edge Impulse\n', res.status_code, res.content)

if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    widget = EspCamWidget()
    widget.resize(640, 480)
    widget.show()
    sys.exit(app.exec())