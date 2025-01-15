# LibZynthbox - The Zynthbox Core Library

A set of QtQuick Components to be used by the Zynthbox Sketchpad UI, as well
as core functionality built on top of Jack, and JUCE and tracktion, such as the
SyncTimer sequencer, MidiRouter, and the SamplerSynth sample player. This is
all  built as a C++ library and exposed as a Python module using Shiboken2.

These should be configured to install into somewhere that Python knows about.
On most systems, this will mean doing something akin to the following in your
clone location:

```
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

That should pick up your Qt, ECM, Shiboken2, and Python installations, and use
them to build and install into the expected locations. If those are missing,
you will likely need to install those packages. On zynthian, you can do this by
running:

```
apt install extra-cmake-modules libkf5pty-dev qtbase5-dev kirigami2-dev qtdeclarative5-dev librtmidi-dev libwebkit2gtk-4.0-dev libpyside2-dev libshiboken2-dev libclang-dev llvm-dev libappimage-dev
```

Once installed, you should be able to use the components simply by adding
something like the following to your qml files:

```
import io.zynthbox.components 1.0 as Zynthbox

Zynthbox.PlayGrid {
    id: component
    name: "My Awesome PlayGrid"
    // ...the rest of your implementation goes here (or just use the BasePlayGrid component from zynthian-qml)
}

```

Using the python module is done by the Sketchpad module in Zynthbox itself, and
you would not usually do this yourself (see the contents of zynthian-qml for how
that works).
