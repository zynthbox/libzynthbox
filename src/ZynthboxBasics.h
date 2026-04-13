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

    enum TrackRackType {
        SynthRackType,
        SampleRackType,
    };
    Q_ENUM(TrackRackType)

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

    enum Button {
        ButtonInvalid = -1,
        ButtonMenu = 0,
        ButtonNum1,
        ButtonNum2,
        ButtonNum3,
        ButtonNum4,
        ButtonNum5,
        ButtonStar,
        ButtonMode,
        ButtonStep1,
        ButtonStep2,
        ButtonStep3,
        ButtonStep4,
        ButtonStep5,
        ButtonStep6,
        ButtonStep7,
        ButtonStep8,
        ButtonStep9,
        ButtonStep10,
        ButtonStep11,
        ButtonStep12,
        ButtonStep13,
        ButtonStep14,
        ButtonStep15,
        ButtonStep16,
        ButtonAlt,
        ButtonRecord,
        ButtonPlay,
        ButtonMetronome,
        ButtonStop,
        ButtonBack,
        ButtonUp,
        ButtonSelect,
        ButtonLeft,
        ButtonDown,
        ButtonRight,
        ButtonGlobal,
    };
    Q_ENUM(Button)

    enum KitVersion {
        KitCustom = 0,
        KitZ2V4,
        KitZ2V5,
        KitZ2V5B
    };
    Q_ENUM(KitVersion)

    Q_INVOKABLE QString slotLabelText(const Slot &slot) const;
    Q_INVOKABLE QString clipLabelText(const Slot &slot) const;
    Q_INVOKABLE QString soundSlotLabelText(const Slot &slot) const;
    Q_INVOKABLE QString fxLabelText(const Slot &slot) const;

    /**
     * /brief Returns the button ID for the given button, or -1 if the button is not valid for the current kit version.
     * The button ID is determined based on the current kit version.
     * 
     * @param button The ZynthboxBasics::Button enum for which to retrieve the ID.
     * @return The button ID corresponding to the given button and current kit version, or -1 if the button is not valid.
     */
    Q_INVOKABLE int buttonId(const Button &button) const;

private:
    class Private;
    Private *d;
};
