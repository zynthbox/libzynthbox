#pragma once

#include <QCoreApplication>

#define ZynthboxSongCount 1
#define ZynthboxTrackCount 10
#define ZynthboxPartCount 5
#define ZynthboxClipMaximumPositionCount 128

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

    enum Part {
        AnyPart = -2,
        CurrentPart = -1,
        Part1 = 0,
        Part2 = 1,
        Part3 = 2,
        Part4 = 3,
        Part5 = 4,
    };
    Q_ENUM(Part)

    Q_INVOKABLE QString partLabelText(const Part &part) const;
    Q_INVOKABLE QString clipLabelText(const Part &part) const;
    Q_INVOKABLE QString slotLabelText(const Part &part) const;
    Q_INVOKABLE QString fxLabelText(const Part &part) const;
};
