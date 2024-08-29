/*
    ==============================================================================

    JackPassthroughAnalyser.cpp
    Created: 20/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#include "JackPassthroughAnalyser.h"

class JackPassthroughAnalyserPrivate {
public:
    JackPassthroughAnalyserPrivate() {};

    inline float indexToX(float index, float minFreq) const {
        const auto freq = (sampleRate * index) / fft.getSize();
        return (freq > 0.01f) ? std::log(freq / minFreq) / std::log(2.0f) : 0.0f;
    }

    inline float binToY(float bin,  const QRectF &bounds) const {
        const float infinity = -80.0f;
        return juce::jmap(juce::Decibels::gainToDecibels(bin, infinity), infinity, 0.0f, float(bounds.bottom()), float(bounds.top()));
    }

    juce::WaitableEvent waitForData;
    juce::CriticalSection pathCreationLock;

    float sampleRate{};

    juce::dsp::FFT fft                           { 12 };
    juce::dsp::WindowingFunction<float> windowing { size_t(fft.getSize()), juce::dsp::WindowingFunction<float>::hann, true };
    juce::AudioBuffer<float> fftBuffer           { 1, fft.getSize() * 2 };

    juce::AudioBuffer<float> averager            { 5, fft.getSize() / 2 };
    int averagerPtr = 1;

    juce::AbstractFifo abstractFifo              { 48000 };
    juce::AudioBuffer<float> audioFifo;

    std::atomic<bool> newDataAvailable;

    void task() {
        fftBuffer.clear();

        int start1, block1, start2, block2;
        abstractFifo.prepareToRead(fft.getSize(), start1, block1, start2, block2);
        if (block1 > 0) {
            fftBuffer.copyFrom(0, 0, audioFifo.getReadPointer(0, start1), block1);
        }
        if (block2 > 0) {
            fftBuffer.copyFrom(0, block1, audioFifo.getReadPointer(0, start2), block2);
        }
        abstractFifo.finishedRead((block1 + block2) / 2);

        windowing.multiplyWithWindowingTable(fftBuffer.getWritePointer(0), size_t(fft.getSize()));
        fft.performFrequencyOnlyForwardTransform(fftBuffer.getWritePointer(0));

        juce::ScopedLock lockedForWriting(pathCreationLock);
        averager.addFrom(0, 0, averager.getReadPointer(averagerPtr), averager.getNumSamples(), -1.0f);
        averager.copyFrom(averagerPtr, 0, fftBuffer.getReadPointer(0), averager.getNumSamples(), 1.0f / (averager.getNumSamples() * (averager.getNumChannels() - 1)));
        averager.addFrom(0, 0, averager.getReadPointer(averagerPtr), averager.getNumSamples());
        if (++averagerPtr == averager.getNumChannels()) {
            averagerPtr = 1;
        }

        newDataAvailable = true;
    }
};

JackPassthroughAnalyser::JackPassthroughAnalyser()
    : juce::Thread ("JackPassthroughAnalyser")
    , d(new JackPassthroughAnalyserPrivate())
{
    d->averager.clear();
}

JackPassthroughAnalyser::~JackPassthroughAnalyser()
{
    delete d;
}

void JackPassthroughAnalyser::addAudioData(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels)
{
    if (d->abstractFifo.getFreeSpace() >= buffer.getNumSamples()) {
        int start1, block1, start2, block2;
        d->abstractFifo.prepareToWrite (buffer.getNumSamples(), start1, block1, start2, block2);
        d->audioFifo.copyFrom(0, start1, buffer.getReadPointer(startChannel), block1);
        if (block2 > 0) {
            d->audioFifo.copyFrom(0, start2, buffer.getReadPointer(startChannel, block1), block2);
        }

        for (int channel = startChannel + 1; channel < startChannel + numChannels; ++channel)
        {
            if (block1 > 0) {
                d->audioFifo.addFrom(0, start1, buffer.getReadPointer(channel), block1);
            }
            if (block2 > 0) {
                d->audioFifo.addFrom(0, start2, buffer.getReadPointer(channel, block1), block2);
            }
        }
        d->abstractFifo.finishedWrite(block1 + block2);
        d->waitForData.signal();
    }
}

void JackPassthroughAnalyser::setupAnalyser(int audioFifoSize, float sampleRateToUse)
{
    d->sampleRate = sampleRateToUse;
    d->audioFifo = juce::AudioBuffer<float>(1, audioFifoSize);
    d->abstractFifo.setTotalSize(audioFifoSize);
    startThread(Priority::normal);
}

void JackPassthroughAnalyser::run()
{
    while (threadShouldExit() == false) {
        if (d->abstractFifo.getNumReady() >= d->fft.getSize()) {
            d->task();
        }
        if (d->abstractFifo.getNumReady() < d->fft.getSize()) {
            d->waitForData.wait(100);
        }
    }
}

void JackPassthroughAnalyser::createPath(QPolygonF& p, const QRectF& bounds, float minFreq)
{
    p.clear();
    p.reserve(8 + d->averager.getNumSamples() * 3);

    juce::ScopedLock lockedForReading(d->pathCreationLock);
    const auto* fftData = d->averager.getReadPointer(0);
    const auto  factor  = bounds.width() / 10.0f;

    p << QPointF(bounds.left() + factor * d->indexToX(0, minFreq), d->binToY(fftData[0], bounds));
    for (int i = 0; i < d->averager.getNumSamples(); ++i) {
        p << QPointF(bounds.left() + factor * d->indexToX(float (i), minFreq), d->binToY(fftData[i], bounds));
    }
}

bool JackPassthroughAnalyser::checkForNewData()
{
    auto available = d->newDataAvailable.load();
    d->newDataAvailable.store(false);
    return available;
}

