from m25_plugin.qt5_designer import M25_ui
import sys
import binascii
import threading
import time
import struct
from lib2to3.pytree import convert
from struct import unpack
import socket
from subprocess import call
import logging

from ctypes import *
from typing import Any, Tuple

from M25_ui import Ui_MainWindow
from PyQt5.QtWidgets import (
    QApplication, QDialog, QMainWindow, QFileDialog
)
class M25_widget(QWidget):
    def __init__(self,napari_viewer: Viewer):
        super().__init__()
        self.viewer = napari_viewer

        ui =Ui_MainWindow.()
        ui.setupUi(self)

