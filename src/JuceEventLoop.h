#include "JUCEHeaders.h"

class JuceEventLoop : public Thread {
public:
    JuceEventLoop()
        : Thread("Juce EventLoop Thread"),
        initializer(new ScopedJuceInitialiser_GUI()) {}

    void run() override {
        MessageManager::getInstance()->runDispatchLoop();
    }

    void start() {
        startThread();
    }

    void stop() {
        stopThread(500);
    }

private:
    ScopedJuceInitialiser_GUI *initializer{nullptr};
};
