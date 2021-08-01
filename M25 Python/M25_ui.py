# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'M25.ui'
#
# Created by: PyQt5 UI code generator 5.15.4
#
# WARNING: Any manual changes made to this file will be lost when pyuic5 is
# run again.  Do not edit this file unless you know what you are doing.


from PyQt5 import QtCore, QtGui, QtWidgets


class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName("MainWindow")
        MainWindow.resize(640, 480)
        self.centralwidget = QtWidgets.QWidget(MainWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.ResolutionLabel = QtWidgets.QLabel(self.centralwidget)
        self.ResolutionLabel.setGeometry(QtCore.QRect(70, 20, 81, 16))
        self.ResolutionLabel.setObjectName("ResolutionLabel")
        self.HorzLabel = QtWidgets.QLabel(self.centralwidget)
        self.HorzLabel.setGeometry(QtCore.QRect(20, 40, 71, 20))
        self.HorzLabel.setObjectName("HorzLabel")
        self.VertLabel = QtWidgets.QLabel(self.centralwidget)
        self.VertLabel.setGeometry(QtCore.QRect(130, 40, 71, 20))
        self.VertLabel.setObjectName("VertLabel")
        self.HorzLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.HorzLineEdit.setGeometry(QtCore.QRect(10, 60, 81, 20))
        self.HorzLineEdit.setObjectName("HorzLineEdit")
        self.VertLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.VertLineEdit.setGeometry(QtCore.QRect(110, 60, 81, 20))
        self.VertLineEdit.setObjectName("VertLineEdit")
        self.FPSLabel = QtWidgets.QLabel(self.centralwidget)
        self.FPSLabel.setGeometry(QtCore.QRect(10, 90, 191, 16))
        self.FPSLabel.setObjectName("FPSLabel")
        self.FPSLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.FPSLineEdit.setGeometry(QtCore.QRect(10, 110, 91, 20))
        self.FPSLineEdit.setObjectName("FPSLineEdit")
        self.BPPLabel = QtWidgets.QLabel(self.centralwidget)
        self.BPPLabel.setGeometry(QtCore.QRect(10, 140, 171, 16))
        self.BPPLabel.setObjectName("BPPLabel")
        self.EXPLabel = QtWidgets.QLabel(self.centralwidget)
        self.EXPLabel.setGeometry(QtCore.QRect(10, 210, 151, 16))
        self.EXPLabel.setObjectName("EXPLabel")
        self.EXPLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.EXPLineEdit.setGeometry(QtCore.QRect(10, 230, 91, 20))
        self.EXPLineEdit.setObjectName("EXPLineEdit")
        self.CaptureLabel = QtWidgets.QLabel(self.centralwidget)
        self.CaptureLabel.setGeometry(QtCore.QRect(10, 260, 181, 21))
        self.CaptureLabel.setObjectName("CaptureLabel")
        self.CapTimeLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.CapTimeLineEdit.setGeometry(QtCore.QRect(10, 280, 91, 20))
        self.CapTimeLineEdit.setObjectName("CapTimeLineEdit")
        self.CapturePushButton = QtWidgets.QPushButton(self.centralwidget)
        self.CapturePushButton.setGeometry(QtCore.QRect(10, 320, 75, 23))
        self.CapturePushButton.setObjectName("CapturePushButton")
        self.label = QtWidgets.QLabel(self.centralwidget)
        self.label.setGeometry(QtCore.QRect(300, 40, 171, 16))
        self.label.setObjectName("label")
        self.WritePLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.WritePLineEdit.setGeometry(QtCore.QRect(220, 60, 251, 20))
        self.WritePLineEdit.setObjectName("lineEdit")
        self.BrowsePushButton_2 = QtWidgets.QPushButton(self.centralwidget)
        self.BrowsePushButton_2.setGeometry(QtCore.QRect(480, 60, 75, 23))
        self.BrowsePushButton_2.setObjectName("BrowsePushButton_2")
        self.radioButton = QtWidgets.QRadioButton(self.centralwidget)
        self.radioButton.setGeometry(QtCore.QRect(10, 160, 131, 18))
        self.radioButton.setObjectName("radioButton")
        self.radioButton_2 = QtWidgets.QRadioButton(self.centralwidget)
        self.radioButton_2.setGeometry(QtCore.QRect(10, 180, 151, 18))
        self.radioButton_2.setObjectName("radioButton_2")
        self.MsgLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.MsgLineEdit.setGeometry(QtCore.QRect(160, 370, 401, 20))
        self.MsgLineEdit.setObjectName("MsgLineEdit")
        self.StatusLabel = QtWidgets.QLabel(self.centralwidget)
        self.StatusLabel.setGeometry(QtCore.QRect(10, 350, 111, 16))
        self.StatusLabel.setObjectName("StatusLabel")
        self.StatusLineEdit = QtWidgets.QLineEdit(self.centralwidget)
        self.StatusLineEdit.setGeometry(QtCore.QRect(10, 370, 113, 20))
        self.StatusLineEdit.setObjectName("StatusLineEdit")
        self.MsgLabel = QtWidgets.QLabel(self.centralwidget)
        self.MsgLabel.setGeometry(QtCore.QRect(330, 350, 121, 16))
        self.MsgLabel.setObjectName("MsgLabel")
        MainWindow.setCentralWidget(self.centralwidget)
        self.statusbar = QtWidgets.QStatusBar(MainWindow)
        self.statusbar.setObjectName("statusbar")
        MainWindow.setStatusBar(self.statusbar)
        self.menubar = QtWidgets.QMenuBar(MainWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 640, 22))
        self.menubar.setObjectName("menubar")
        self.menuM25_Control = QtWidgets.QMenu(self.menubar)
        self.menuM25_Control.setObjectName("menuM25_Control")
        MainWindow.setMenuBar(self.menubar)
        self.menubar.addAction(self.menuM25_Control.menuAction())

        ##Things I added
        self.onlyInt = QtGui.QIntValidator()
        self.radioButton.toggled.connect(self.onClicked)
        self.radioButton.value = 8
        self.radioButton_2.toggled.connect(self.onClicked)
        self.radioButton_2.value = 12
        self.HorzLineEdit.setText("1920")
        self.HorzLineEdit.textChanged.connect(self.sync_HorzLineEdit)
        self.HorzLineEdit.setValidator(self.onlyInt)
        self.VertLineEdit.setText("1200")
        self.VertLineEdit.textChanged.connect(self.sync_VertLineEdit)
        self.VertLineEdit.setValidator(self.onlyInt)
        self.FPSLineEdit.setText("65")
        self.FPSLineEdit.textChanged.connect(self.sync_FPSLineEdit)
        self.FPSLineEdit.setValidator(self.onlyInt)
        self.EXPLineEdit.setText("6700")
        self.EXPLineEdit.textChanged.connect(self.sync_EXPLineEdit)
        self.EXPLineEdit.setValidator(self.onlyInt)
        self.CapTimeLineEdit.setText("10")
        self.CapTimeLineEdit.textChanged.connect(self.sync_CapTimeLineEdit)
        self.CapTimeLineEdit.setValidator(self.onlyInt)
        self.MsgLineEdit.setText("Default")
        self.StatusLineEdit.setText("OFFLINE")
        self.radioButton.setChecked(True)

        self.retranslateUi(MainWindow)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def retranslateUi(self, MainWindow):
        _translate = QtCore.QCoreApplication.translate
        MainWindow.setWindowTitle(_translate("MainWindow", "MainWindow"))
        self.ResolutionLabel.setText(_translate("MainWindow", "Resolution"))
        self.HorzLabel.setText(_translate("MainWindow", "Horizontal"))
        self.VertLabel.setText(_translate("MainWindow", "Vertical"))
        self.FPSLabel.setText(_translate("MainWindow", "Frames Per Second"))
        self.BPPLabel.setText(_translate("MainWindow", "Bits Per Pixel"))
        self.EXPLabel.setText(_translate("MainWindow", "Exposure(µs)"))
        self.CaptureLabel.setText(_translate("MainWindow", "Capture Time(s)"))
        self.CapturePushButton.setText(_translate("MainWindow", "Capture"))
        self.label.setText(_translate("MainWindow", "Image Write Path"))
        self.BrowsePushButton_2.setText(_translate("MainWindow", "Browse"))
        self.radioButton.setText(_translate("MainWindow", "8 bpp"))
        self.radioButton_2.setText(_translate("MainWindow", "12 bpp"))
        self.StatusLabel.setText(_translate("MainWindow", "Status:"))
        self.MsgLabel.setText(_translate("MainWindow", "Messages:"))
        self.menuM25_Control.setTitle(_translate("MainWindow", "M25 Control"))
