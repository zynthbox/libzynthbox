#include "SamplerSynthVoice.h"

#include "ClipAudioSourcePositionsModel.h"
#include "ClipCommand.h"
#include "libzl.h"
#include "SamplerSynthSound.h"
#include "SyncTimer.h"

#include <QDebug>
#include <QThread>

class SamplerSynthVoicePrivate {
public:
    SamplerSynthVoicePrivate() {
        syncTimer = qobject_cast<SyncTimer*>(SyncTimer_instance());
    }

    SyncTimer *syncTimer{nullptr};
    QList<ClipCommand*> clipCommandsForDeleting;
    ClipCommand *clipCommand{nullptr};
    ClipAudioSource *clip{nullptr};
    qint64 clipPositionId{-1};
    quint64 startTick{0};
    double maxSampleDeviation{0.0};
    double pitchRatio = 0;
    double sourceSamplePosition = 0;
    double sourceSampleLength = 0;
    float lgain = 0, rgain = 0;
    ADSR adsr;
};

SamplerSynthVoice::SamplerSynthVoice()
    : QObject()
    , juce::SamplerVoice()
    , d(new SamplerSynthVoicePrivate)
{
}

SamplerSynthVoice::~SamplerSynthVoice()
{
    delete d;
}

bool SamplerSynthVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SamplerSynthSound*> (sound) != nullptr;
}

void SamplerSynthVoice::setCurrentCommand(ClipCommand *clipCommand)
{
    if (d->clipCommand) {
        // This means we're changing what we should be doing in playback, and we need to delete the old one
        if (clipCommand->changeLooping) {
            d->clipCommand->looping = clipCommand->looping;
            d->clipCommand->changeLooping = true;
        }
        if (clipCommand->changePitch) {
            d->clipCommand->pitchChange = clipCommand->pitchChange;
            d->clipCommand->changePitch = true;
        }
        if (clipCommand->changeSpeed) {
            d->clipCommand->speedRatio = clipCommand->speedRatio;
            d->clipCommand->changeSpeed = true;
        }
        if (clipCommand->changeGainDb) {
            d->clipCommand->gainDb = clipCommand->gainDb;
            d->clipCommand->changeGainDb = true;
        }
        if (clipCommand->changeVolume) {
            d->clipCommand->volume = clipCommand->volume;
            d->clipCommand->changeVolume = true;
            d->lgain = d->clipCommand->volume;
            d->rgain = d->clipCommand->volume;
        }
        if (clipCommand->changeSlice) {
            d->clipCommand->slice = clipCommand->slice;
        }
        if (clipCommand->startPlayback) {
            // This should be interpreted as "restart playback" in this case, so... reset the current position
            if (auto* playingSound = static_cast<SamplerSynthSound*> (getCurrentlyPlayingSound().get())) {
                d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate());
            }
        }
        d->clipCommandsForDeleting << clipCommand;
    } else {
        d->clipCommand = clipCommand;
    }
}

ClipCommand *SamplerSynthVoice::currentCommand() const
{
    return d->clipCommand;
}

void SamplerSynthVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    if (auto* sound = dynamic_cast<const SamplerSynthSound*> (s))
    {
        d->pitchRatio = std::pow (2.0, (midiNoteNumber - sound->rootMidiNote()) / 12.0)
                        * sound->sourceSampleRate() / getSampleRate();

        d->startTick = d->syncTimer->cumulativeBeat();
        d->maxSampleDeviation = d->syncTimer->subbeatCountToSeconds(d->syncTimer->getBpm(), 1) * sound->sourceSampleRate();
        d->clip = sound->clip();
        d->sourceSampleLength = d->clip->getDuration() * sound->sourceSampleRate();
        d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * sound->sourceSampleRate());

        // Asynchronously request the creation of a new position ID - if we call directly (or blocking
        // queued), we may end up in deadlocky threading trouble, so... asynchronous api it is!
        ClipAudioSourcePositionsModel *positionsModel = d->clip->playbackPositionsModel();
        connect(d->clip->playbackPositionsModel(), &ClipAudioSourcePositionsModel::positionIDCreated, this, [this, positionsModel](void* createdFor, qint64 newPositionID){
            if (createdFor == this) {
                if (d->clip && d->clip->playbackPositionsModel() == positionsModel) {
                    if (d->clipPositionId > -1) {
                        QMetaObject::invokeMethod(positionsModel, "removePosition", Qt::QueuedConnection, Q_ARG(qint64, d->clipPositionId));
                    }
                    d->clipPositionId = newPositionID;
                } else {
                    // If we're suddenly playing something else, we didn't receive this quickly enough and should just get rid of it
                    QMetaObject::invokeMethod(positionsModel, "removePosition", Qt::QueuedConnection, Q_ARG(qint64, newPositionID));
                }
                positionsModel->disconnect(this);
            }
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(d->clip->playbackPositionsModel(), "requestPositionID", Qt::QueuedConnection, Q_ARG(void*, this), Q_ARG(float, d->sourceSamplePosition / d->sourceSampleLength));

        d->lgain = velocity;
        d->rgain = velocity;

        d->adsr.setSampleRate (sound->sourceSampleRate());
        d->adsr.setParameters (sound->params());

        d->adsr.noteOn();
    }
    else
    {
        jassertfalse; // this object can only play SamplerSynthSounds!
    }
}

void SamplerSynthVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        d->adsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        d->adsr.reset();
        QMetaObject::invokeMethod(d->clip->playbackPositionsModel(), "removePosition", Qt::QueuedConnection, Q_ARG(qint64, d->clipPositionId));
        d->clipPositionId = -1;
        d->clip = nullptr;
        d->clipCommandsForDeleting << d->clipCommand;
        d->clipCommand = nullptr;
    }
}

void SamplerSynthVoice::pitchWheelMoved (int /*newValue*/) {}
void SamplerSynthVoice::controllerMoved (int /*controllerNumber*/, int /*newValue*/) {}

void SamplerSynthVoice::renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (auto* playingSound = static_cast<SamplerSynthSound*> (getCurrentlyPlayingSound().get()))
    {
        if (playingSound->isValid()) {
            auto& data = *playingSound->audioData();
            const float* const inL = data.getReadPointer (0);
            const float* const inR = data.getNumChannels() > 1 ? data.getReadPointer (1) : nullptr;

            float* outL = outputBuffer.getWritePointer (0, startSample);
            float* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1, startSample) : nullptr;
            float peakGain{0.0f};

            const int stopPosition = playingSound->stopPosition(d->clipCommand->slice);
            const int sampleDuration = playingSound->length();

            // beat align samples by reading clip duration in beats from clip, saving curentbeat in voice on startNote, and adjust in if (looping) section of process, and make sure the loop is restarted on that beat if deviation is sufficiently large (like... one timer tick is too much maybe?)
            // If the clip is a looping type (otherwise we don't really care enough)
            const double localSampleDeviationAllowance{qMax(d->maxSampleDeviation, (numSamples * getSampleRate() / playingSound->sourceSampleRate()))};
            if (d->clipCommand->looping
                // and the clip is actually a clean multiple of a number of beats
                && trunc(d->clip->getLengthInBeats()) == d->clip->getLengthInBeats()
                // and we are currently at some multiple of that beat duration in the playback loop
                && (quint64(d->syncTimer->cumulativeBeat() - d->startTick) % quint64(d->clip->getLengthInBeats() * d->syncTimer->getMultiplier())) == 0
                // and we are at a higher deviation from the start point than we accept
                && ((d->sourceSamplePosition - (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate())) > localSampleDeviationAllowance)
                // and also at a higher deviation from the end point...
                && (abs(d->sourceSamplePosition - stopPosition) > localSampleDeviationAllowance))
            {
                qDebug() << "Resetting playback for" << d->clip->getFilePath() << "due to not matching what we think the position should be, with start point deviation at" << (d->sourceSamplePosition - (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate())) << "and end point deviation" << abs(d->sourceSamplePosition - stopPosition) << "of an accepted" << localSampleDeviationAllowance;
                // TODO Switch start position for the loop position here
                d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate());
            }
            while (--numSamples >= 0)
            {
                auto pos = (int) d->sourceSamplePosition;
                auto alpha = (float) (d->sourceSamplePosition - pos);
                auto invAlpha = 1.0f - alpha;

                // just using a very simple linear interpolation here..
                float l = sampleDuration > pos ? (inL[pos] * invAlpha + inL[pos + 1] * alpha) : 0;
                float r = (inR != nullptr && sampleDuration > pos) ? (inR[pos] * invAlpha + inR[pos + 1] * alpha)
                                                                   : l;

                auto envelopeValue = d->adsr.getNextSample();

                l *= d->lgain * envelopeValue;
                r *= d->rgain * envelopeValue;

                if (outR != nullptr)
                {
                    *outL++ += l;
                    *outR++ += r;
                }
                else
                {
                    *outL++ += (l + r) * 0.5f;
                }
                peakGain = qMax(peakGain, (l + r) * 0.5f);

                d->sourceSamplePosition += d->pitchRatio;

                if (d->clipCommand->looping) {
                    if (d->sourceSamplePosition > stopPosition)
                    {
                        // TODO Switch start position for the loop position here
                        d->sourceSamplePosition = (int) (d->clip->getStartPosition(d->clipCommand->slice) * playingSound->sourceSampleRate());
                    }
                } else {
                    if (d->sourceSamplePosition > stopPosition)
                    {
                        stopNote (0.0f, false);
                        break;
                    } else if (d->sourceSamplePosition > (stopPosition - (d->adsr.getParameters().release * playingSound->sourceSampleRate()))) {
                        // ...really need a way of telling that this has been done already (it's not dangerous, just not pretty, there's maths in there)
                        stopNote (0.0f, true);
                    }
                }
                if (!d->adsr.isActive()) {
                    stopNote(0.0f, false);
                    break;
                }
            }
            // Because it might have gone away after being stopped above, so let's try and not crash
            if (d->clip && d->clipPositionId > -1) {
                QMetaObject::invokeMethod(d->clip->playbackPositionsModel(), "setPositionProgress", Qt::QueuedConnection, Q_ARG(qint64, d->clipPositionId), Q_ARG(float, d->sourceSamplePosition / d->sourceSampleLength));
                QMetaObject::invokeMethod(d->clip->playbackPositionsModel(), "setPositionGain", Qt::QueuedConnection, Q_ARG(qint64, d->clipPositionId), Q_ARG(float, peakGain));
            }
        }
    }
    if (!d->clipCommandsForDeleting.isEmpty()) {
        qDeleteAll(d->clipCommandsForDeleting);
        d->clipCommandsForDeleting.clear();
    }
}
