#include "CUIAHelper.h"

class CUIAHelperPrivate {
public:
    CUIAHelperPrivate() {}
    const QHash<CUIAHelper::Event, QString> titles{
        {CUIAHelper::NoCuiaEvent, QLatin1String{"No Event"}},
        {CUIAHelper::PowerOffEvent, QLatin1String{"Show Power Off Popup"}},
        {CUIAHelper::RebootEvent, QLatin1String{"Show Reboot Popup"}},
        {CUIAHelper::RestartUiEvent, QLatin1String{"Show UI Restart Popup"}},
        {CUIAHelper::ReloadMidiConfigEvent, QLatin1String{"Reload Midi Configuration"}},
        {CUIAHelper::ReloadKeybindingsEvent, QLatin1String{"Reload Keybindings"}},
        {CUIAHelper::LastStateActionEvent, QLatin1String{"Recall Last State"}},
        {CUIAHelper::AllNotesOffEvent, QLatin1String{"Send All Notes Off"}},
        {CUIAHelper::AllSoundsOffEvent, QLatin1String{"Send All Sounds Off"}},
        {CUIAHelper::AllOffEvent, QLatin1String{"Send All Off"}},
        {CUIAHelper::StartAudioRecordEvent, QLatin1String{"Start Audio Recording"}},
        {CUIAHelper::StopAudioRecordEvent, QLatin1String{"Stop Audio Recording"}},
        {CUIAHelper::ToggleAudioRecordEvent, QLatin1String{"Toggle Audio Recording"}},
        {CUIAHelper::StartAudioPlayEvent, QLatin1String{"Start Audio Playback"}},
        {CUIAHelper::StopAudioPlayEvent, QLatin1String{"Stop Audio Playback"}},
        {CUIAHelper::ToggleAudioPlayEvent, QLatin1String{"Toggle Audio Playback"}},
        {CUIAHelper::StartMidiRecordEvent, QLatin1String{"Start Midi Recording"}},
        {CUIAHelper::StopMidiRecordEvent, QLatin1String{"Stop Midi Recording"}},
        {CUIAHelper::ToggleMidiRecordEvent, QLatin1String{"Toggle Midi Recording"}},
        {CUIAHelper::StartMidiPlayEvent, QLatin1String{"Start Midi Playback"}},
        {CUIAHelper::StopMidiPlayEvent, QLatin1String{"Stop Midi Playback"}},
        {CUIAHelper::ToggleMidiPlayEvent, QLatin1String{"Toggle Midi Playback"}},
        {CUIAHelper::ZlPlayEvent, QLatin1String{"Start Playback"}},
        {CUIAHelper::ZlStopEvent, QLatin1String{"Stop Playback"}},
        {CUIAHelper::StartRecordEvent, QLatin1String{"Record"}},
        {CUIAHelper::StopRecordEvent, QLatin1String{"Stop Recording"}},
        {CUIAHelper::SelectEvent, QLatin1String{"Select"}},
        {CUIAHelper::SelectUpEvent, QLatin1String{"Select Up"}},
        {CUIAHelper::SelectDownEvent, QLatin1String{"Select Down"}},
        {CUIAHelper::SelectLeftEvent, QLatin1String{"Select Left"}},
        {CUIAHelper::SelectRightEvent, QLatin1String{"Select Right"}},
        {CUIAHelper::NavigateLeftEvent, QLatin1String{"Navigate Left"}},
        {CUIAHelper::NavigateRightEvent, QLatin1String{"Navigate Right"}},
        {CUIAHelper::BackUpEvent, QLatin1String{"Back Up"}},
        {CUIAHelper::BackDownEvent, QLatin1String{"Back Down"}},
        {CUIAHelper::LayerUpEvent, QLatin1String{"Layer Up"}},
        {CUIAHelper::LayerDownEvent, QLatin1String{"Layer Down"}},
        {CUIAHelper::SnapshotUpEvent, QLatin1String{"Snapshot Up"}},
        {CUIAHelper::SnapshotDownEvent, QLatin1String{"Snapshot Down"}},
        {CUIAHelper::SceneUpEvent, QLatin1String{"Scene Up"}},
        {CUIAHelper::SceneDownEvent, QLatin1String{"Scene Down"}},
        {CUIAHelper::KeyboardEvent, QLatin1String{"Toggle Keyboard"}},
        {CUIAHelper::SwitchLayerShortEvent, QLatin1String{"Short Press Layer Button"}},
        {CUIAHelper::SwitchLayerBoldEvent, QLatin1String{"Bold Press Layer Button"}},
        {CUIAHelper::SwitchLayerLongEvent, QLatin1String{"Long Press Layer Button"}},
        {CUIAHelper::SwitchBackShortEvent, QLatin1String{"Short Press Back Button"}},
        {CUIAHelper::SwitchBackBoldEvent, QLatin1String{"Bold Press Back Button"}},
        {CUIAHelper::SwitchBackLongEvent, QLatin1String{"Long Press Back Button"}},
        {CUIAHelper::SwitchSnapshotShortEvent, QLatin1String{"Short Press Snapshot Button"}},
        {CUIAHelper::SwitchSnapshotBoldEvent, QLatin1String{"Bold Press Snapshot Button"}},
        {CUIAHelper::SwitchSnapshotLongEvent, QLatin1String{"Long Press Snapshot Button"}},
        {CUIAHelper::SwitchSelectShortEvent, QLatin1String{"Short Press Select Button"}},
        {CUIAHelper::SwitchSelectBoldEvent, QLatin1String{"Bold Press Select Button"}},
        {CUIAHelper::SwitchSelectLongEvent, QLatin1String{"Long Press Select Button"}},
        {CUIAHelper::ModeSwitchShortEvent, QLatin1String{"Short Press Mode Button"}},
        {CUIAHelper::ModeSwitchBoldEvent, QLatin1String{"Bold Press Mode Button"}},
        {CUIAHelper::ModeSwitchLongEvent, QLatin1String{"Long Press Mode Button"}},
        {CUIAHelper::SwitchChannelsModShortEvent, QLatin1String{"Short Press Channel Mod Button"}},
        {CUIAHelper::SwitchChannelsModBoldEvent, QLatin1String{"Bold Press Channel Mod Button"}},
        {CUIAHelper::SwitchChannelsModLongEvent, QLatin1String{"Long Press Channel Mod Button"}},
        {CUIAHelper::SwitchMetronomeShortEvent, QLatin1String{"Short Press Metronome Button"}},
        {CUIAHelper::SwitchMetronomeBoldEvent, QLatin1String{"Bold Press Metronome Button"}},
        {CUIAHelper::SwitchMetronomeLongEvent, QLatin1String{"Long Press Metronome Button"}},
        {CUIAHelper::ScreenAdminEvent, QLatin1String{"Show Admin Screen"}},
        {CUIAHelper::ScreenAudioSettingsEvent, QLatin1String{"Show Audio Settings Screen"}},
        {CUIAHelper::ScreenBankEvent, QLatin1String{"Show Bank Screen"}},
        {CUIAHelper::ScreenControlEvent, QLatin1String{"Show Control Screen"}},
        {CUIAHelper::ScreenEditContextualEvent, QLatin1String{"Show Contextual Edit Screen"}},
        {CUIAHelper::ScreenLayerEvent, QLatin1String{"Show Layer Screen"}},
        {CUIAHelper::ScreenLayerFxEvent, QLatin1String{"Show Layer FX Screen"}},
        {CUIAHelper::ScreenMainEvent, QLatin1String{"Show Main Menu"}},
        {CUIAHelper::ScreenPlaygridEvent, QLatin1String{"Show Playground"}},
        {CUIAHelper::ScreenPresetEvent, QLatin1String{"Show Preset Selection Screen"}},
        {CUIAHelper::ScreenSketchpadEvent, QLatin1String{"Show Sketchpad"}},
        {CUIAHelper::ScreenSongManagerEvent, QLatin1String{"Show Song Manager"}},
        {CUIAHelper::ModalSnapshotLoadEvent, QLatin1String{"Load Snapshot"}},
        {CUIAHelper::ModalSnapshotSaveEvent, QLatin1String{"Save Snapshot"}},
        {CUIAHelper::ModalAudioRecorderEvent, QLatin1String{"Show Audio Recorder"}},
        {CUIAHelper::ModalMidiRecorderEvent, QLatin1String{"Show Midi Recorder"}},
        {CUIAHelper::ModalAlsaMixerEvent, QLatin1String{"Show Mixer"}},
        {CUIAHelper::ModalStepseqEvent, QLatin1String{"Show Step Sequencer"}},
        {CUIAHelper::Channel1Event, QLatin1String{"Switch to Track 1"}},
        {CUIAHelper::Channel2Event, QLatin1String{"Switch to Track 2"}},
        {CUIAHelper::Channel3Event, QLatin1String{"Switch to Track 3"}},
        {CUIAHelper::Channel4Event, QLatin1String{"Switch to Track 4"}},
        {CUIAHelper::Channel5Event, QLatin1String{"Switch to Track 5"}},
        {CUIAHelper::Channel6Event, QLatin1String{"Switch to Track 6"}},
        {CUIAHelper::Channel7Event, QLatin1String{"Switch to Track 7"}},
        {CUIAHelper::Channel8Event, QLatin1String{"Switch to Track 8"}},
        {CUIAHelper::Channel9Event, QLatin1String{"Switch to Track 9"}},
        {CUIAHelper::Channel10Event, QLatin1String{"Switch to Track 10"}},
        {CUIAHelper::ChannelPreviousEvent, QLatin1String{"Switch to Previous Track"}},
        {CUIAHelper::ChannelNextEvent, QLatin1String{"Switch to Next Track"}},
        {CUIAHelper::Knob0UpEvent, QLatin1String{"Knob 1: Up"}},
        {CUIAHelper::Knob0DownEvent, QLatin1String{"Knob 1: Down"}},
        {CUIAHelper::Knob0TouchedEvent, QLatin1String{"Knob 1: Touch"}},
        {CUIAHelper::Knob0ReleasedEvent, QLatin1String{"Knob 1: Release"}},
        {CUIAHelper::Knob1UpEvent, QLatin1String{"Knob 2: Up"}},
        {CUIAHelper::Knob1DownEvent, QLatin1String{"Knob 2: Down"}},
        {CUIAHelper::Knob1TouchedEvent, QLatin1String{"Knob 2: Touch"}},
        {CUIAHelper::Knob1ReleasedEvent, QLatin1String{"Knob 2: Release"}},
        {CUIAHelper::Knob2UpEvent, QLatin1String{"Knob 3: Up"}},
        {CUIAHelper::Knob2DownEvent, QLatin1String{"Knob 3: Down"}},
        {CUIAHelper::Knob2TouchedEvent, QLatin1String{"Knob 3: Touch"}},
        {CUIAHelper::Knob2ReleasedEvent, QLatin1String{"Knob 3: Release"}},
        {CUIAHelper::Knob3UpEvent, QLatin1String{"Knob 4: Up"}},
        {CUIAHelper::Knob3DownEvent, QLatin1String{"Knob 4: Down"}},
        {CUIAHelper::Knob3TouchedEvent, QLatin1String{"Knob 4: Touch"}},
        {CUIAHelper::Knob3ReleasedEvent, QLatin1String{"Knob 4: Release"}},
        {CUIAHelper::IncreaseEvent, QLatin1String{"Increase Value"}},
        {CUIAHelper::DecreaseEvent, QLatin1String{"Decrease Value"}},
        {CUIAHelper::SwitchPressedEvent, QLatin1String{"Switch Pressed"}},
        {CUIAHelper::SwitchReleasedEvent, QLatin1String{"Switch Released"}},
        {CUIAHelper::ActivateTrackEvent, QLatin1String{"Activate Track"}}, ///@< Set the given track active
        {CUIAHelper::ToggleTrackMuted, QLatin1String{"Toggle Track Muted"}}, ///@< Toggle the muted state of the given track
        {CUIAHelper::ToggleTrackSoloed, QLatin1String{"Toggle Track Soloed"}}, ///@< Toggle the soloed state of the given track
        {CUIAHelper::SetTrackVolumeEvent, QLatin1String{"Set Track Volume"}}, ///@< Set the given track's volume to the given value
        {CUIAHelper::SetTrackPanEvent, QLatin1String{"Set Track Pan"}}, ///@< Set the given track's pan to the given value
        {CUIAHelper::SetTrackSend1AmountEvent, QLatin1String{"Set Track Send 1 Amount"}}, ///@< Set the given track's send 1 amount to the given value
        {CUIAHelper::SetTrackSend2AmountEvent, QLatin1String{"Set Track Send 2 Amount"}}, ///@< Set the given track's send 2 amount to the given value
        {CUIAHelper::SetPartActiveStateEvent, QLatin1String{"Set Part Active State"}}, ///@< Sets the part to either active or inactive (value of 0 is active, 1 is inactive, 2 is that it will be active on the next bar)
        {CUIAHelper::TogglePartEvent, QLatin1String{"Toggle Part Active State"}}, ///@< Toggle the given part's active state
        {CUIAHelper::SetPartGain, QLatin1String{"Set Part Gain"}}, ///@< Set the gain of the given part to the given value
        {CUIAHelper::SetFxAmount, QLatin1String{"Set FX Amount"}}, ///@< Set the wet/dry mix for the given fx to the given value
    };
    const QHash<CUIAHelper::Event, QString> commands{
        {CUIAHelper::NoCuiaEvent, QLatin1String{"NONE"}},
        {CUIAHelper::PowerOffEvent, QLatin1String{"POWER_OFF"}},
        {CUIAHelper::RebootEvent, QLatin1String{"REBOOT"}},
        {CUIAHelper::RestartUiEvent, QLatin1String{"RESTART_UI"}},
        {CUIAHelper::ReloadMidiConfigEvent, QLatin1String{"RELOAD_MIDI_CONFIG"}},
        {CUIAHelper::ReloadKeybindingsEvent, QLatin1String{"RELOAD_KEYBINDINGS"}},
        {CUIAHelper::LastStateActionEvent, QLatin1String{"LAST_STATE_ACTION"}},
        {CUIAHelper::AllNotesOffEvent, QLatin1String{"ALL_NOTES_OFF"}},
        {CUIAHelper::AllSoundsOffEvent, QLatin1String{"ALL_SOUNDS_OFF"}},
        {CUIAHelper::AllOffEvent, QLatin1String{"ALL_OFF"}},
        {CUIAHelper::StartAudioRecordEvent, QLatin1String{"START_AUDIO_RECORD"}},
        {CUIAHelper::StopAudioRecordEvent, QLatin1String{"STOP_AUDIO_RECORD"}},
        {CUIAHelper::ToggleAudioRecordEvent, QLatin1String{"TOGGLE_AUDIO_RECORD"}},
        {CUIAHelper::StartAudioPlayEvent, QLatin1String{"START_AUDIO_PLAY"}},
        {CUIAHelper::StopAudioPlayEvent, QLatin1String{"STOP_AUDIO_PLAY"}},
        {CUIAHelper::ToggleAudioPlayEvent, QLatin1String{"TOGGLE_AUDIO_PLAY"}},
        {CUIAHelper::StartMidiRecordEvent, QLatin1String{"START_MIDI_RECORD"}},
        {CUIAHelper::StopMidiRecordEvent, QLatin1String{"STOP_MIDI_RECORD"}},
        {CUIAHelper::ToggleMidiRecordEvent, QLatin1String{"TOGGLE_MIDI_RECORD"}},
        {CUIAHelper::StartMidiPlayEvent, QLatin1String{"START_MIDI_PLAY"}},
        {CUIAHelper::StopMidiPlayEvent, QLatin1String{"STOP_MIDI_PLAY"}},
        {CUIAHelper::ToggleMidiPlayEvent, QLatin1String{"TOGGLE_MIDI_PLAY"}},
        {CUIAHelper::ZlPlayEvent, QLatin1String{"ZL_PLAY"}},
        {CUIAHelper::ZlStopEvent, QLatin1String{"ZL_STOP"}},
        {CUIAHelper::StartRecordEvent, QLatin1String{"START_RECORD"}},
        {CUIAHelper::StopRecordEvent, QLatin1String{"STOP_RECORD"}},
        {CUIAHelper::SelectEvent, QLatin1String{"SELECT"}},
        {CUIAHelper::SelectUpEvent, QLatin1String{"SELECT_UP"}},
        {CUIAHelper::SelectDownEvent, QLatin1String{"SELECT_DOWN"}},
        {CUIAHelper::SelectLeftEvent, QLatin1String{"SELECT_LEFT"}},
        {CUIAHelper::SelectRightEvent, QLatin1String{"SELECT_RIGHT"}},
        {CUIAHelper::NavigateLeftEvent, QLatin1String{"NAVIGATE_LEFT"}},
        {CUIAHelper::NavigateRightEvent, QLatin1String{"NAVIGATE_RIGHT"}},
        {CUIAHelper::BackUpEvent, QLatin1String{"BACK_UP"}},
        {CUIAHelper::BackDownEvent, QLatin1String{"BACK_DOWN"}},
        {CUIAHelper::LayerUpEvent, QLatin1String{"LAYER_UP"}},
        {CUIAHelper::LayerDownEvent, QLatin1String{"LAYER_DOWN"}},
        {CUIAHelper::SnapshotUpEvent, QLatin1String{"SNAPSHOT_UP"}},
        {CUIAHelper::SnapshotDownEvent, QLatin1String{"SNAPSHOT_DOWN"}},
        {CUIAHelper::SceneUpEvent, QLatin1String{"SCENE_UP"}},
        {CUIAHelper::SceneDownEvent, QLatin1String{"SCENE_DOWN"}},
        {CUIAHelper::KeyboardEvent, QLatin1String{"KEYBOARD"}},
        {CUIAHelper::SwitchLayerShortEvent, QLatin1String{"SWITCH_LAYER_SHORT"}},
        {CUIAHelper::SwitchLayerBoldEvent, QLatin1String{"SWITCH_LAYER_BOLD"}},
        {CUIAHelper::SwitchLayerLongEvent, QLatin1String{"SWITCH_LAYER_LONG"}},
        {CUIAHelper::SwitchBackShortEvent, QLatin1String{"SWITCH_BACK_SHORT"}},
        {CUIAHelper::SwitchBackBoldEvent, QLatin1String{"SWITCH_BACK_BOLD"}},
        {CUIAHelper::SwitchBackLongEvent, QLatin1String{"SWITCH_BACK_LONG"}},
        {CUIAHelper::SwitchSnapshotShortEvent, QLatin1String{"SWITCH_SNAPSHOT_SHORT"}},
        {CUIAHelper::SwitchSnapshotBoldEvent, QLatin1String{"SWITCH_SNAPSHOT_BOLD"}},
        {CUIAHelper::SwitchSnapshotLongEvent, QLatin1String{"SWITCH_SNAPSHOT_LONG"}},
        {CUIAHelper::SwitchSelectShortEvent, QLatin1String{"SWITCH_SELECT_SHORT"}},
        {CUIAHelper::SwitchSelectBoldEvent, QLatin1String{"SWITCH_SELECT_BOLD"}},
        {CUIAHelper::SwitchSelectLongEvent, QLatin1String{"SWITCH_SELECT_LONG"}},
        {CUIAHelper::ModeSwitchShortEvent, QLatin1String{"MODE_SWITCH_SHORT"}},
        {CUIAHelper::ModeSwitchBoldEvent, QLatin1String{"MODE_SWITCH_BOLD"}},
        {CUIAHelper::ModeSwitchLongEvent, QLatin1String{"MODE_SWITCH_LONG"}},
        {CUIAHelper::SwitchChannelsModShortEvent, QLatin1String{"SWITCH_CHANNELS_SHORT"}},
        {CUIAHelper::SwitchChannelsModBoldEvent, QLatin1String{"SWITCH_CHANNELS_BOLD"}},
        {CUIAHelper::SwitchChannelsModLongEvent, QLatin1String{"SWITCH_CHANNELS_LONG"}},
        {CUIAHelper::SwitchMetronomeShortEvent, QLatin1String{"SWITCH_METRONOME_SHORT"}},
        {CUIAHelper::SwitchMetronomeBoldEvent, QLatin1String{"SWITCH_METRONOME_BOLD"}},
        {CUIAHelper::SwitchMetronomeLongEvent, QLatin1String{"SWITCH_METRONOME_LONG"}},
        {CUIAHelper::ScreenAdminEvent, QLatin1String{"SCREEN_ADMIN"}},
        {CUIAHelper::ScreenAudioSettingsEvent, QLatin1String{"SCREEN_AUDIO_SETTINGS"}},
        {CUIAHelper::ScreenBankEvent, QLatin1String{"SCREEN_BANK"}},
        {CUIAHelper::ScreenControlEvent, QLatin1String{"SCREEN_CONTROL"}},
        {CUIAHelper::ScreenEditContextualEvent, QLatin1String{"SCREEN_EDIT_CONTEXTUAL"}},
        {CUIAHelper::ScreenLayerEvent, QLatin1String{"SCREEN_LAYER"}},
        {CUIAHelper::ScreenLayerFxEvent, QLatin1String{"SCREEN_LAYER_FX"}},
        {CUIAHelper::ScreenMainEvent, QLatin1String{"SCREEN_MAIN"}},
        {CUIAHelper::ScreenPlaygridEvent, QLatin1String{"SCREEN_PLAYGRID"}},
        {CUIAHelper::ScreenPresetEvent, QLatin1String{"SCREEN_PRESET"}},
        {CUIAHelper::ScreenSketchpadEvent, QLatin1String{"SCREEN_SKETCHPAD"}},
        {CUIAHelper::ScreenSongManagerEvent, QLatin1String{"SCREEN_SONG_MANAGER"}},
        {CUIAHelper::ModalSnapshotLoadEvent, QLatin1String{"MODAL_SNAPSHOT_LOAD"}},
        {CUIAHelper::ModalSnapshotSaveEvent, QLatin1String{"MODAL_SNAPSHOT_SAVE"}},
        {CUIAHelper::ModalAudioRecorderEvent, QLatin1String{"MODAL_AUDIO_RECORDER"}},
        {CUIAHelper::ModalMidiRecorderEvent, QLatin1String{"MODAL_MIDI_RECORDER"}},
        {CUIAHelper::ModalAlsaMixerEvent, QLatin1String{"MODAL_ALSA_MIXER"}},
        {CUIAHelper::ModalStepseqEvent, QLatin1String{"MODAL_STEPSEQ"}},
        {CUIAHelper::Channel1Event, QLatin1String{"CHANNEL_1"}},
        {CUIAHelper::Channel2Event, QLatin1String{"CHANNEL_2"}},
        {CUIAHelper::Channel3Event, QLatin1String{"CHANNEL_3"}},
        {CUIAHelper::Channel4Event, QLatin1String{"CHANNEL_4"}},
        {CUIAHelper::Channel5Event, QLatin1String{"CHANNEL_5"}},
        {CUIAHelper::Channel6Event, QLatin1String{"CHANNEL_6"}},
        {CUIAHelper::Channel7Event, QLatin1String{"CHANNEL_7"}},
        {CUIAHelper::Channel8Event, QLatin1String{"CHANNEL_8"}},
        {CUIAHelper::Channel9Event, QLatin1String{"CHANNEL_9"}},
        {CUIAHelper::Channel10Event, QLatin1String{"CHANNEL_10"}},
        {CUIAHelper::ChannelPreviousEvent, QLatin1String{"CHANNEL_PREVIOUS"}},
        {CUIAHelper::ChannelNextEvent, QLatin1String{"CHANNEL_NEXT"}},
        {CUIAHelper::Knob0UpEvent, QLatin1String{"KNOB0_UP"}},
        {CUIAHelper::Knob0DownEvent, QLatin1String{"KNOB0_DOWN"}},
        {CUIAHelper::Knob0TouchedEvent, QLatin1String{"KNOB0_TOUCHED"}},
        {CUIAHelper::Knob0ReleasedEvent, QLatin1String{"KNOB0_RELEASED"}},
        {CUIAHelper::Knob1UpEvent, QLatin1String{"KNOB1_UP"}},
        {CUIAHelper::Knob1DownEvent, QLatin1String{"KNOB1_DOWN"}},
        {CUIAHelper::Knob1TouchedEvent, QLatin1String{"KNOB1_TOUCHED"}},
        {CUIAHelper::Knob1ReleasedEvent, QLatin1String{"KNOB1_RELEASED"}},
        {CUIAHelper::Knob2UpEvent, QLatin1String{"KNOB2_UP"}},
        {CUIAHelper::Knob2DownEvent, QLatin1String{"KNOB2_DOWN"}},
        {CUIAHelper::Knob2TouchedEvent, QLatin1String{"KNOB2_TOUCHED"}},
        {CUIAHelper::Knob2ReleasedEvent, QLatin1String{"KNOB2_RELEASED"}},
        {CUIAHelper::Knob3UpEvent, QLatin1String{"KNOB3_UP"}},
        {CUIAHelper::Knob3DownEvent, QLatin1String{"KNOB3_DOWN"}},
        {CUIAHelper::Knob3TouchedEvent, QLatin1String{"KNOB3_TOUCHED"}},
        {CUIAHelper::Knob3ReleasedEvent, QLatin1String{"KNOB3_RELEASED"}},
        {CUIAHelper::IncreaseEvent, QLatin1String{"INCREASE"}},
        {CUIAHelper::DecreaseEvent, QLatin1String{"DECREASE"}},
        // The following need handling in "special ways" at the consumer (python) level, as they all come with particular values
        {CUIAHelper::SwitchPressedEvent, QLatin1String{"SWITCH_PRESSED"}}, ///@< Tell the UI that a specific switch has been pressed. The given value indicates a specific switch ID
        {CUIAHelper::SwitchReleasedEvent, QLatin1String{"SWITCH_RELEASED"}}, ///@< Tell the UI that a specific switch has been released. The given value indicates a specific switch ID
        {CUIAHelper::ActivateTrackEvent, QLatin1String{"ACTIVATE_TRACK"}}, ///@< Set the given track active
        {CUIAHelper::ToggleTrackMuted, QLatin1String{"TOGGLE_TRACK_MUTED"}}, ///@< Toggle the muted state of the given track
        {CUIAHelper::ToggleTrackSoloed, QLatin1String{"TOGGLE_TRACK_SOLOED"}}, ///@< Toggle the soloed state of the given track
        {CUIAHelper::SetTrackVolumeEvent, QLatin1String{"SET_TRACK_VOLUME"}}, ///@< Set the given track's volume to the given value
        {CUIAHelper::SetTrackPanEvent, QLatin1String{"SET_TRACK_PAN"}}, ///@< Set the given track's pan to the given value
        {CUIAHelper::SetTrackSend1AmountEvent, QLatin1String{"SET_TRACK_SEND1_AMOUNT"}}, ///@< Set the given track's send 1 amount to the given value
        {CUIAHelper::SetTrackSend2AmountEvent, QLatin1String{"SET_TRACK_SEND2_AMOUNT"}}, ///@< Set the given track's send 2 amount to the given value
        {CUIAHelper::TogglePartEvent, QLatin1String{"TOGGLE_PART"}}, ///@< Toggle the given part's active state
        {CUIAHelper::SetPartActiveStateEvent, QLatin1String{"SET_PART_ACTIVE_STATE"}}, ///@< Sets the part to either active or inactive (value of 0 is active, 1 is inactive, 2 is that it will be inactive on the next beat, 3 is that it will be active on the next bar)
        {CUIAHelper::SetPartGain, QLatin1String{"SET_PART_GAIN"}}, ///@< Set the gain of the given part to the given value
        {CUIAHelper::SetFxAmount, QLatin1String{"SET_FX_AMOUNT"}}, ///@< Set the wet/dry mix for the given fx to the given value
    };
};

CUIAHelper::CUIAHelper(QObject* parent)
    : QObject(parent)
    , d(new CUIAHelperPrivate)
{
    qRegisterMetaType<CUIAHelper::Event>();
}

CUIAHelper::~CUIAHelper()
{
    delete d;
}

QString CUIAHelper::cuiaTitle(const Event& cuiaEvent) const
{
    return d->titles[cuiaEvent];
}

QString CUIAHelper::cuiaCommand(const Event& cuiaEvent) const
{
    return d->commands[cuiaEvent];
}

CUIAHelper::Event CUIAHelper::cuiaEvent(const QString& cuiaCommand) const
{
    return d->commands.key(cuiaCommand, NoCuiaEvent);
}

bool CUIAHelper::cuiaEventWantsATrack(const Event& cuiaEvent) const
{
    static const QList<Event> eventsThatWantATrack{
        ActivateTrackEvent,
        ToggleTrackMuted,
        ToggleTrackSoloed,
        SetTrackVolumeEvent,
        SetTrackPanEvent,
        SetTrackSend1AmountEvent,
        SetTrackSend2AmountEvent,
        TogglePartEvent,
        SetPartActiveStateEvent,
        SetPartGain,
        SetFxAmount,
    };
    if (eventsThatWantATrack.contains(cuiaEvent)) {
        return true;
    }
    return false;
}

bool CUIAHelper::cuiaEventWantsAPart(const Event& cuiaEvent) const
{
    static const QList<Event> eventsThatWantAPart{
        TogglePartEvent,
        SetPartActiveStateEvent,
        SetPartGain,
        SetFxAmount,
    };
    if (eventsThatWantAPart.contains(cuiaEvent)) {
        return true;
    }
    return false;
}

bool CUIAHelper::cuiaEventWantsAValue(const Event& cuiaEvent) const
{
    static const QList<Event> eventsThatWantAValue{
        SetTrackVolumeEvent,
        SetTrackPanEvent,
        SetTrackSend1AmountEvent,
        SetTrackSend2AmountEvent,
        SetPartActiveStateEvent,
        SetPartGain,
        SetFxAmount,
    };
    if (eventsThatWantAValue.contains(cuiaEvent)) {
        return true;
    }
    return false;
}

// Get a floating point value between -1.0 and 1.0 for a given CC value (that is, 0 through 127), with 63 being 0.0 (meaning both 126 and 127 are 1.0)
static inline float centeredRelativeCCValue(const int &ccValue) {
    return float(std::clamp(ccValue, 0, 126) - 63) / 63.0f;
}

// Get a floating point value between 0.0 and 1.0 for a give CC value (that is, 0 through 127)
static inline float relativeCCValue(const int &ccValue) {
    return float(std::clamp(ccValue, 0, 127)) / 127.0f;
}

QString CUIAHelper::describe(const Event& cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Part& part, const int& value, const int &upperValue) const
{
    if (cuiaEvent == CUIAHelper::SwitchPressedEvent) {
        return QString{"Switch %1 Pressed"}.arg(value); // This wants to be named - a getter for switch names by index probably
    } else if (cuiaEvent == CUIAHelper::SwitchReleasedEvent) {
        return QString{"Switch %1 Released"}.arg(value); // This wants to be named - a getter for switch names by index probably
    } else if (cuiaEvent == CUIAHelper::ActivateTrackEvent) {
        return QString{"Activate %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track));
    } else if (cuiaEvent == CUIAHelper::ToggleTrackMuted) {
        return QString{"Toggle %1 Muted"}.arg(ZynthboxBasics::instance()->trackLabelText(track));
    } else if (cuiaEvent == CUIAHelper::SetTrackVolumeEvent) {
        if (upperValue == -1) {
            return QString{"Set %1 volume to %2%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * relativeCCValue(value)));
        } else {
            return QString{"Set %1 volume to between %2% and %3%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * relativeCCValue(value))).arg(int(100 * relativeCCValue(upperValue)));
        }
    } else if (cuiaEvent == CUIAHelper::SetTrackPanEvent) {
        if (upperValue == -1) {
            return QString{"Set %1 pan to %2%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * centeredRelativeCCValue(value)));
        } else {
            return QString{"Set %1 pan to between %2% and %3%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * centeredRelativeCCValue(value))).arg(int(100 * centeredRelativeCCValue(upperValue)));
        }
    } else if (cuiaEvent == CUIAHelper::SetTrackSend1AmountEvent) {
        if (upperValue == -1) {
            return QString{"Set %1 Send FX 1 amount to %2%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * relativeCCValue(value)));
        } else {
            return QString{"Set %1 Send FX 1 amount to between %2% and %3%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * relativeCCValue(value))).arg(int(100 * relativeCCValue(upperValue)));
        }
    } else if (cuiaEvent == CUIAHelper::SetTrackSend2AmountEvent) {
        return QString{"Set %1 Send FX 2 amount to %2%"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(100 * relativeCCValue(value)));
    } else if (cuiaEvent == CUIAHelper::TogglePartEvent) {
        return QString{"Toggle %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part));
    } else if (cuiaEvent == CUIAHelper::ActivateTrackEvent) {
        switch(value) {
            default:
            case 0:
                return QString{"Activate %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part));
                break;
            case 1:
                return QString{"Deactivate %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part));
                break;
            case 2:
                return QString{"Activate %2 on %1 Next Bar"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part));
                break;
            case 3:
                return QString{"Deactivate %2 on %1 Next Bar"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part));
                break;
        }
    } else if (cuiaEvent == CUIAHelper::SetPartGain) {
        if (upperValue == -1) {
            return QString{"Set Gain to %3% for %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part)).arg(int(100 * relativeCCValue(value)));
        } else {
            return QString{"Set Gain to between %3% and %4% for %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(ZynthboxBasics::instance()->partLabelText(part)).arg(int(100 * relativeCCValue(value))).arg(int(100 * relativeCCValue(upperValue)));
        }
    } else if (cuiaEvent == CUIAHelper::SetFxAmount) {
        if (upperValue == -1) {
            return QString{"Set FX wet/dry mix to %3% for slot %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(part) + 1).arg(int(100 * centeredRelativeCCValue(value)));
        } else {
            return QString{"Set FX wet/dry mix to between %3% and %4% for slot %2 on %1"}.arg(ZynthboxBasics::instance()->trackLabelText(track)).arg(int(part) + 1).arg(int(100 * centeredRelativeCCValue(value))).arg(int(100 * centeredRelativeCCValue(upperValue)));
        }
    } else {
        return cuiaTitle(cuiaEvent);
    }
}

QString CUIAHelper::switchName(const int& switchIndex) const
{
    QString name{"Unknown Switch"};
    switch(switchIndex) {
        case 0:
            name = QLatin1String{"Unnamed Switch Index 0"};
            break;
        case 1:
            name = QLatin1String{"Unnamed Switch Index 1"};
            break;
        case 2:
            name = QLatin1String{"Unnamed Switch Index 2"};
            break;
        case 3:
            name = QLatin1String{"Unnamed Switch Index 3"};
            break;
        case 4:
            name = QLatin1String{"Unnamed Switch Index 4"};
            break;
        case 5:
            name = QLatin1String{"Track 1 button"};
            break;
        case 6:
            name = QLatin1String{"Track 2 button"};
            break;
        case 7:
            name = QLatin1String{"Track 3 button"};
            break;
        case 8:
            name = QLatin1String{"Track 4 button"};
            break;
        case 9:
            name = QLatin1String{"Track 5 button"};
            break;
        case 10:
            name = QLatin1String{"Track * button"};
            break;
        case 11:
            name = QLatin1String{"Mode button"};
            break;
        case 12:
            name = QLatin1String{"Sketchpad/F1 button"};
            break;
        case 13:
            name = QLatin1String{"Playground/F2 button "};
            break;
        case 14:
            name = QLatin1String{"Song Editor/F3 button"};
            break;
        case 15:
            name = QLatin1String{"Presets/F4 button"};
            break;
        case 16:
            name = QLatin1String{"Sound Editor/F5 button"};
            break;
        case 17:
            name = QLatin1String{"Alt button"};
            break;
        case 18:
            name = QLatin1String{"Record button"};
            break;
        case 19:
            name = QLatin1String{"Play button"};
            break;
        case 20:
            name = QLatin1String{"Metronome button"};
            break;
        case 21:
            name = QLatin1String{"Stop button"};
            break;
        case 22:
            name = QLatin1String{"Back/No button"};
            break;
        case 23:
            name = QLatin1String{"Up arrow button"};
            break;
        case 24:
            name = QLatin1String{"Select/Yes button"};
            break;
        case 25:
            name = QLatin1String{"Left arrow button"};
            break;
        case 26:
            name = QLatin1String{"Down arrow button"};
            break;
        case 27:
            name = QLatin1String{"Right arrow button"};
            break;
        case 28:
            name = QLatin1String{"Global button"};
            break;
        case 29:
            name = QLatin1String{"Big Knob button"};
            break;
        case 30:
            name = QLatin1String{"Knob 1"};
            break;
        case 31:
            name = QLatin1String{"Knob 0"};
            break;
        case 32:
            name = QLatin1String{"Knob 2"};
            break;
        case 33:
            name = QLatin1String{"Big Knob"};
            break;
    }
    return name;
}
