/*
  ==============================================================================

    AudioLevels.cpp
    Created: 8 Feb 2022
    Author:  Anupam Basak <anupam.basak27@gmail.com>

  ==============================================================================
*/

#pragma once

#include "AudioLevelsChannel.h"
#include "JUCEHeaders.h"

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QCoreApplication>
#include <jack/jack.h>
#include <atomic>
#include <mutex>

struct TimerCommand;
class AudioLevelsPrivate;
/**
 * @brief The AudioLevels class provides a way to read audio levels of different ports
 *
 * This class exposes some Q_PROPERTY which reports respective audio levels in decibel
 * It also provides a helper method to add multiple decibel values
 *
 * To use this class in qml import libzl and use read the properties as follows :
 * <code>
 * import libzl 1.0 as ZL
 * </code>
 *
 * <code>
 * console.log(ZL.AudioLevels.synthA)
 * </code>
 */
class AudioLevels : public QObject {
Q_OBJECT
    /**
     * \brief Left Capture channel audio level in decibels
     */
    Q_PROPERTY(float captureA MEMBER captureA NOTIFY audioLevelsChanged)
    /**
     * \brief Right Capture channel audio level in decibels
     */
    Q_PROPERTY(float captureB MEMBER captureB NOTIFY audioLevelsChanged)

    /**
     * \brief Left system playback channel audio level in decibels
     */
    Q_PROPERTY(float playbackA MEMBER playbackA NOTIFY audioLevelsChanged)
    /**
     * \brief Right system playback channel audio level in decibels
     */
    Q_PROPERTY(float playbackB MEMBER playbackB NOTIFY audioLevelsChanged)
    /**
     * \brief Left system playback channel hold value (the slow-fade peak)
     */
    Q_PROPERTY(float playbackAHold MEMBER playbackAHold NOTIFY audioLevelsChanged)
    /**
     * \brief Right system playback channel hold value (the slow-fade peak)
     */
    Q_PROPERTY(float playbackBHold MEMBER playbackBHold NOTIFY audioLevelsChanged)
    /**
     * \brief Combined playback channel audio level in decibels
     */
    Q_PROPERTY(float playback MEMBER playback NOTIFY audioLevelsChanged)

    /**
     * \brief Left recording channel audio level in decibels
     */
    Q_PROPERTY(float recordingA MEMBER recordingA NOTIFY audioLevelsChanged)
    /**
     * \brief Right recording channel audio level in decibels
     */
    Q_PROPERTY(float recordingB MEMBER recordingB NOTIFY audioLevelsChanged)

    /**
     * \brief A list of AudioLevelsChannel objects for each of the Sketchpad tracks
     */
    Q_PROPERTY(QVariantList tracks READ tracks CONSTANT)
    /**
     * \brief Channels audio level in decibels as an array of 10 elements
     */
    Q_PROPERTY(QVariantList channels READ getChannelsAudioLevels NOTIFY audioLevelsChanged)

    /**
     * \brief Set whether or not to record the global playback when calling startRecording
     */
    Q_PROPERTY(bool recordGlobalPlayback READ recordGlobalPlayback WRITE setRecordGlobalPlayback NOTIFY recordGlobalPlaybackChanged)
    /**
     * \brief A list of the channel indices of the channels marked to be included when recording
     */
    Q_PROPERTY(QVariantList channelsToRecord READ channelsToRecord NOTIFY channelsToRecordChanged)
    /**
     * \brief Set whether or not to record the explicitly toggled ports
     * @see addRecordPort(QString, int)
     * @see removeRecordPort(QString, int)
     * @see clearRecordPorts()
     */
    Q_PROPERTY(bool shouldRecordPorts READ shouldRecordPorts WRITE setShouldRecordPorts NOTIFY shouldRecordPortsChanged)
    /**
     * \brief Whether or not we are currently performing any recording operations
     */
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
public:
    static AudioLevels* instance();

    // Delete the methods we dont want to avoid having copies of the singleton class
    AudioLevels(AudioLevels const&) = delete;
    void operator=(AudioLevels const&) = delete;

    /**
     * \brief Add two decibel values
     * @param db1 Audio level in decibels
     * @param db2 Audio level in decibels
     * @return db1+db2
     */
    Q_INVOKABLE float add(float db1, float db2);

    QVariantList tracks() const;

    Q_INVOKABLE void setRecordGlobalPlayback(bool shouldRecord = true);
    Q_INVOKABLE bool recordGlobalPlayback() const;
    /**
     * \brief Set the first part of the filename used when recording the global playback
     * This should be the full first part of the filename, path and all. The recorder will then append
     * a timestamp and the file suffix (.wav). You should also ensure that the path exists before calling
     * @note If you pass in something that ends in .wav, the prefix will be used verbatim and no details added
     * @param fileNamePrefix The prefix you wish to use as the basis for the global playback recording's filenames
     */
    Q_INVOKABLE void setGlobalPlaybackFilenamePrefix(const QString& fileNamePrefix);
    /**
     * \brief Set the last part of the filename used when recording
     * @note This is reset to ".wav" whenever the recording ports are cleared or recording is stopped
     * @param fileNamePrefix The suffix you wish to use at the end of the global channel's filenames
     */
    Q_INVOKABLE void setGlobalPlaybackFilenameSuffix(const QString& fileNameSuffix);

    /**
     * \brief Sets whether or not a channel should be included when recording
     * @param channel The index of the channel you wish to change the recording status of
     * @param shouldRecord Whether or not the channel should be recorded
     */
    Q_INVOKABLE void setChannelToRecord(int channel, bool shouldRecord = true);
    /**
     * \brief Returns a list of channel indices for channels marked to be recorded
     * @see setChannelToRecord(int, bool)
     */
    Q_INVOKABLE QVariantList channelsToRecord() const;
    /**
     * \brief Set the first part of the filename used when recording
     * This should be the full first part of the filename, path and all. The recorder will then append
     * a timestamp and the file suffix (.wav). You should also ensure that the path exists before calling
     * startRecording.
     * @param channel The index of the channel you wish to change the filename prefix for
     * @param fileNamePrefix The prefix you wish to use as the basis of the given channel's filenames
     */
    Q_INVOKABLE void setChannelFilenamePrefix(int channel, const QString& fileNamePrefix);
    /**
     * \brief Set the last part of the filename used when recording
     * @note This is reset to ".wav" whenever the recording ports are cleared or recording is stopped
     * @param channel The index of the channel you wish to change the filename suffix for
     * @param fileNamePrefix The suffix you wish to use at the end of the given channel's filenames
     */
    Q_INVOKABLE void setChannelFilenameSuffix(int channel, const QString& fileNameSuffix);

    /**
     * \brief Set the first part of the filename used when recording
     * This should be the full first part of the filename, path and all. The recorder will then append
     * a timestamp and the file suffix (.wav). You should also ensure that the path exists before calling
     * startRecording.
     * @note If you pass in something that ends in .wav, the prefix will be used verbatim and no details added
     * @param fileNamePrefix The prefix you wish to use as the basis of the port recorder's filenames
     */
    Q_INVOKABLE void setRecordPortsFilenamePrefix(const QString& fileNamePrefix);
    /**
     * \brief Set the last part of the filename used when recording
     * @note This is reset to ".wav" whenever the recording ports are cleared or recording is stopped
     * @param fileNamePrefix The suffix you wish to use at the end of the port recorder's filenames
     */
    Q_INVOKABLE void setRecordPortsFilenameSuffix(const QString& fileNameSuffix);
    /**
     * \brief Adds a port to the list of ports to be recorded
     * @param portName The audio type jack port to record
     * @param channel The logical channel (0 is left, 1 is right)
     */
    Q_INVOKABLE void addRecordPort(const QString &portName, int channel);
    /**
     * \brief Removes a port from the list of ports to be recorded
     * @param portName The audio type jack port to stop recording
     * @param channel The logical channel (0 is left, 1 is right)
     */
    Q_INVOKABLE void removeRecordPort(const QString &portName, int channel);
    /**
     * \brief Clear the list of ports to be recorded
     */
    Q_INVOKABLE void clearRecordPorts();
    Q_INVOKABLE void setShouldRecordPorts(bool shouldRecord);
    Q_INVOKABLE bool shouldRecordPorts() const;

    /**
     * \brief Returns a timestamped filename for the given prefix
     * @param prefix The filename prefix to be timestamped
     * @param suffix The file suffix for the timestamped file
     * @return The full filename that for the given prefix and suffix
     */
    Q_INVOKABLE QString getTimestampedFilename(const QString &prefix, const QString &suffix);

    /**
     * \brief Start the recording process on all enabled channels
     *
     * The logical progression of doing semi-automated multi-channeled recording is:
     * - Mark all the channels that need including for recording and those that shouldn't be (setChannelToRecord and setRecordGlobalPlayback)
     * - Set the filename prefixes for all the channels that will be included (you can also set the others, it has no negative side effects)
     * - Start the recording
     * - Start playback after the recording, to ensure everything is included
     * - Stop recording when needed
     * - Stop playback
     * @param startTimestamp If set, this will be used in place of the current jack playhead as the start time for recordings
     */
    Q_INVOKABLE void startRecording(quint64 startTimestamp = 0);
    /**
     * \brief Schedules a start of the recording process on all enabled channels
     * @param delay The amount of time to wait until starting the recording (if you need to do it now, just call startRecording)
     * @see startRecording()
     */
    Q_INVOKABLE void scheduleStartRecording(quint64 delay);
    /**
     * \brief Schedules a start of the recording process for the given sketchpad track, with the given filename prefix
     * @param delay The amount of time to wait until starting the recording
     * @param sketchpadTrack Which sketchpad track to start recording (0 through 9 inclusive, invalid numbers will cause the command to be ignored)
     * @param prefix The filename prefix for the recording which is asking to be started
     * @param suffix The file suffix for the recording which is being asked to be started
     * @return The full filename that will be used for the recording (timestamp will be scheduling time, not recording start time)
     */
    Q_INVOKABLE QString scheduleChannelRecorderStart(quint64 delay, int sketchpadTrack, const QString &prefix, const QString &suffix = QString{".wav"});
    /**
     * \brief Stop any ongoing recordings
     * @param stopTimestamp If set, this will be used in place of the current jack playhead as the stop time for recordings
     */
    Q_INVOKABLE void stopRecording(quint64 stopTimestamp = 0);
    /**
     * \brief Schedules a stop of all recording processes ongoing at the time the event is fired
     * @param delay The amount of time to wait until stopping recording (if you need to do it now, just call stopRecording)
     * @see stopRecording()
     */
    Q_INVOKABLE void scheduleStopRecording(quint64 delay);
    /**
     * \brief Schedules the recording to stop on the given sketchpad track
     * @param delay The amount of time to wait until stopping the recording
     * @param sketchpadTrack The sketchpad track on which to stop recording (0 through 9 inclusive, invalid numbers will cause the command to be ignored)
     */
    Q_INVOKABLE void scheduleChannelRecorderStop(quint64 delay, int sketchpadTrack);

    /**
     * \brief Handle the given timer command
     * @param timestamp The jack playhead time that the operation should actually happen at
     * @param command The timer command that requires handling
     */
    void handleTimerCommand(quint64 timestamp, TimerCommand *command);

    /**
     * \brief Returns a list of filenames for all the recordings (index 0 is global, 1 is the ports recording, 2 through 11 are sketchpad tracks 0 through 9)
     * If a recorder was unused, its position will exist in the list but contain an empty string
     * @note This will be cleared the next time startRecording is called
     * @return The list of filenames used for the most recently started recording session
     */
    Q_INVOKABLE QStringList recordingFilenames() const;

    /**
     * @brief Check if a recording is in progress
     * @return Whether a recording is currently in progress
     */
    Q_INVOKABLE bool isRecording() const;

    /**
     * \brief Get the AudioLevelsChannel instance for the given sketchpad track
     * @param sketchpadTrack The sketchpad track to get the AudioLevelsChannel for (0 through 9)
     * @return The AudioLevelsChannel instance for the given sketchpad track, or a nullptr for an invalid track
     */
    AudioLevelsChannel *audioLevelsChannel(const int &sketchpadTrack) const;
    /**
     * \brief Get the AudioLevelsChannel instance for the system capture recorder
     * @return The AudioLevelsChannel instance for the system capture recorder
     */
    AudioLevelsChannel *systemCaptureAudioLevelsChannel() const;
    /**
     * \brief Get the AudioLevelsChannel instance for the global output
     * @return The AudioLevelsChannel instance for the global output
     */
    AudioLevelsChannel *globalAudioLevelsChannel() const;
    /**
     * \brief Get the AudioLevelsChannel instance for the ports recorder
     * @return The AudioLevelsChannel instance for the ports recorder
     */
    AudioLevelsChannel *portsRecorderAudioLevelsChannel() const;

    juce::AudioFormatManager m_formatManager;
    juce::AudioThumbnailCache m_thumbnailsCache{100};
Q_SIGNALS:
    void audioLevelsChanged();
    void recordGlobalPlaybackChanged();
    void channelsToRecordChanged();
    void shouldRecordPortsChanged();
    void isRecordingChanged();

private:
    explicit AudioLevels(QObject *parent = nullptr);

    const QVariantList getChannelsAudioLevels();

    float convertTodbFS(float raw);

    float captureA{-200.0f}, captureB{-200.0f};
    float playbackA{-200.0f}, playbackB{-200.0f}, playbackAHold{-200.0f}, playbackBHold{-200.0f}, playback{-200.0f};
    float recordingA{-200.0f}, recordingB{-200.0f};
    float channelsA[CHANNELS_COUNT] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
          channelsB[CHANNELS_COUNT] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    Q_SLOT void timerCallback();
    AudioLevelsPrivate *d{nullptr};

    static std::atomic<AudioLevels*> singletonInstance;
    static std::mutex singletonMutex;
};
Q_DECLARE_METATYPE(AudioLevels*)
