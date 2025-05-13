import sys
from PySide2.QtCore import QByteArray, QCoreApplication, QObject, Slot, QTimer
import Zynthbox

class process_whisperer(QObject):
    def __init__(self, parent):
        super(process_whisperer, self).__init__(parent)

    @Slot()
    def start(self):
        self.processWrapper = Zynthbox.ProcessWrapper(app)
        self.processWrapper.setCommandPrompt("\n> ")
        # self.processWrapper.standardOutputChanged.connect(self.handleStandardOutput)
        self.processWrapper.standardErrorChanged.connect(self.handleStandardError)
        self.processWrapper.stateChanged.connect(self.handleStateChanged)
        print("--- Created process wrapper, now starting process")
        startTransaction = self.processWrapper.start("jalv", ["-n", "synthv1-py", "http://synthv1.sourceforge.net/lv2"])
        print("--- Process started")
        while startTransaction.state() != Zynthbox.ProcessWrapperTransaction.TransactionState.CompletedState:
            QCoreApplication.instance().processEvents()
        print(f"--- Process startup completed with output:\n{startTransaction.standardOutput()}\n---Process startup output end ---")
        startTransaction.release()
        self.talkToProcess()

    @Slot()
    def handleStateChanged(self):
        if self.processWrapper is not None:
            print(f"--- ProcessWrapper state is now {self.processWrapper.state()}\n")
            if self.processWrapper.state() == Zynthbox.ProcessWrapper.ProcessState.NotRunningState:
                print(f"--- The process has entered the not-running state, so let's quit")
                self.processWrapper.deleteLater()
                self.processWrapper = None
                QCoreApplication.quit()

    @Slot()
    def talkToProcess(self):
        print("--- Call a couple of functions - first a non-blocking one: set 15 1")
        self.processWrapper.send("set 15 1\n")
        print("--- Non-blocking function (without output) called - now calling a blocking function (which must return some output)")
        theResult = self.processWrapper.call("preset file:///zynthian/zynthian-data/presets/lv2/synthv1_392Synthv1Patches.presets.lv2/392Synthv1Patches_NoizeExport01.ttl")
        print(f"--- The result data from the blocking call was:\n--- START RESULT ---\n{theResult.standardOutput()}\n--- END RESULT ---")
        print(f"--- Now stopping the process ---")
        self.processWrapper.stop()
        print(f"--- Requested stopping the process ---")

    @Slot(str)
    def handleStandardOutput(self, output):
        print(f"--- STDOUT BEGIN\n{output}\n--- STDOUT END")

    @Slot(str)
    def handleStandardError(self, output):
        print(f"--- STDERR BEGIN\n{output}\n--- STDERR END")
        # if "Comm buffers" in output and "Update rate: " in output:

if __name__ == "__main__":
    app = QCoreApplication()

    pw = process_whisperer(app)
    timer = QTimer(app)
    timer.timeout.connect(pw.start)
    timer.setInterval(100)
    timer.setSingleShot(True)
    timer.start()

    sys.exit(app.exec_())
