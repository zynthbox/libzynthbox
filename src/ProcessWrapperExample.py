import sys
from PySide2.QtCore import QByteArray, QCoreApplication, Slot, QTimer
import Zynthbox

@Slot()
def handleStateChanged():
    print(f"--- ProcessWrapper state is now {p.state()}\n")
    if p.state() == Zynthbox.ProcessWrapper.ProcessState.NotRunningState:
        app.quit()

@Slot()
def talkToProcess():
    print("--- Call a couple of functions - first a non-blocking one: set 15 1")
    p.send(QByteArray(b"set 15 1\n"))
    print("--- Non-blocking function (without output) called - now calling a blocking function (which must return some output)")
    theResult = p.call(b"preset file:///zynthian/zynthian-data/presets/lv2/synthv1_392Synthv1Patches.presets.lv2/392Synthv1Patches_NoizeExport01.ttl")
    print(f"--- The result data from the blocking call was:\n--- START RESULT ---\n{theResult}\n--- END RESULT ---")
    p.stop()


@Slot(str)
def handleStandardOutput(output):
    print(f"--- STDOUT BEGIN\n{output}\n--- STDOUT END")
    # We know the last thing output by jalv on startup is the alsa
    # playback configuration, we can use that here. Other tools will
    # require other estimates, but that's out test case here, and it
    # shows how to use that knowledge to perform actions
    if output.startswith("ALSA: use") and output.endswith("periods for playback\n"):
        talkToProcess()

@Slot(str)
def handleStandardError(output):
    print(f"--- STDERR BEGIN\n{output}\n--- STDERR END")
    # if "Comm buffers" in output and "Update rate: " in output:

if __name__ == "__main__":
    app = QCoreApplication()

    p = Zynthbox.ProcessWrapper(app)
    p.standardOutputChanged.connect(handleStandardOutput)
    p.standardErrorChanged.connect(handleStandardError)
    p.stateChanged.connect(handleStateChanged)
    print("--- Created process wrapper, now starting process")
    p.start("jalv", ["-n", "synthv1-py", "http://synthv1.sourceforge.net/lv2"])
    print("--- Process started")

    sys.exit(app.exec_())
