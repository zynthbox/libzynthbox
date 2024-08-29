#include "ZynthboxBasics.h"

ZynthboxBasics::ZynthboxBasics(QObject* parent)
    : QObject(parent)
{ }

ZynthboxBasics::~ZynthboxBasics()
{ }

QString ZynthboxBasics::trackLabelText(const Track& track) const
{
    switch(track) {
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

QString ZynthboxBasics::partLabelText(const Part& part) const
{
    switch(part) {
        case AnyPart:
            return QLatin1String{"Any Part"};
        case CurrentPart:
            return QLatin1String{"Currently Part"};
        case Part1:
            return QLatin1String{"Part 1"};
        case Part2:
            return QLatin1String{"Part 2"};
        case Part3:
            return QLatin1String{"Part 3"};
        case Part4:
            return QLatin1String{"Part 4"};
        case Part5:
            return QLatin1String{"Part 5"};
    }
    return QLatin1String{"Unknown Part"};
}
