##Things I added
self.onlyInt = QtGui.QIntValidator()
self.radioButton.toggled.connect(self.onClicked)
self.radioButton.value = 8
self.radioButton_2.toggled.connect(self.onClicked)
self.radioButton_2.value = 16
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
self.BrowsePushButton.clicked.connect(self.browseState)
self.AcquireCamsButton.clicked.connect(self.AcquireState)
self.ReleaseCamsButton.clicked.connect(self.ReleaseState)
self.ConfButton.clicked.connect(self.ConfState)
self.CapturePushButton.clicked.connect(self.CaptureState)
self.GainlineEdit.setText("0.0")
self.GainlineEdit.textChanged.connect(self.sync_GainLineEdit)