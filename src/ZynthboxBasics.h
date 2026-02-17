#pragma once

#include <QCoreApplication>

#define ZynthboxSongCount 1
#define ZynthboxTrackCount 10
#define ZynthboxSlotCount 5
#define ZynthboxSampleSlotRowCount 2
#define ZynthboxClipMaximumPositionCount 256

class ZynthboxBasics : public QObject
{
    Q_OBJECT
public:
    static ZynthboxBasics* instance() {
        static ZynthboxBasics* instance{nullptr};
        if (!instance) {
            instance = new ZynthboxBasics(qApp);
        }
        return instance;
    };
    explicit ZynthboxBasics(QObject *parent = nullptr);
    ~ZynthboxBasics() override;

    enum Track {
        NoTrack = -3,
        AnyTrack = -2,
        CurrentTrack = -1,
        Track1 = 0,
        Track2 = 1,
        Track3 = 2,
        Track4 = 3,
        Track5 = 4,
        Track6 = 5,
        Track7 = 6,
        Track8 = 7,
        Track9 = 8,
        Track10 = 9,
    };
    Q_ENUM(Track)

    Q_INVOKABLE QString trackLabelText(const Track &track) const;

    enum Slot {
        NoSlot = -3,
        AnySlot = -2,
        CurrentSlot = -1,
        Slot1 = 0,
        Slot2 = 1,
        Slot3 = 2,
        Slot4 = 3,
        Slot5 = 4,
    };
    Q_ENUM(Slot)

    Q_INVOKABLE QString slotLabelText(const Slot &slot) const;
    Q_INVOKABLE QString clipLabelText(const Slot &slot) const;
    Q_INVOKABLE QString soundSlotLabelText(const Slot &slot) const;
    Q_INVOKABLE QString fxLabelText(const Slot &slot) const;
};
