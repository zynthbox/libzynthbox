#pragma once

#include <QObject>

/**
 * \brief Handles the three-way synchronisation between three different gain representations (gain, decibels, and a the absolute position on the linear distance between the lower and upper dB range)
 *
 * Use this to manage the synchronisation of the three logical variations of gain values:
 * * gain, being the amount an audio signal would be multiplied by to get the expected audible result
 * * gainDb, being the amount of decibel change applied to an audio signal
 * * gainAbsolute, being the linear position along the minimum and maximum ranges the class exposes representing the gain held by the object (a conceptual "slider" value)
 */
class GainHandler : public QObject
{
    Q_OBJECT
    /**
     * \brief The lower limit for the gain decibel range (when reaching this value, gain will be clamped to 0)
     * @default -24
     */
    Q_PROPERTY(float minimumDecibel READ minimumDecibel WRITE setMinimumDecibel NOTIFY minimumDecibelChanged)
    /**
     * \brief The upper limit for the gain decibel range
     * @default 24
     */
    Q_PROPERTY(float maximumDecibel READ maximumDecibel WRITE setMaximumDecibel NOTIFY maximumDecibelChanged)

    /**
     * \brief The absolute gain value at which we have zero decibel
     */
    Q_PROPERTY(float absoluteGainAtZeroDb READ absoluteGainAtZeroDb NOTIFY gainChanged)

    /**
     * \brief The gain in absolute style (from 0.0 through 1.0, along the range set by minimumDecibel and maximumDecibel)
     */
    Q_PROPERTY(float gainAbsolute READ gainAbsolute WRITE setGainAbsolute NOTIFY gainChanged)
    /**
     * \brief The gain as a dB value
     */
    Q_PROPERTY(float gainDb READ gainDb WRITE setGainDb NOTIFY gainChanged)
    /**
     * \brief The gain as a raw gain value (that is, what should the audio signal be multiplied by)
     */
    Q_PROPERTY(float gain READ gain WRITE setGain NOTIFY gainChanged)

    /**
     * \brief Set this to true to mark this gain handler as muted
     * The direct result of setting this property will be that operationalGain() will return 0
     * @default false
     */
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
public:
    explicit GainHandler(QObject *parent = nullptr);
    ~GainHandler() override;

    float minimumDecibel() const;
    void setMinimumDecibel(const float &minimumDecibel);
    Q_SIGNAL void minimumDecibelChanged();
    float maximumDecibel() const;
    void setMaximumDecibel(const float &maximumDecibel);
    Q_SIGNAL void maximumDecibelChanged();

    float absoluteGainAtZeroDb() const;
    float gain() const;
    float gainDb() const;
    float gainAbsolute() const;
    void setGain(const float &gain);
    void setGainDb(const float &db);
    void setGainAbsolute(const float &gainAbsolute);
    Q_SIGNAL void gainChanged();

    bool muted() const;
    void setMuted(const bool &muted);
    Q_SIGNAL void mutedChanged();

    /**
     * \brief The gain value used for actually processing audio signals
     * @return If the handler is muted, this will return 0, otherwise it will return the same as calling gain()
     * @see muted()
     * @see gain()
     */
    const float &operationalGain() const;
    // Emitted whenever either gain or muted change
    Q_SIGNAL void operationalGainChanged();
private:
    class Private;
    Private *d{nullptr};
};
