#include "ClipAudioSourcePositionsModel.h"
#include "ClipCommand.h"
#include <QDateTime>
#include <QDebug>

#define POSITION_COUNT 32

#define DataRingSize 2048
class DataRing {
public:
    struct alignas(64) Entry {
        Entry *previous{nullptr};
        Entry *next{nullptr};
        ClipCommand *clipCommand{nullptr};
        float progress{0.0f};
        float gain{0.0f};
        float pan{0.0f};
        jack_nframes_t timestamp{0};
        bool processed{true};
    };

    DataRing () {
        Entry* entryPrevious{&ringData[DataRingSize - 1]};
        for (quint64 i = 0; i < DataRingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~DataRing() {}
    void write(jack_nframes_t timestamp, ClipCommand *clipCommand, float progress, float gain, float pan) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data stored at the write location: id" << writeHead->clipCommand << "for time" << writeHead->timestamp << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->clipCommand = clipCommand;
        entry->progress = progress;
        entry->gain = gain;
        entry->pan = pan;
        entry->timestamp = timestamp;
        entry->processed = false;
    }
    /**
     * \brief Attempt to read the data out of the ring, until there are no more unprocessed entries
     * @return Whether or not the read was valid
     */
    bool read(jack_nframes_t *timestamp, ClipCommand **clipCommand, float *progress, float *gain, float *pan) {
        if (readHead->processed == false) {
            Entry *entry = readHead;
            readHead = readHead->next;
            *clipCommand = entry->clipCommand;
            *progress = entry->progress;
            *gain = entry->gain;
            *pan = entry->pan;
            *timestamp = entry->timestamp;
            entry->processed = true;
            return true;
        }
        return false;
    }
    Entry ringData[DataRingSize];
    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
    QString name;
};

struct PositionData {
    qint64 id{-1};
    ClipCommand *clipCommand{nullptr};
    float progress{0.0f};
    float gain{0.0f};
    float pan{0.0f};
    qint64 keepUntil{0};
};

class ClipAudioSourcePositionsModelPrivate
{
public:
    ClipAudioSourcePositionsModelPrivate() {
        positionUpdates.name = "PositionUpdates";
    }
    ~ClipAudioSourcePositionsModelPrivate() {
    }
    PositionData positions[POSITION_COUNT];
    bool updatePeakGain{false};
    float peakGain{0.0f};
    jack_nframes_t mostRecentPositionUpdate{0};
    // ui update period, or double the frame size, which ever is larger
    jack_nframes_t updateGracePeriod{2048};
    DataRing positionUpdates;
};

ClipAudioSourcePositionsModel::ClipAudioSourcePositionsModel(ClipAudioSource *clip)
    : QAbstractListModel(clip)
    , d(new ClipAudioSourcePositionsModelPrivate)
{
}

ClipAudioSourcePositionsModel::~ClipAudioSourcePositionsModel() = default;

QHash<int, QByteArray> ClipAudioSourcePositionsModel::roleNames() const
{
    static const QHash<int, QByteArray> roleNames{
        {PositionIDRole, "positionID"},
        {PositionProgressRole, "positionProgress"},
        {PositionGainRole, "positionGain"},
        {PositionPanRole, "positionPan"},
    };
    return roleNames;
}

int ClipAudioSourcePositionsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return POSITION_COUNT;
}

QVariant ClipAudioSourcePositionsModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (checkIndex(index)) {
        PositionData *position = &d->positions[index.row()];
        switch (role) {
            case PositionIDRole:
                result.setValue<qint64>(position->clipCommand ? position->id : -1);
                break;
            case PositionProgressRole:
                result.setValue<float>(position->clipCommand ? position->progress : 0);
                break;
            case PositionGainRole:
                result.setValue<float>(position->clipCommand ? position->gain : 0);
                break;
            case PositionPanRole:
                result.setValue<float>(position->clipCommand ? position->pan : 0);
                break;
            default:
                break;
        }
    }
    return result;
}

void ClipAudioSourcePositionsModel::setPositionData(jack_nframes_t timestamp, ClipCommand *clipCommand, float gain, float progress, float pan)
{
    d->positionUpdates.write(timestamp, clipCommand, progress, gain, pan);
    d->mostRecentPositionUpdate = timestamp; // we can safely do this without checking, as this timestamp will always grow
    d->updatePeakGain = true;
}

void ClipAudioSourcePositionsModel::setMostRecentPositionUpdate(jack_nframes_t timestamp)
{
    d->mostRecentPositionUpdate = timestamp;
}

float ClipAudioSourcePositionsModel::peakGain()
{
    if (d->updatePeakGain) {
        // First update the positions given new data
        updatePositions();
        // Then update the peak gain
        float peak{0.0f};
        for (int positionIndex = 0; positionIndex < POSITION_COUNT; ++positionIndex) {
            peak = qMax(peak, d->positions[positionIndex].gain);
        }
        if (abs(d->peakGain - peak) > 0.01) {
            d->peakGain = peak;
        }
        d->updatePeakGain = false;
        QMetaObject::invokeMethod(this, "peakGainChanged", Qt::QueuedConnection);
    }
    return d->peakGain;
}

double ClipAudioSourcePositionsModel::firstProgress() const
{
    double progress{-1.0f};
    for (int positionIndex = 0; positionIndex < POSITION_COUNT; ++positionIndex) {
        const PositionData &position = d->positions[positionIndex];
        if (position.id > -1) {
            progress = position.progress;
            break;
        }
    }
    return progress;
}

void ClipAudioSourcePositionsModel::updatePositions()
{
    bool anyPositionUpdates{false};
    // Clear out all positions older than our grace time, so we can stuff things into the model
    for (int positionIndex = 0; positionIndex < POSITION_COUNT; ++positionIndex) {
        PositionData &position = d->positions[positionIndex];
        if (position.keepUntil < d->mostRecentPositionUpdate) {
            position.clipCommand = nullptr;
            position.id = -1;
            anyPositionUpdates = true;
        }
    }
    // Now add in all the new data
    int positionIndex{0};
    jack_nframes_t timestamp;
    ClipCommand *clipCommand;
    float progress, gain, pan;
    while (d->positionUpdates.read(&timestamp, &clipCommand, &progress, &gain, &pan)) {
        for (positionIndex = 0; positionIndex < POSITION_COUNT; ++positionIndex) {
            PositionData *position = &d->positions[positionIndex];
            // If this is the same clip command, or it's an empty position (which will not happen unless we haven't added updates for this command yet), add the data to this position
            if (position->clipCommand == clipCommand || position->clipCommand == nullptr) {
                position->clipCommand = clipCommand;
                position->id = reinterpret_cast<qint64>(clipCommand);
                position->progress = progress;
                position->gain = gain;
                position->pan = pan;
                position->keepUntil = timestamp + d->updateGracePeriod;
                anyPositionUpdates = true;
                break;
            }
        }
    }
    // Now notify that the model has changed its data (which is cheaper than a reset, as it updates existing delegates instead of remaking them)
    if (anyPositionUpdates) {
        QModelIndex topLeft{createIndex(0, 0)};
        QModelIndex bottomRight{createIndex(POSITION_COUNT - 1, 0)};
        dataChanged(topLeft, bottomRight, {PositionIDRole, PositionProgressRole, PositionGainRole, PositionPanRole});
    }
}
