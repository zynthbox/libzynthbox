#pragma once

#include <QObject>

struct ClipCommand;
class ClipAudioSourcePositionsModelEntry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int id READ id NOTIFY dataChanged)
    Q_PROPERTY(float progress READ progress NOTIFY dataChanged)
    Q_PROPERTY(float gain READ gain NOTIFY dataChanged)
    Q_PROPERTY(float pan READ pan NOTIFY dataChanged)
public:
    explicit ClipAudioSourcePositionsModelEntry(QObject *parent = nullptr);
    ~ClipAudioSourcePositionsModelEntry() override = default;

    void clear();
    void updateData(const int &id, const float &progress, const float &gain, const float &pan);

    int id() const;
    float progress() const;
    float gain() const;
    float pan() const;
    Q_SIGNAL void dataChanged();
private:
    friend class ClipAudioSourcePositionsModel;
    qint64 m_id{-1};
    ClipCommand *m_clipCommand{nullptr};
    float m_progress{0.0f};
    float m_gain{0.0f};
    float m_pan{0.0f};
    qint64 m_keepUntil{0};
};
