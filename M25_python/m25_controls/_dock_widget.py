"""
This module is an example of a barebones QWidget plugin for napari

It implements the ``napari_experimental_provide_dock_widget`` hook specification.
see: https://napari.org/docs/dev/plugins/hook_specifications.html

Replace code below according to your needs.
"""
import sys
import binascii
from napari_plugin_engine import napari_hook_implementation
from m25_controls.widget import m25_widget

import threading
import time
import struct
from struct import unpack
import socket
from subprocess import call
import logging

from ctypes import *
from PyQt5.QtWidgets import QWidget, QFileDialog,QDialog
from napari import Viewer
from PyQt5.QtCore import pyqtSlot, pyqtSignal



HOST = '127.0.0.1'  # The server's hostname or IP address
PORT = 27015        # The port used by the server
run = True

CHANGE_CONFIG = 0x1
DROPPED_FRAME = 0x2
SET_RTC = 0x4
ACK_CMD = 0x8
START_COUNT = 0x10
COUNTING = 0x20
STOP_COUNT = 0x40
ACQUIRE_CAMERAS = 0x80
CAMERAS_ACQUIRED = 0x100
RELEASE_CAMERAS = 0x200
AQUIRE_FAIL = 0x400
START_CAPTURE = 0x800
CAPTURING = 0x1000
USB_HERE = 0x2000 #Use this as flag for Server alive too, since we wont trigger without USB
CONVERTING = 0x4000
FINISHED_CONVERT = 0x8000
ACQUIRING_CAMERAS = 0x10000
CONFIG_CHANGED = 0x20000
EXIT_THREAD = 0x80000000
DEFAULT_FPS = 65

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
exe_path: str = r'C:\Users\Callisto\Documents\abajor\M25_basler\basler_candidate\ide\x64\Debug'
myEXE = "Basler_Candidate.exe"

write_mutex = threading.Lock()
def client_thread():
    #time.sleep(2)
    prevFlag = 0
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
            packer = struct.Struct('L L L L L L 255s L')
            packed_data = packer.pack(*values)
            s.sendall(packed_data)
            #print('flags: %d' % int(flags))
            flags = 0
            #print('flags after: %d' % int(flags))
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
                'L',
                data
            )
        #pathStr = convert(rec_path)
        #print('Received horz: %d' % int(rec_horz))
        #print('Received vert: %d' % int(rec_vert))
        #print('Received fps: %d' % int(rec_fps))
        #print('Received exp: %d' % int(rec_exp))
        #print('Received bpp: %d' % int(rec_bpp))
        #print('Received capTime: %d' % int(rec_capTime))
        #print('Received path: %s' % rec_path)
        write_mutex.acquire()
        if prevFlag != rec_flags:
            print('Received flags: %d' % int(rec_flags))

        flags = flags | rec_flags
        prevFlag = rec_flags
        write_mutex.release()
        #print('Received ' + repr(data))
        time.sleep(0.1)
th = threading.Thread(target=client_thread)

class M25_widget(QWidget):
    def __init__(self, napari_viewer: Viewer):
        super().__init__()
        self.viewer = napari_viewer

        #Layout the GUI
        self.ui = m25_widget.Ui_Form()
        self.ui.setupUi(self)

        global path
        # self.WritePLineEdit.setText(path)
        print(exe_path)
        # rc = call("start cmd /K " + myEXE, cwd=exe_path, shell=True)  # run `cmdline` in `dir`
        # global bpp
        # bpp = 8
        # global run
        # th.start()


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

    def AcquireState(self):
        global flags
        global write_mutex
        write_mutex.acquire()
        if flags & CAMERAS_ACQUIRED or flags & CAPTURING:
            pass
        else:
            flags |= ACQUIRE_CAMERAS
        write_mutex.release()

    def ReleaseState(self):
        global write_mutex
        global flags
        write_mutex.acquire()
        if flags & CAPTURING:
            pass
        else:
            flags |= RELEASE_CAMERAS
        write_mutex.release()

    def ConfState(self):
        global write_mutex
        global flags
        write_mutex.acquire()
        if flags & CAPTURING or flags & ACQUIRING_CAMERAS or flags & CAMERAS_ACQUIRED:
            pass
        else:
            flags |= CHANGE_CONFIG
        write_mutex.release()

    def CaptureState(self):
        global write_mutex
        global flags
        write_mutex.acquire()
        if flags & CAMERAS_ACQUIRED:
            if flags & CAPTURING:
                pass
            else:
                flags |= START_CAPTURE
        write_mutex.release()

    def closeEvent(self, event):
        global write_mutex
        global flags
        write_mutex.acquire()
        flags |= EXIT_THREAD
        write_mutex.release()
        print('close event fired')
        global run
        run = False
        th.join


class FindReplaceDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)

class QtLogger(logging.Handler):
    """
    Class to changing logging handler to the napari log output display
    """
    def __init__(self, widget):
        super().__init__()
        self.widget = widget

    # necessary to be a logging handler
    def emit(self, record):
        msg = self.format(record)
        self.widget.appendPlainText(msg)


@napari_hook_implementation
def napari_experimental_provide_dock_widget():
    # you can return either a single widget, or a sequence of widgets
    # each widget is accessible as a new plugin that stacks in the side panel
    return [(M25_widget, {'name': 'M25_viewer'})]
