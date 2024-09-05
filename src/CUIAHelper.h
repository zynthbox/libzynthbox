#pragma once

#include "ZynthboxBasics.h"

#include <QObject>
#include <QCoreApplication>
#include <QDebug>

class CUIAHelperPrivate;
/**
 * \brief A class used to convert CUIA commands between a programmatically helpful enum, and the string types that go with them
 */
class CUIAHelper : public QObject
{
    Q_OBJECT
public:
    static CUIAHelper* instance() {
        static CUIAHelper* instance{nullptr};
        if (!instance) {
            instance = new CUIAHelper(qApp);
        }
        return instance;
    };
    explicit CUIAHelper(QObject *parent = nullptr);
    ~CUIAHelper() override;

    enum Event {
        NoCuiaEvent,
        PowerOffEvent,
        RebootEvent,
        RestartUiEvent,
        ReloadMidiConfigEvent,
        ReloadKeybindingsEvent,
        LastStateActionEvent,
        AllNotesOffEvent,
        AllSoundsOffEvent,
        AllOffEvent,
        StartAudioRecordEvent,
        StopAudioRecordEvent,
        ToggleAudioRecordEvent,
        StartAudioPlayEvent,
        StopAudioPlayEvent,
        ToggleAudioPlayEvent,
        StartMidiRecordEvent,
        StopMidiRecordEvent,
        ToggleMidiRecordEvent,
        StartMidiPlayEvent,
        StopMidiPlayEvent,
        ToggleMidiPlayEvent,
        ZlPlayEvent,
        ZlStopEvent,
        StartRecordEvent,
        StopRecordEvent,
        SelectEvent,
        SelectUpEvent,
        SelectDownEvent,
        SelectLeftEvent,
        SelectRightEvent,
        NavigateLeftEvent,
        NavigateRightEvent,
        BackUpEvent,
        BackDownEvent,
        LayerUpEvent,
        LayerDownEvent,
        SnapshotUpEvent,
        SnapshotDownEvent,
        SceneUpEvent,
        SceneDownEvent,
        KeyboardEvent,
        SwitchLayerShortEvent,
        SwitchLayerBoldEvent,
        SwitchLayerLongEvent,
        SwitchBackShortEvent,
        SwitchBackBoldEvent,
        SwitchBackLongEvent,
        SwitchSnapshotShortEvent,
        SwitchSnapshotBoldEvent,
        SwitchSnapshotLongEvent,
        SwitchSelectShortEvent,
        SwitchSelectBoldEvent,
        SwitchSelectLongEvent,
        ModeSwitchShortEvent,
        ModeSwitchBoldEvent,
        ModeSwitchLongEvent,
        SwitchChannelsModShortEvent,
        SwitchChannelsModBoldEvent,
        SwitchChannelsModLongEvent,
        SwitchMetronomeShortEvent,
        SwitchMetronomeBoldEvent,
        SwitchMetronomeLongEvent,
        ScreenAdminEvent,
        ScreenAudioSettingsEvent,
        ScreenBankEvent,
        ScreenControlEvent,
        ScreenEditContextualEvent,
        ScreenLayerEvent,
        ScreenLayerFxEvent,
        ScreenMainEvent,
        ScreenPlaygridEvent,
        ScreenPresetEvent,
        ScreenSketchpadEvent,
        ScreenSongManagerEvent,
        ModalSnapshotLoadEvent,
        ModalSnapshotSaveEvent,
        ModalAudioRecorderEvent,
        ModalMidiRecorderEvent,
        ModalAlsaMixerEvent,
        ModalStepseqEvent,
        Channel1Event,
        Channel2Event,
        Channel3Event,
        Channel4Event,
        Channel5Event,
        Channel6Event,
        Channel7Event,
        Channel8Event,
        Channel9Event,
        Channel10Event,
        ChannelPreviousEvent,
        ChannelNextEvent,
        Knob0UpEvent,
        Knob0DownEvent,
        Knob0TouchedEvent,
        Knob0ReleasedEvent,
        Knob1UpEvent,
        Knob1DownEvent,
        Knob1TouchedEvent,
        Knob1ReleasedEvent,
        Knob2UpEvent,
        Knob2DownEvent,
        Knob2TouchedEvent,
        Knob2ReleasedEvent,
        Knob3UpEvent,
        Knob3DownEvent,
        Knob3TouchedEvent,
        Knob3ReleasedEvent,
        IncreaseEvent,
        DecreaseEvent,
        // The following events are supposed to be sent along with a value of some description. The value, where appropriate, will be an integer from 0 through 127 inclusive
        SwitchPressedEvent, ///@< Tell the UI that a specific switch has been pressed. The given value indicates a specific switch ID
        SwitchReleasedEvent, ///@< Tell the UI that a specific switch has been released. The given value indicates a specific switch ID
        ActivateTrackEvent, ///@< Set the given track active
        ToggleTrackMuted, ///@< Toggle the muted state of the given track
        ToggleTrackSoloed, ///@< Toggle the soloed state of the given track
        SetTrackVolumeEvent, ///@< Set the given track's volume to the given value
        SetTrackPanEvent, ///@< Set the given track's pan to the given value
        SetTrackSend1AmountEvent, ///@< Set the given track's send 1 amount to the given value
        SetTrackSend2AmountEvent, ///@< Set the given track's send 2 amount to the given value
        TogglePartEvent, ///@< Toggle the given part's active state
        SetPartActiveStateEvent, ///@< Sets the part to either active or inactive (value of 0 is active, 1 is inactive, 2 is that it will be inactive on the next beat, 3 is that it will be active on the next bar)
        SetPartGain, ///@< Set the gain of the given part to the given value
        SetFxAmount, ///@< Set the wet/dry mix for the given fx to the given value
    };
    Q_ENUM(Event)

    /**
     * \brief Get a human-readable name for the given CUIA event
     * @param cuiaEvent A CUIA event to retrieve a human-readable name for
     * @return The human-readable name for the given event
     */
    Q_INVOKABLE QString cuiaTitle(const Event &cuiaEvent) const;
    /**
     * \brief Get the CUIA event string for the given CUIA event
     * @param cuiaEvent A CUIA event to retrieve a CUIA event string for
     * @return The CUIA event string for the given event
     */
    Q_INVOKABLE QString cuiaCommand(const Event &cuiaEvent) const;
    /**
     * \brief Get the CUIA event matching the given CUIA command string
     * @param cuiaCommand The command to convert to a CUIA event value
     * @return The CUIA event for the given command string (will return NoCuiaEvent for a string with no match)
     */
    Q_INVOKABLE Event cuiaEvent(const QString &cuiaCommand) const;

    /**
     * \brief Whether the given event uses the track parameter
     * @param event The event you want to know details about
     * @return True if the given event uses the track parameter, false otherwise
     */
    Q_INVOKABLE bool cuiaEventWantsATrack(const Event &cuiaEvent) const;
    /**
     * \brief Whether the given event uses the part parameter
     * @param event The event you want to know details about
     * @return True if the given event uses the part parameter, false otherwise
     */
    Q_INVOKABLE bool cuiaEventWantsAPart(const Event &cuiaEvent) const;
    /**
     * \brief Whether the given event uses the value parameter
     * @param event The event you want to know details about
     * @return True if the given event uses the value parameter, false otherwise
     */
    Q_INVOKABLE bool cuiaEventWantsAValue(const Event &cuiaEvent) const;

    /**
     * \brief Get a human-readable description of the given CUIA event and associated flags
     * @param cuiaEvent A CUIA event to get a description of
     * @param track The track the event is associated with (will be ignored if the event doesn't use this parameter)
     * @param part The part the event is associated with (will be ignored if the event doesn't use this parameter)
     * @param value The value the event is associated with (will be ignored if the event doesn't use this parameter)
     * @param upperValue Treat value as a lower limit, and describe the values in a range
     */
    Q_INVOKABLE QString describe(const Event &cuiaEvent, const ZynthboxBasics::Track &track, const ZynthboxBasics::Part &part, const int &value, const int &upperValue = -1) const;

    /**
     * \brief Get the human-readable name of the given switch
     */
    Q_INVOKABLE QString switchName(const int &switchIndex) const;
private:
    CUIAHelperPrivate *d{nullptr};
};
Q_DECLARE_METATYPE(CUIAHelper::Event)

#define CUIARingSize 512
class CUIARing {
public:
    struct Entry {
        Entry *next{nullptr};
        Entry *previous{nullptr};
        CUIAHelper::Event event{CUIAHelper::NoCuiaEvent};
        int originId{-1};
        ZynthboxBasics::Track track{ZynthboxBasics::CurrentTrack};
        ZynthboxBasics::Part part{ZynthboxBasics::CurrentPart};
        int value{0};
        bool processed{true};
    };
    explicit CUIARing() {
        Entry* entryPrevious{&ringData[CUIARingSize - 1]};
        for (quint64 i = 0; i < CUIARingSize; ++i) {
            entryPrevious->next = &ringData[i];
            ringData[i].previous = entryPrevious;
            entryPrevious = &ringData[i];
        }
        readHead = writeHead = ringData;
    }
    ~CUIARing() {
    }

    void write(const CUIAHelper::Event &event, const int &originId, const ZynthboxBasics::Track &track = ZynthboxBasics::CurrentTrack, const ZynthboxBasics::Part &part = ZynthboxBasics::CurrentPart, const double &value = 0) {
        Entry *entry = writeHead;
        writeHead = writeHead->next;
        if (entry->processed == false) {
            qWarning() << Q_FUNC_INFO << "There is unprocessed data at the write location:" << entry->event << "This likely means the buffer size is too small, which will require attention at the api level.";
        }
        entry->event = event;
        entry->originId = originId;
        entry->track = track;
        entry->part = part;
        entry->value = value;
        entry->processed = false;
    }
    CUIAHelper::Event read(int *originId = nullptr, ZynthboxBasics::Track *track = nullptr, ZynthboxBasics::Part *part = nullptr, int *value = nullptr) {
        Entry *entry = readHead;
        readHead = readHead->next;
        CUIAHelper::Event event = entry->event;
        entry->event = CUIAHelper::NoCuiaEvent;
        if (originId) {
            *originId = entry->originId;
        }
        if (track) {
            *track = entry->track;
        }
        if (part) {
            *part = entry->part;
        }
        if (value) {
            *value = entry->value;
        }
        entry->processed = true;
        return event;
    }

    Entry *readHead{nullptr};
    Entry *writeHead{nullptr};
private:
    Entry ringData[CUIARingSize];
};
