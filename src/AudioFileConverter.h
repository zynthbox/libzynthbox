#pragma once

#include "JUCEHeaders.h"

#include <QObject>
#include <QCoreApplication>

class AudioFileConverterPrivate;
/**
 * \brief A class used to convert files between audio formats
 */
class AudioFileConverter : public QObject {
    Q_OBJECT
    /**
     * \brief Functions to give information on the progress of the most recent conversion request
     * @note When the value is -1.0, consider us "spinning", that is, having an unknown progress level
     * @min -1.0
     * @max 1.0
     * @default 0.0
     */
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString taskDescription READ taskDescription NOTIFY taskDescriptionChanged)

    Q_PROPERTY(QString origin READ origin NOTIFY originChanged)
    Q_PROPERTY(QString destination READ destination NOTIFY destinationChanged)
    Q_PROPERTY(AudioFormat destinationFormat READ destinationFormat NOTIFY destinationFormatChanged)

    Q_PROPERTY(ConversionState state READ state NOTIFY stateChanged)
public:
    static AudioFileConverter* instance() {
        static AudioFileConverter* instance{nullptr};
        if (!instance) {
            instance = new AudioFileConverter(qApp);
        }
        return instance;
    }
    explicit AudioFileConverter(QObject *parent = nullptr);
    ~AudioFileConverter() override;

    enum AudioFormat {
        ///@< A high-quality ogg/vorbis format
        OggVorbisAudioFormat,
        ///@< A low-quality ogg/vorbis format
        OggVorbisLowQualityAudioFormat,
        ///@< A raw wave format file
        WaveAudioFormat,
    };
    Q_ENUM(AudioFormat)
    /**
     * \brief If we are not currently active, this will begin a conversion from the origin file's format to the destination and block until the process completes
     * During the conversion process, we will create an entirely new file, and also copy any tags which exist in the origin file to the new one
     * @note The function itself will block, but will yield to the event loop on a regular basis during the process
     * @param origin The full file path for the file that you want to convert to a new format
     * @param destination The full file path for where you want the converted file to live (this must not exist, and we will create the path if it is missing)
     * @param destinationFormat The audio format of the destination file
     * @return Whether the conversion was started successfully. We will fail immediate if one is already ongoing, and for all other information, check state()
     */
    Q_INVOKABLE bool convert(const QString &origin, const QString &destination, const AudioFormat &destinationFormat = OggVorbisAudioFormat);

    /**
     * \brief If there is an ongoing conversion, calling this will abort it
     * @param deleteLeftovers If true, aborting will remove any data that had already been created by the conversion process (note, created directories will *not* be removed)
     * @return True if the ongoing process was aborted, false if there was nothing to abort
     */
    Q_INVOKABLE bool abort(bool deleteLeftovers = true);

    double progress() const;
    Q_SIGNAL void progressChanged();
    QString taskDescription() const;
    Q_SIGNAL void taskDescriptionChanged();
    QString origin() const;
    Q_SIGNAL void originChanged();
    QString destination() const;
    Q_SIGNAL void destinationChanged();
    AudioFormat destinationFormat() const;
    Q_SIGNAL void destinationFormatChanged();

    enum ConversionState {
        ///@< Initial state (only used before the first call to convert)
        IdleState,
        ///@< There is an ongoing conversion (see progress and taskDescription for how far along we are)
        ConvertingState,
        ///@< The most recent conversion completed successfully
        SuccessState,
        ///@< The most recent conversion failed (there will be a description of why in taskDescription)
        FailureState,
        ///@< The most recent conversion was aborted before completion
        AbortedState,
    };
    Q_ENUM(ConversionState)
    ConversionState state() const;
    Q_SIGNAL void stateChanged();
private:
    AudioFileConverterPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(AudioFileConverter::AudioFormat)
Q_DECLARE_METATYPE(AudioFileConverter::ConversionState)
