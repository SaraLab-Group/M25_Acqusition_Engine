import sys
import binascii
import threading
import time
import struct
import mmap
import math
from matplotlib import pyplot as plt
import numpy as np
#from StringIO import StringIO
from PIL import *
from PIL import ImageDraw


from datetime import date
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

today = date.today()

HOST = '127.0.0.1'  # The server's hostname or IP address
PORT = 27015        # The port used by the server
run = True


# Signaling Flags
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
START_LIVE = 0x100000
LIVE_RUNNING = 0x200000
STOP_LIVE = 0x400000
START_Z_STACK = 0x800000
Z_STACK_RUNNING = 0x1000000
STOP_Z_STACK = 0x2000000
EXIT_THREAD = 0x80000000
DEFAULT_FPS = 65

# shared mem flags
WRITING_BUFF1 = 0x1
WRITING_BUFF2 = 0x2
READING_BUFF1 = 0x4
READING_BUFF2 = 0x8



horz: int = 960
vert: int = 600
fps: int = 2
exp: int = 250000
bpp: int = 8
capTime: int = 10
z_frames: int = 0;
gain: float = 0.0
path = "\0"*255
path = "D:\\Ant1 Test\\raws"
proName = "\0"*255
proName = today.strftime("%Y%m%d_M25")  #As Per Request
flags: int = 0
exe_path: str = r'C:\Users\Callisto\Documents\abajor\M25_basler\basler_candidate\ide\x64\Debug'
myEXE = "Basler_Candidate.exe"

live_running = False
zMode = False
singleMode = False
singleCam = 13

write_mutex = threading.Lock()
def client_thread():
    #time.sleep(2)
    global run
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
            global z_frames
            global gain
            global path
            global proName
            global flags
            #global zMode
            write_mutex.acquire()
            values = (horz, vert, fps, exp, bpp, z_frames, capTime, path.encode(), proName.encode(), flags, gain)
            packer = struct.Struct('L L L L L L L 255s 255s L d')
            packed_data = packer.pack(*values)
            s.sendall(packed_data)

            #print('flags: %d' % int(flags))

            #print('flags after: %d' % int(flags))


            if flags & EXIT_THREAD:
                write_mutex.release()
                print('Closing Socket')
                run = False
                s.close()
            else:
                flags = 0
                write_mutex.release()
                data: bytes = s.recv(1024)
                (rec_horz, rec_vert, rec_fps, rec_exp, rec_bpp, rec_z_frames, rec_capTime,
                 rec_path, rec_proName,
                 rec_flags, rec_gain) = unpack(
                    'L L L L L L L'
                    '255s'
                    '255s'
                    'L'
                    'd',
                    data
                )
            #print(inData.path);
            #outData: bytes = inData
            #values: Tuple[Any, Any, Any, Any, Any, Any, str, Any] = (inData.horz, inData.vert, inData.fps, inData.exp, inData.bpp, inData.capTime, inData.path, inData.flags)
            #packer = struct.Struct('L L L L L L 255s H')
            #packed_data = Payload(inData)
            #s.sendall(outData)

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

global sleep_mutex
sleep_mutex = threading.Event()

def liveView_func():
    global horz
    global vert
    global bpp
    global flags
    global run
    global live_running

    if run:
        live_running = True

        # Create Memory Maps
        imageSize = np.uint64((horz * vert) / 8 * bpp)

        # adhearing to sector aligned memory
        if imageSize % 512 != 0:
            imageSize += (512 - (imageSize % 512))

        imgObj = np.dtype(np.uint8, imageSize)

        RW_flags = mmap.mmap(0, 8, "Local\\Flags")  # for basic signaling
        buff1 = mmap.mmap(0, int(imageSize * 25), "Local\\buff1")
        buff2 = mmap.mmap(0, int(imageSize * 25), "Local\\buff2")
        frameVect = []
        read_flags = np.uint8
        #fig = plt.figure(figsize=(10, 7))
        #fig, ax_list = plt.subplots(1,1)

        ##Preload
#        img_x = []
#        ax_list = ax_list.ravel()
#
#        for i in range(25):
#            frameVect.append(buff2.read(int(imageSize)))
#
#        for i in range(25):
#            image_conv = Image.frombuffer("L", [horz, vert],
#                                         frameVect[i],
#                                         'raw', 'L', 0, 1)
#           # fig.add_subplot(5, 5, i + 1)
#           img_x.append(ax_list[i].imshow(image_conv))

        frameVect.clear()
        plt.ion()
        buff1.seek(0)
        buff2.seek(0)

        if singleMode:
            myimg = plt.imshow(np.zeros([vert, horz]))
            dst = Image.new('L', [horz, vert])
        else:
            if horz < 960:
                myimg = plt.imshow(np.zeros([vert*5, horz*5]))
            else:
                myimg = plt.imshow(np.zeros([vert*2, horz*2]))
            dst = Image.new('L', [horz * 5, vert * 5])

        while live_running:
            start = time.time()
            # do stuff
            read_flags = RW_flags.read_byte()
            RW_flags.seek(0)
            if read_flags & WRITING_BUFF1:
                print("BUFF2")
                read_flags |= READING_BUFF2
                #read_flags &= ~(READING_BUFF1)
                RW_flags.write_byte(read_flags)
                for i in range(25):
                    frameVect.append(buff2.read(int(imageSize)))
            else:
                print("BUFF1")
                read_flags |= READING_BUFF1
                #read_flags &= ~(READING_BUFF2)
                RW_flags.write_byte(read_flags)
                for i in range(25):
                    frameVect.append(buff2.read(int(imageSize)))
            RW_flags.seek(0)
            read_flags &= ~(READING_BUFF1 | READING_BUFF2)
            RW_flags.write_byte(read_flags)
            RW_flags.seek(0)
            buff1.seek(0)
            buff2.seek(0)
            print("Then length: ", len(frameVect[i]))
            # RW_flags &= ~( READING_BUFF1 | READING_BUFF2 )

            if singleMode:
                dst = Image.frombuffer("L", [horz, vert],
                                              frameVect[singleCam - 1],
                                              'raw', 'L', 0, 1)
                myimg.set_data(dst)
            else:
                for i in range(25):
                    image_conv = Image.frombuffer("L", [horz, vert],
                                                  frameVect[i],
                                                  'raw', 'L', 0, 1)
                    dst.paste(image_conv, [horz * (i % 5), vert * ((i // 5 % 5))])

                if horz < 960:
                    myimg.set_data(dst)
                else:
                    myimg.set_data(dst.resize((horz*2, vert*2)))

            #plt.axis('off')
            #plt.show()
            plt.pause(0.001)

            if not live_running:
                plt.close()

            frameVect.clear()
            #time.sleep(0.001)
            print("total time taken this loop: ", time.time() - start)
        plt.close()

def liveView_thread():
    global run
    global sleep_mutex
    print("Start LiveView")
    while run:
        print("Before Wait")
        sleep_mutex.wait(None)
        print("Before LiveView Function")
        if run:
            liveView_func()
        print("Bottom of Looop")


th = threading.Thread(target=client_thread)
l_th = threading.Thread(target=liveView_thread)


class Window(QMainWindow, Ui_MainWindow):

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setupUi(self)
        global path
        self.WritePLineEdit.setText(path)
        self.PNameLineEdit.setText(proName)
        print(exe_path)
        rc = call("start cmd /K " + myEXE, cwd=exe_path, shell=True)  # run `cmdline` in `dir`
        global bpp
        bpp = 8
        global run
        global singleCam
        singleCam = self.CamSpinBox.value()
        th.start()
        l_th.start()


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

    def sync_GainLineEdit(self, text):
        global gain
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            gain = (float(text))
        else:
            gain = float(0.0)
        write_mutex.release()

    def sync_PNameLineEdit(self, text):
        global proName
        global write_mutex
        write_mutex.acquire()
        proName = text
        write_mutex.release()

    def sync_StackFramesEdit(self, text):
        global z_frames
        global write_mutex
        write_mutex.acquire()
        if len(text) > 0:
            z_frames = int(text)
        else:
            z_frames = int(0)
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

    def checkClicked(self):
        global zMode
        global fps
        global write_mutex
        write_mutex.acquire()
        box = self.sender()
        if box.isChecked():
            zMode = True
        else:
            zMode = False
        write_mutex.release()

    def singleClicked(self):
        global singleMode
        global write_mutex
        write_mutex.acquire()
        box = self.sender()
        if box.isChecked():
            singleMode = True
        else:
            singleMode = False
        write_mutex.release()

    def singleSpinClicked(self):
        global singleCam
        global write_mutex
        write_mutex.acquire()
        singleCam = self.CamSpinBox.value()
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
        global zMode
        global flags
        write_mutex.acquire()
        if flags & CAMERAS_ACQUIRED:
            if flags & CAPTURING:
                pass
            else:
                if zMode == True:
                    flags |= START_Z_STACK
                else:
                    flags |= START_CAPTURE
        write_mutex.release()

    def toggleLive(self):
        global write_mutex
        global sleep_mutex
        global flags
        global live_running
        write_mutex.acquire()
        if flags & CAMERAS_ACQUIRED:
            if flags & CAPTURING:
                pass
            elif live_running:
                live_running = False
                flags |= STOP_LIVE
                flags &= ~(LIVE_RUNNING)
                sleep_mutex.clear()
            else:
                live_running = True
                flags |= START_LIVE
                sleep_mutex.set()
        write_mutex.release()



    def closeEvent(self, event):
        global write_mutex
        global sleep_mutex
        global flags
        write_mutex.acquire()
        flags |= EXIT_THREAD
        write_mutex.release()
        print('close event fired')
        time.sleep(0.2)
        global run
        run = False
        global live_running
        live_running = False
        sleep_mutex.set()
        th.join
        l_th.join
        time.sleep(0.2)


class FindReplaceDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = Window()
    win.show()
    sys.exit(app.exec())

