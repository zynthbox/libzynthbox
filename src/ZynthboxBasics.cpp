#include <QtGlobal>
#include <QMetaEnum>
#include <QMap>
#include <QDebug>
#include "ZynthboxBasics.h"

class ZynthboxBasics::Private
{
public:
    Private() {
        QMetaEnum kitVersionMetaEnum = QMetaEnum::fromType<ZynthboxBasics::KitVersion>();
        int value = kitVersionMetaEnum.keyToValue(std::getenv("ZYNTHIAN_KIT_VERSION"));
        if (value != -1) {
            kitVersion = static_cast<ZynthboxBasics::KitVersion>(value);
        }
    }

    ZynthboxBasics::KitVersion kitVersion{ZynthboxBasics::KitVersion::KitCustom};
    QMap<ZynthboxBasics::Button, int> buttonIdMapZ2V4 {
        { ZynthboxBasics::ButtonMenu, 0 },
        { ZynthboxBasics::ButtonNum1, 1 },
        { ZynthboxBasics::ButtonNum2, 2 },
        { ZynthboxBasics::ButtonNum3, 3 },
        { ZynthboxBasics::ButtonNum4, 4 },
        { ZynthboxBasics::ButtonNum5, 5 },
        { ZynthboxBasics::ButtonStar, 6 },
        { ZynthboxBasics::ButtonMode, 7 },
        { ZynthboxBasics::ButtonStep1, 8 },
        { ZynthboxBasics::ButtonStep2, 9 },
        { ZynthboxBasics::ButtonStep3, 10 },
        { ZynthboxBasics::ButtonStep4, 11 },
        { ZynthboxBasics::ButtonStep5, 12 },
        { ZynthboxBasics::ButtonStep6, -1 },
        { ZynthboxBasics::ButtonStep7, -1 },
        { ZynthboxBasics::ButtonStep8, -1 },
        { ZynthboxBasics::ButtonStep9, -1 },
        { ZynthboxBasics::ButtonStep10, -1 },
        { ZynthboxBasics::ButtonStep11, -1 },
        { ZynthboxBasics::ButtonStep12, -1 },
        { ZynthboxBasics::ButtonStep13, -1 },
        { ZynthboxBasics::ButtonStep14, -1 },
        { ZynthboxBasics::ButtonStep15, -1 },
        { ZynthboxBasics::ButtonStep16, -1 },
        { ZynthboxBasics::ButtonAlt, 13 },
        { ZynthboxBasics::ButtonRecord, 14 },
        { ZynthboxBasics::ButtonPlay, 15 },
        { ZynthboxBasics::ButtonMetronome, 16 },
        { ZynthboxBasics::ButtonStop, 17 },
        { ZynthboxBasics::ButtonBack, 18 },
        { ZynthboxBasics::ButtonUp, 19 },
        { ZynthboxBasics::ButtonSelect, 20 },
        { ZynthboxBasics::ButtonLeft, 21 },
        { ZynthboxBasics::ButtonDown, 22 },
        { ZynthboxBasics::ButtonRight, 23 },
        { ZynthboxBasics::ButtonGlobal, 24 }
    };
    QMap<ZynthboxBasics::Button, int> buttonIdMapZ2V5 {
        { ZynthboxBasics::ButtonMenu, 0 },
        { ZynthboxBasics::ButtonNum1, 1 },
        { ZynthboxBasics::ButtonNum2, 2 },
        { ZynthboxBasics::ButtonNum3, 3 },
        { ZynthboxBasics::ButtonNum4, 4 },
        { ZynthboxBasics::ButtonNum5, 5 },
        { ZynthboxBasics::ButtonStar, 6 },
        { ZynthboxBasics::ButtonMode, 7 },
        { ZynthboxBasics::ButtonStep1, 8 },
        { ZynthboxBasics::ButtonStep2, 9 },
        { ZynthboxBasics::ButtonStep3, 10 },
        { ZynthboxBasics::ButtonStep4, 11 },
        { ZynthboxBasics::ButtonStep5, 12 },
        { ZynthboxBasics::ButtonStep6, 13 },
        { ZynthboxBasics::ButtonStep7, 14 },
        { ZynthboxBasics::ButtonStep8, 15 },
        { ZynthboxBasics::ButtonStep9, 16 },
        { ZynthboxBasics::ButtonStep10, 17 },
        { ZynthboxBasics::ButtonStep11, 18 },
        { ZynthboxBasics::ButtonStep12, 19 },
        { ZynthboxBasics::ButtonStep13, 20 },
        { ZynthboxBasics::ButtonStep14, 21 },
        { ZynthboxBasics::ButtonStep15, 22 },
        { ZynthboxBasics::ButtonStep16, 23 },
        { ZynthboxBasics::ButtonAlt, 24 },
        { ZynthboxBasics::ButtonRecord, 25 },
        { ZynthboxBasics::ButtonPlay, 26 },
        { ZynthboxBasics::ButtonMetronome, 27 },
        { ZynthboxBasics::ButtonStop, 28 },
        { ZynthboxBasics::ButtonBack, 29 },
        { ZynthboxBasics::ButtonUp, 30 },
        { ZynthboxBasics::ButtonSelect, 31 },
        { ZynthboxBasics::ButtonLeft, 32 },
        { ZynthboxBasics::ButtonDown, 33 },
        { ZynthboxBasics::ButtonRight, 34 },
        { ZynthboxBasics::ButtonGlobal, 35 }
    };
};

ZynthboxBasics::ZynthboxBasics(QObject* parent)
    : QObject(parent)
    , d(new Private())
{ }

ZynthboxBasics::~ZynthboxBasics()
{
    delete d;
}

QString ZynthboxBasics::trackLabelText(const Track& track) const
{
    switch(track) {
        case NoTrack:
            return QLatin1String{"No Track"};
        case AnyTrack:
            return QLatin1String{"Any Track"};
        case CurrentTrack:
            return QLatin1String{"Current Track"};
        case Track1:
            return QLatin1String{"Track 1"};
        case Track2:
            return QLatin1String{"Track 2"};
        case Track3:
            return QLatin1String{"Track 3"};
        case Track4:
            return QLatin1String{"Track 4"};
        case Track5:
            return QLatin1String{"Track 5"};
        case Track6:
            return QLatin1String{"Track 6"};
        case Track7:
            return QLatin1String{"Track 7"};
        case Track8:
            return QLatin1String{"Track 8"};
        case Track9:
            return QLatin1String{"Track 9"};
        case Track10:
            return QLatin1String{"Track 10"};
    };
    return QLatin1String{"Unknown Track"};
}

QString ZynthboxBasics::slotLabelText(const Slot& slot) const
{
    switch(slot) {
        case NoSlot:
            return QLatin1String{"No Slot"};
        case AnySlot:
            return QLatin1String{"Any Slot"};
        case CurrentSlot:
            return QLatin1String{"Current Slot"};
        case Slot1:
            return QLatin1String{"Slot 1"};
        case Slot2:
            return QLatin1String{"Slot 2"};
        case Slot3:
            return QLatin1String{"Slot 3"};
        case Slot4:
            return QLatin1String{"Slot 4"};
        case Slot5:
            return QLatin1String{"Slot 5"};
    }
    return QLatin1String{"Unknown Slot"};
}

QString ZynthboxBasics::clipLabelText(const Slot& slot) const
{
    switch(slot) {
        case NoSlot:
            return QLatin1String{"No Clip"};
        case AnySlot:
            return QLatin1String{"Any Clip"};
        case CurrentSlot:
            return QLatin1String{"Current Clip"};
        case Slot1:
            return QLatin1String{"Clip 1"};
        case Slot2:
            return QLatin1String{"Clip 2"};
        case Slot3:
            return QLatin1String{"Clip 3"};
        case Slot4:
            return QLatin1String{"Clip 4"};
        case Slot5:
            return QLatin1String{"Clip 5"};
    }
    return QLatin1String{"Unknown Clip"};
}

QString ZynthboxBasics::soundSlotLabelText(const Slot& slot) const
{
    switch(slot) {
        case NoSlot:
            return QLatin1String{"No Sound Slot"};
        case AnySlot:
            return QLatin1String{"Any Sound Slot"};
        case CurrentSlot:
            return QLatin1String{"Current Sound Slot"};
        case Slot1:
            return QLatin1String{"Sound Slot 1"};
        case Slot2:
            return QLatin1String{"Sound Slot 2"};
        case Slot3:
            return QLatin1String{"Sound Slot 3"};
        case Slot4:
            return QLatin1String{"Sound Slot 4"};
        case Slot5:
            return QLatin1String{"Sound Slot 5"};
    }
    return QLatin1String{"Unknown Sound Slot"};
}

QString ZynthboxBasics::fxLabelText(const Slot& slot) const
{
    switch(slot) {
        case NoSlot:
            return QLatin1String{"No FX Slot"};
        case AnySlot:
            return QLatin1String{"Any FX Slot"};
        case CurrentSlot:
            return QLatin1String{"Current FX Slot"};
        case Slot1:
            return QLatin1String{"FX Slot 1"};
        case Slot2:
            return QLatin1String{"FX Slot 2"};
        case Slot3:
            return QLatin1String{"FX Slot 3"};
        case Slot4:
            return QLatin1String{"FX Slot 4"};
        case Slot5:
            return QLatin1String{"FX Slot 5"};
    }
    return QLatin1String{"Unknown FX Slot"};
}

int ZynthboxBasics::buttonId(const Button &button) const
{
    int returnVal = -1;

    switch (d->kitVersion) {
        case KitVersion::KitZ2V4:
            returnVal = d->buttonIdMapZ2V4.value(button, -1);
            break;
        case KitVersion::KitZ2V5:
        case KitVersion::KitZ2V5B:
            returnVal = d->buttonIdMapZ2V5.value(button, -1);
            break;
        default:
            returnVal = -1;
            break;
    }

    return returnVal;
}
