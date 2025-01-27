#pragma once

#include <QObject>

struct ClipCommand;
class ClipAudioSourcePositionsModelEntry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int id READ id NOTIFY dataChanged)
    Q_PROPERTY(float progress READ progress NOTIFY dataChanged)
    Q_PROPERTY(float gain READ gain NOTIFY dataChanged)
    Q_PROPERTY(float gainLeft READ gainLeft NOTIFY dataChanged)
    Q_PROPERTY(float gainRight READ gainRight NOTIFY dataChanged)
    Q_PROPERTY(float pan READ pan NOTIFY dataChanged)
public:
    explicit ClipAudioSourcePositionsModelEntry(QObject *parent = nullptr);
    ~ClipAudioSourcePositionsModelEntry() override = default;

    void clear();
    // emitting data changed is disable-able because otherwise we emit it twice when clearing
    void updateData(const int &id, const int &playheadId, const float &progress, const float &gainLeft, const float &gainRight, const float &pan, const bool &emitDataChanged = true);

    int id() const;
    int playheadId() const;
    float progress() const;
    float gain() const;
    float gainLeft() const;
    float gainRight() const;
    float pan() const;
    Q_SIGNAL void dataChanged();
private:
    friend class ClipAudioSourcePositionsModel;
    qint64 m_id{-1};
    ClipCommand *m_clipCommand{nullptr};
    int m_playheadId{0};
    float m_progress{0.0f};
    float m_gain{0.0f};
    float m_gainLeft{0.0f};
    float m_gainRight{0.0f};
    float m_pan{0.0f};
    qint64 m_keepUntil{0};
};
