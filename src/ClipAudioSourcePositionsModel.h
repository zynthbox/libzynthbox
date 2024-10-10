#pragma once

#include <QAbstractListModel>
#include <memory>

#include "ClipAudioSource.h"

typedef uint32_t jack_nframes_t;
struct ClipCommand;
class ClipAudioSourcePositionsModelPrivate;
class ClipAudioSourcePositionsModel : public QAbstractListModel
{
    Q_OBJECT
    /**
     * \brief The highest gain among all positions in the model
     */
    Q_PROPERTY(float peakGain READ peakGain NOTIFY peakGainChanged)
    Q_PROPERTY(float peakGainLeft READ peakGainLeft NOTIFY peakGainChanged)
    Q_PROPERTY(float peakGainRight READ peakGainRight NOTIFY peakGainChanged)
    /**
     * \brief All of the position objects held by the model (it will contain ZynthboxClipMaximumPositionCount entries)
     */
    Q_PROPERTY(QVariantList positions READ positions CONSTANT)
public:
    explicit ClipAudioSourcePositionsModel(ClipAudioSource *clip);
    ~ClipAudioSourcePositionsModel() override;

    enum PositionRoles {
        PositionIDRole = Qt::UserRole + 1,
        PositionProgressRole,
        PositionGainRole,
        PositionGainLeftRole,
        PositionGainRightRole,
        PositionPanRole,
    };
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QVariantList positions() const;

    Q_INVOKABLE void setPositionData(const jack_nframes_t &timestamp, ClipCommand *clipCommand, const int &playheadIndex, const float &gainLeft, const float &gainRight, const float &progress, const float &pan);
    void setMostRecentPositionUpdate(jack_nframes_t timestamp);

    float peakGain();
    float peakGainLeft() const;
    float peakGainRight() const;
    Q_SIGNAL void peakGainChanged();

    /**
     * \brief The progress of the first active position (or -1 if there's no active position)
     * @return The progress (from 0 through 1) of the first active position (or -1 if there is no active position)
     */
    double firstProgress() const;

    Q_INVOKABLE void updatePositions();
private:
    std::unique_ptr<ClipAudioSourcePositionsModelPrivate> d;
};
