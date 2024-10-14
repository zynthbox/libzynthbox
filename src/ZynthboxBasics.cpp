#include "ZynthboxBasics.h"

ZynthboxBasics::ZynthboxBasics(QObject* parent)
    : QObject(parent)
{ }

ZynthboxBasics::~ZynthboxBasics()
{ }

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
