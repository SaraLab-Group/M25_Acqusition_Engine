import sys
import binascii
import threading
import time
import struct
from lib2to3.pytree import convert
from struct import unpack
import socket
from subprocess import call

from ctypes import *
from typing import Any, Tuple

from M25_ui import Ui_MainWindow
from PyQt5.QtWidgets import (
    QApplication, QDialog, QMainWindow, QFileDialog
)


HOST = '127.0.0.1'  # The server's hostname or IP address
PORT = 27015        # The port used by the server
run = True


class ConfData(Structure):
    __fields__ = [('horz', c_uint32),
                  ('vert', c_uint32),
                  ('fps', c_uint32),
                  ('exp', c_uint32),
                  ('bpp', c_uint32),
                  ('capTime', c_uint32),
                  ('flags', c_uint16),
                  ('path', c_char_p)]

#inData: ConfData = ConfData()
#inData.horz = 1920
#inData.vert = 1200
#inData.fps = 65
#inData.exp = 6700
#inData.bpp = 8
#inData.capTime = 10
#inData.path = "\0"*255
#inData.path = "D:\\Ant1 Test\\tiff"
#inData.flags = 0
horz: int = 1920
vert: int = 1200
fps: int = 65
exp: int = 6700
bpp: int = 8
capTime: int = 10
path = "\0"*255
path = "D:\\Ant1 Test\\tiff"
flags: int = 0
exe_path: str = r'C:\Users\Callisto\Documents\abajor\M25_basler\winsock_adventure_server\x64\Debug'
myEXE = "winsock_adventure_server.exe"

write_mutex = threading.Lock()
def client_thread():
    while run:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, PORT))
            global write_mutex
            global horz
            global vert
            global fps
            global exp
            global bpp
            global capTime
            global path
            global flags
            write_mutex.acquire()
            values = (horz, vert, fps, exp, bpp, capTime, path.encode(), flags)
            packer = struct.Struct('L L L L L L 255s H')
            packed_data = packer.pack(*values)
            s.sendall(packed_data)

            write_mutex.release()
            #print(inData.path);
            #outData: bytes = inData
            #values: Tuple[Any, Any, Any, Any, Any, Any, str, Any] = (inData.horz, inData.vert, inData.fps, inData.exp, inData.bpp, inData.capTime, inData.path, inData.flags)
            #packer = struct.Struct('L L L L L L 255s H')
            #packed_data = Payload(inData)
            #s.sendall(outData)
            data: bytes = s.recv(512)
            (rec_horz, rec_vert, rec_fps, rec_exp, rec_bpp, rec_capTime,
             rec_path,
             rec_flags) = unpack(
                'L L L L L L'
                '255s'
                'H',
                data
            )
        #pathStr = convert(rec_path)
        print('Received horz: %d' % int(rec_horz))
        print('Received vert: %d' % int(rec_vert))
        print('Received fps: %d' % int(rec_fps))
        print('Received exp: %d' % int(rec_exp))
        print('Received bpp: %d' % int(rec_bpp))
        print('Received capTime: %d' % int(rec_capTime))
        print('Received path: %s' % rec_path)
        print('Received flags: %d' % int(rec_flags))
        #print('Received ' + repr(data))
        time.sleep(0.5)


th = threading.Thread(target=client_thread)





class Window(QMainWindow, Ui_MainWindow):

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setupUi(self)
        global path
        self.WritePLineEdit.setText(path)
        print(exe_path)
        rc = call("start cmd /K " + myEXE, cwd=exe_path, shell=True)  # run `cmdline` in `dir`
        global bpp
        bpp = 8
        global run
        th.start()


    def sync_HorzLineEdit(self, text):
        global horz
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            horz = (int(text))
        else:
            horz = (0)
        write_mutex.release()


    def sync_VertLineEdit(self, text):
        global vert
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            vert = (int(text))
        else:
            vert = (0)
        write_mutex.release()


    def sync_FPSLineEdit(self, text):
        global fps
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            fps = (int(text))
        else:
            fps = (0)
        write_mutex.release()


    def sync_EXPLineEdit(self, text):
        global exp
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            exp = (int(text))
        else:
            exp = (0)
        write_mutex.release()


    def sync_CapTimeLineEdit(self, text):
        global capTime
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            capTime = (int(text))
        else:
            capTime = (0)
        write_mutex.release()


    def onClicked(self):
        global bpp
        global write_mutex
        write_mutex.acquire()
        radioButton = self.sender()
        if radioButton.isChecked():
            bpp = (radioButton.value)
            print("Button Value bpp: %d" % (radioButton.value))
        write_mutex.release()

    def browseState(self):
        global path
        path = str(QFileDialog.getExistingDirectory(self, "Select Directory"))
        self.WritePLineEdit.setText(path)

    def closeEvent(self, event):
        print('close event fired')
        global run
        run = False
        th.join


class FindReplaceDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = Window()
    win.show()
    sys.exit(app.exec())

