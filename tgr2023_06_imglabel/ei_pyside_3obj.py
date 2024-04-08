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
        self.all_bbox = []
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

        self.label_txt1 = QtWidgets.QLineEdit()
        self.ctrl_layout.addRow("Label_1", self.label_txt1)

        self.label_txt2 = QtWidgets.QLineEdit()
        self.ctrl_layout.addRow("Label_2", self.label_txt2)

        self.label_txt3 = QtWidgets.QLineEdit()
        self.ctrl_layout.addRow("Label_3", self.label_txt3)

        self.data_type_label = QtWidgets.QLabel("Data Type:")
        self.data_type_combo = QtWidgets.QComboBox()
        self.data_type_combo.addItems(["Training", "Testing"])  # Add options for training and testing
        self.ctrl_layout.addRow(self.data_type_label, self.data_type_combo)

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
            print("Image size: ", jpg_sz);
            self.buf = bytearray(b'\x00'*jpg_sz)
            Tstart = datetime.now()
            print("Grab start...")
            result = self.rpc_master.call("jpeg_image_read", recv_timeout=1000)
            self.rpc_master.get_bytes(self.buf, jpg_sz)
            print(str(datetime.now() - Tstart))
            #print(buf)
            #img = cv2.imread("test.jpg")
            self.img = cv2.imdecode(np.frombuffer(self.buf, dtype=np.uint8), cv2.IMREAD_COLOR)
            self.update_image(self.img.copy())
        self.all_bbox = []
        print("reset all bbox")
    
    def update_image(self, img):
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        h,w,c = img.shape
        img = QtGui.QImage(img.data, w, h, QtGui.QImage.Format_RGB888)
        self.preview_img.setPixmap(QPixmap(img))
        
    # def label_image(self):
    #     print(self.preview_img.status)
    #     if self.preview_img.status == 'RELEASED':
    #         print (self.preview_img.pos_1st, self.preview_img.pos_2nd)
    #         if self.preview_img.pos_1st.x() < self.preview_img.pos_2nd.x():
    #             x1 = int(self.preview_img.pos_1st.x())
    #             x2 = int(self.preview_img.pos_2nd.x())
    #             y1 = int(self.preview_img.pos_1st.y())
    #             y2 = int(self.preview_img.pos_2nd.y())
    #         else:
    #             x1 = int(self.preview_img.pos_2nd.x())
    #             y1 = int(self.preview_img.pos_2nd.y())
    #             x2 = int(self.preview_img.pos_1st.x())
    #             y2 = int(self.preview_img.pos_1st.y())
    #         print(x1,y1,x2,y2)
    #         self.bbox = (x1,y1,(x2-x1),(y2-y1))
    #         img = cv2.rectangle(self.img.copy(), self.bbox, (0,255,0), 1)
    #         self.update_image(img)
        
    def label_image(self):
        if self.preview_img.status == 'RELEASED':
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
            self.bbox = (x1, y1, (x2 - x1), (y2 - y1))   #  x,y,width,height
            self.all_bbox.append(self.bbox)

            num = 1
            for pos in self.all_bbox:
                print(f"pos:{pos}")
                img = cv2.rectangle(self.img.copy(), pos, (0, 0, 255), 1)
                self.img = img.copy()
                cv2.putText(self.img, f"{num}", (pos[0], pos[1] - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)  #(B,G,R)
                self.update_image(self.img)
                num += 1

            print(f"all_bbox:{self.all_bbox}")


    # def upload_data(self):
    #     bbox = {
    #         "version": 1,
    #         "type": "bounding-box-labels",
    #         "boundingBoxes": {
    #             "tmp.jpg": [
    #                 {
    #                 "label": self.label_txt.text(),
    #                 "x": self.bbox[0],
    #                 "y": self.bbox[1],
    #                 "width": self.bbox[2],
    #                 "height": self.bbox[3]
    #                 }
    #             ]
    #         }
    #     }
    #     bbox_label = json.dumps(bbox, separators=(',', ':'))
    #     headers = {'x-api-key': self.ei_api.text(),
    #                'x-label': self.label_txt.text(),
    #                'x-add-date-id': '1',
    #                }
    #     print(headers)
    #     payload = (('data',('tmp.jpg', self.buf, 'image/jpeg')), ('data', ('bounding_boxes.labels', bbox_label)))
    #     #payload = (('data',('tmp.jpg', self.buf, 'image/jpeg')),)
    #     #payload = (('data',self.buf),)
    #     res = requests.post('https://ingestion.edgeimpulse.com/api/training/files',
    #                         headers=headers,
    #                         files=payload)
    #     print('Uploaded file(s) to Edge Impulse\n', res.status_code, res.content)


    def upload_data(self):
        data_type = self.data_type_combo.currentText()
        self.label_txts = []  # List to store line edit widgets

        for i in range(3):
            #label_txt3
            txt = getattr(self, f"label_txt{i+1}").text()  # Access the current text of each line edit widget
            self.label_txts.append(txt)  # Add the current text to the list
            print(f"self.label_txts: {self.label_txts}")


        bounding_boxes = []
        num_boxes = len(self.all_bbox)
        
        for i in range(num_boxes):
                #"label": self.label_txt.text(),
            bbox = {
                "label": str(self.label_txts[i]),
                "x": self.all_bbox[i][0],
                "y": self.all_bbox[i][1],
                "width": self.all_bbox[i][2],
                "height": self.all_bbox[i][3]
            }
            print(f"bbox:{bbox}")
            bounding_boxes.append(bbox)
        
        bbox_data = {
            "version": 1,
            "type": "bounding-box-labels",
            "boundingBoxes": {
                "tmp.jpg": bounding_boxes
            }
        }
        
        bbox_label = json.dumps(bbox_data, separators=(',', ':'))
        
        headers = {'x-api-key': self.ei_api.text(),
                   'x-label': "smart_refrigerator",
                   'x-add-date-id': '1',
                   }
        # headers = {'x-api-key': "ei_d89401d59b380fb1df57d3427f7eff800732126fced681dd8e22a87eea3d989e",
        #            'x-label': "smart_refrigerator",
        #            'x-add-date-id': '1',
        #            }
        print(headers)
        payload = (('data',('tmp.jpg', self.buf, 'image/jpeg')), ('data', ('bounding_boxes.labels', bbox_label)))
        
        if data_type == "Training":
            endpoint = 'https://ingestion.edgeimpulse.com/api/training/files'
        else:
            endpoint = 'https://ingestion.edgeimpulse.com/api/testing/files'

        res = requests.post(endpoint, headers=headers, files=payload)

        print('Uploaded file(s) to Edge Impulse\n', res.status_code, res.content)

if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    widget = EspCamWidget()
    widget.resize(640, 480)
    widget.show()
    sys.exit(app.exec())