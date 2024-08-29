
#pragma once

#include <QObject>
#include <QCoreApplication>

struct ClipCommand;
class ClipAudioSource;
class SamplerSynthPrivate;
class SamplerSynthSound;
class SyncTimerPrivate;
namespace tracktion_engine {
    class Engine;
}
class SamplerSynth : public QObject
{
    Q_OBJECT
    friend class SyncTimerPrivate;
public:
    static SamplerSynth *instance();

    explicit SamplerSynth(QObject *parent = nullptr);
    ~SamplerSynth() override;

    void initialize(tracktion::Engine *engine);
    tracktion::Engine *engine() const;

    /**
     * \brief Returns the sample rate of the jack process
     * @return The samplerate used by jack
     */
    double sampleRate() const;

    void registerClip(ClipAudioSource *clip);
    void unregisterClip(ClipAudioSource *clip);
    SamplerSynthSound *clipToSound(ClipAudioSource *clip) const;
protected:
    // Some stuff to ensure SyncTimer can operate with sufficient speed
    void handleClipCommand(ClipCommand* clipCommand, quint64 currentTick);

    /**
     * \brief Set a given samplersynth channel as enabled (or not) for processing
     * @note This does *not* clear the buffer or anything. If set disabled, you should also disconnect from the client's ports
     * @param channel The channel index (-2 being the uneffected global channel, -1 being the effected global channel, and 0 trough 9 being the zl channels)
     * @param enabled True to enable the channel for processing, false to disable it
     */
    void setChannelEnabled(const int &channel, const bool& enabled) const;

private:
    SamplerSynthPrivate *d{nullptr};
};
