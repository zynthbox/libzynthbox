#pragma once

#include <QObject>

class ClipAudioSourceSubvoiceSettingsPrivate;
class ClipAudioSourceSubvoiceSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float pan READ pan WRITE setPan NOTIFY panChanged)
    Q_PROPERTY(float pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(float gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(float gainAbsolute READ gainAbsolute WRITE setGainAbsolute NOTIFY gainChanged)
    Q_PROPERTY(float gainDb READ gainDb NOTIFY gainChanged)
public:
    explicit ClipAudioSourceSubvoiceSettings(QObject *parent = nullptr);
    ~ClipAudioSourceSubvoiceSettings() override;

    float pan() const;
    void setPan(const float &pan);
    Q_SIGNAL void panChanged();

    float pitch() const;
    float pitchChangePrecalc() const;
    void setPitch(const float &pitch);
    Q_SIGNAL void pitchChanged();

    float gain() const;
    float gainDb() const;
    float gainAbsolute() const;
    void setGain(const float &gain);
    void setGainDb(const float &gainDb);
    void setGainAbsolute(const float &gainAbsolute);
    Q_SIGNAL void gainChanged();
private:
    ClipAudioSourceSubvoiceSettingsPrivate *d{nullptr};
};
