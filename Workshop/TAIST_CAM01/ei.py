# example code:
# https://github.com/openmv/openmv/blob/master/tools/rpc/rpc_image_transfer_jpg_as_the_controller_device.py

from PySide6 import QtCore, QtWidgets, QtGui
from PySide6.QtGui import QPixmap
import serial.tools.list_ports
import sys
import cv2
import serial
import numpy as np
import rpc
from datetime import datetime


class EspCamWidget(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.ser_ports = [port for (port, desc, hwid) in serial.tools.list_ports.comports()]
        self.populate_ui()
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.grab_image)

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
        self.preview_img.setFixedSize(640, 480)
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
        self.esp32_button.setText("Start Monitoring")
        print("start Monitoring")
        self.esp32_button.clicked.disconnect()
        self.esp32_button.clicked.connect(self.monitor_stream)
        self.esp32_button.repaint()
        self.timer.start(1000)

    def monitor_stream(self):
        # while True:
        #     self.grab_image()
        pass
            
            


    # def grab_image(self):
    #     Tstart = datetime.now()
    #     print("Grab start...")
    #     result = self.rpc_master.call("jpeg_image_read", recv_timeout=1000)
    #     print("Received data:", result)
    #     jpg_sz = int.from_bytes(result.tobytes(), "little")
    #     buf = bytearray(b'\x00'*jpg_sz)
    #     self.rpc_master.get_bytes(buf, jpg_sz)
    #     print(str(datetime.now() - Tstart))
    #     img = cv2.imdecode(np.frombuffer(buf, dtype=np.uint8), cv2.IMREAD_COLOR)
    #     self.update_image(img.copy())
    def grab_image(self):
        # example code 
        # https://github.com/openmv/openmv/blob/master/tools/rpc/README.md
        Tstart = datetime.now()
        print("Snapshot start...")
        result = self.rpc_master.call("jpeg_image_snapshot", recv_timeout=1000)
        print(result)
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

    # def update_image(self, img):
    #     img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    #     h, w, c = img.shape
    #     img = QtGui.QImage(img.data, w, h, QtGui.QImage.Format_RGB888)
    #     pixmap = QPixmap(img)
    #     self.preview_img.setPixmap(pixmap)
    def update_image(self, img):
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        h,w,c = img.shape
        img = QtGui.QImage(img.data, w, h, QtGui.QImage.Format_RGB888)
        self.preview_img.setPixmap(QPixmap(img))
        
if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    widget = EspCamWidget()
    widget.resize(640, 480)
    widget.show()
    sys.exit(app.exec())
