/*
    ==============================================================================

    JackPassthroughAnalyser.h
    Created: 19/06/2024
    Author:  Dan Leinir Turthra Jensen <admin@leinir.dk>

    ==============================================================================
*/

#pragma once

#include "JUCEHeaders.h"

#include <QPolygonF>
#include <QRectF>

// This is heavily based on Frequalizer's Analyser
// https://github.com/ffAudio/Frequalizer/blob/master/Source/Analyser.h
class JackPassthroughAnalyserPrivate;
class JackPassthroughAnalyser : public juce::Thread {
public:
    explicit JackPassthroughAnalyser();
    ~JackPassthroughAnalyser() override;

    void addAudioData(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels);

    void setupAnalyser(int audioFifoSize, float sampleRateToUse);

    void run() override;

    void createPath(QPolygonF& p, const QRectF &bounds, float minFreq);

    bool checkForNewData();
private:
    JackPassthroughAnalyserPrivate *d{nullptr};
};
