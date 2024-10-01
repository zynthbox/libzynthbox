#include "MidiRouterFilterEntry.h"
#include "MidiRouterFilter.h"

#include <QDebug>
#include <QTimer>

MidiRouterFilterEntry::MidiRouterFilterEntry(MidiRouterDevice* routerDevice, MidiRouterFilter* parent)
    : QObject(parent)
    , m_routerDevice(routerDevice)
{
    // During loading, this is likely to get hit quite a lot, so let's ensure we throttle it, make it a bit lighter
    QTimer *descriptionThrottle = new QTimer(this);
    descriptionThrottle->setInterval(0);
    descriptionThrottle->setSingleShot(true);
    descriptionThrottle->callOnTimeout(this, &MidiRouterFilterEntry::descripionChanged);
    connect(this, &MidiRouterFilterEntry::requiredBytesChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte1MinimumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte1MaximumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte2MinimumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte2MaximumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte3MinimumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::byte3MaximumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::cuiaEventChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::originTrackChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::originSlotChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::valueMinimumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
    connect(this, &MidiRouterFilterEntry::valueMaximumChanged, descriptionThrottle, QOverload<>::of(&QTimer::start));
}

MidiRouterFilterEntry::~MidiRouterFilterEntry()
{
}

bool MidiRouterFilterEntry::match(const jack_midi_event_t& event) const
{
    if (event.size == size_t(m_requiredBytes)) {
        switch(m_requiredBytes) {
            case 3:
                if (m_byte1Minimum <= event.buffer[0] && event.buffer[0] <= m_byte1Maximum && m_byte2Minimum <= event.buffer[1] && event.buffer[1] <= m_byte2Maximum && m_byte3Minimum <= event.buffer[2] && event.buffer[2] <= m_byte3Maximum) {
                    mangleEvent(event);
                    return true;
                }
                break;
            case 2:
                if (m_byte1Minimum <= event.buffer[0] && event.buffer[0] <= m_byte1Maximum && m_byte2Minimum <= event.buffer[1] && event.buffer[1] <= m_byte2Maximum) {
                    mangleEvent(event);
                    return true;
                }
                break;
            default:
            case 1:
                if (m_byte1Minimum <= event.buffer[0] && event.buffer[0] <= m_byte1Maximum) {
                    mangleEvent(event);
                    return true;
                }
                break;
        }
    }
    return false;
}

void MidiRouterFilterEntry::mangleEvent(const jack_midi_event_t& event) const
{
    int byteIndex{0};
    const int eventChannel{event.buffer[0] & 0xf};
    for (const MidiRouterFilterEntryRewriter * rule : qAsConst(m_rewriteRules)) {
        switch(rule->m_type) {
            case MidiRouterFilterEntryRewriter::TrackRule:
                rule->m_bufferEvent->size = size_t(rule->m_byteSize);
                for (byteIndex = 0; byteIndex < rule->m_byteSize; ++ byteIndex) {
                    switch(rule->m_bytes[byteIndex]) {
                        case MidiRouterFilterEntryRewriter::OriginalByte1:
                            rule->m_bufferEvent->buffer[byteIndex] = event.buffer[0];
                            break;
                        case MidiRouterFilterEntryRewriter::OriginalByte2:
                           rule->m_bufferEvent->buffer[byteIndex] = event.size > 1 ? event.buffer[1] : 0;
                            break;
                        case MidiRouterFilterEntryRewriter::OriginalByte3:
                            rule->m_bufferEvent->buffer[byteIndex] = event.size > 2 ? event.buffer[2] : 0;
                            break;
                        case MidiRouterFilterEntryRewriter::ExplicitByte0:
                        case MidiRouterFilterEntryRewriter::ExplicitByte1:
                        case MidiRouterFilterEntryRewriter::ExplicitByte2:
                        case MidiRouterFilterEntryRewriter::ExplicitByte3:
                        case MidiRouterFilterEntryRewriter::ExplicitByte4:
                        case MidiRouterFilterEntryRewriter::ExplicitByte5:
                        case MidiRouterFilterEntryRewriter::ExplicitByte6:
                        case MidiRouterFilterEntryRewriter::ExplicitByte7:
                        case MidiRouterFilterEntryRewriter::ExplicitByte8:
                        case MidiRouterFilterEntryRewriter::ExplicitByte9:
                        case MidiRouterFilterEntryRewriter::ExplicitByte10:
                        case MidiRouterFilterEntryRewriter::ExplicitByte11:
                        case MidiRouterFilterEntryRewriter::ExplicitByte12:
                        case MidiRouterFilterEntryRewriter::ExplicitByte13:
                        case MidiRouterFilterEntryRewriter::ExplicitByte14:
                        case MidiRouterFilterEntryRewriter::ExplicitByte15:
                        case MidiRouterFilterEntryRewriter::ExplicitByte16:
                        case MidiRouterFilterEntryRewriter::ExplicitByte17:
                        case MidiRouterFilterEntryRewriter::ExplicitByte18:
                        case MidiRouterFilterEntryRewriter::ExplicitByte19:
                        case MidiRouterFilterEntryRewriter::ExplicitByte20:
                        case MidiRouterFilterEntryRewriter::ExplicitByte21:
                        case MidiRouterFilterEntryRewriter::ExplicitByte22:
                        case MidiRouterFilterEntryRewriter::ExplicitByte23:
                        case MidiRouterFilterEntryRewriter::ExplicitByte24:
                        case MidiRouterFilterEntryRewriter::ExplicitByte25:
                        case MidiRouterFilterEntryRewriter::ExplicitByte26:
                        case MidiRouterFilterEntryRewriter::ExplicitByte27:
                        case MidiRouterFilterEntryRewriter::ExplicitByte28:
                        case MidiRouterFilterEntryRewriter::ExplicitByte29:
                        case MidiRouterFilterEntryRewriter::ExplicitByte30:
                        case MidiRouterFilterEntryRewriter::ExplicitByte31:
                        case MidiRouterFilterEntryRewriter::ExplicitByte32:
                        case MidiRouterFilterEntryRewriter::ExplicitByte33:
                        case MidiRouterFilterEntryRewriter::ExplicitByte34:
                        case MidiRouterFilterEntryRewriter::ExplicitByte35:
                        case MidiRouterFilterEntryRewriter::ExplicitByte36:
                        case MidiRouterFilterEntryRewriter::ExplicitByte37:
                        case MidiRouterFilterEntryRewriter::ExplicitByte38:
                        case MidiRouterFilterEntryRewriter::ExplicitByte39:
                        case MidiRouterFilterEntryRewriter::ExplicitByte40:
                        case MidiRouterFilterEntryRewriter::ExplicitByte41:
                        case MidiRouterFilterEntryRewriter::ExplicitByte42:
                        case MidiRouterFilterEntryRewriter::ExplicitByte43:
                        case MidiRouterFilterEntryRewriter::ExplicitByte44:
                        case MidiRouterFilterEntryRewriter::ExplicitByte45:
                        case MidiRouterFilterEntryRewriter::ExplicitByte46:
                        case MidiRouterFilterEntryRewriter::ExplicitByte47:
                        case MidiRouterFilterEntryRewriter::ExplicitByte48:
                        case MidiRouterFilterEntryRewriter::ExplicitByte49:
                        case MidiRouterFilterEntryRewriter::ExplicitByte50:
                        case MidiRouterFilterEntryRewriter::ExplicitByte51:
                        case MidiRouterFilterEntryRewriter::ExplicitByte52:
                        case MidiRouterFilterEntryRewriter::ExplicitByte53:
                        case MidiRouterFilterEntryRewriter::ExplicitByte54:
                        case MidiRouterFilterEntryRewriter::ExplicitByte55:
                        case MidiRouterFilterEntryRewriter::ExplicitByte56:
                        case MidiRouterFilterEntryRewriter::ExplicitByte57:
                        case MidiRouterFilterEntryRewriter::ExplicitByte58:
                        case MidiRouterFilterEntryRewriter::ExplicitByte59:
                        case MidiRouterFilterEntryRewriter::ExplicitByte60:
                        case MidiRouterFilterEntryRewriter::ExplicitByte61:
                        case MidiRouterFilterEntryRewriter::ExplicitByte62:
                        case MidiRouterFilterEntryRewriter::ExplicitByte63:
                        case MidiRouterFilterEntryRewriter::ExplicitByte64:
                        case MidiRouterFilterEntryRewriter::ExplicitByte65:
                        case MidiRouterFilterEntryRewriter::ExplicitByte66:
                        case MidiRouterFilterEntryRewriter::ExplicitByte67:
                        case MidiRouterFilterEntryRewriter::ExplicitByte68:
                        case MidiRouterFilterEntryRewriter::ExplicitByte69:
                        case MidiRouterFilterEntryRewriter::ExplicitByte70:
                        case MidiRouterFilterEntryRewriter::ExplicitByte71:
                        case MidiRouterFilterEntryRewriter::ExplicitByte72:
                        case MidiRouterFilterEntryRewriter::ExplicitByte73:
                        case MidiRouterFilterEntryRewriter::ExplicitByte74:
                        case MidiRouterFilterEntryRewriter::ExplicitByte75:
                        case MidiRouterFilterEntryRewriter::ExplicitByte76:
                        case MidiRouterFilterEntryRewriter::ExplicitByte77:
                        case MidiRouterFilterEntryRewriter::ExplicitByte78:
                        case MidiRouterFilterEntryRewriter::ExplicitByte79:
                        case MidiRouterFilterEntryRewriter::ExplicitByte80:
                        case MidiRouterFilterEntryRewriter::ExplicitByte81:
                        case MidiRouterFilterEntryRewriter::ExplicitByte82:
                        case MidiRouterFilterEntryRewriter::ExplicitByte83:
                        case MidiRouterFilterEntryRewriter::ExplicitByte84:
                        case MidiRouterFilterEntryRewriter::ExplicitByte85:
                        case MidiRouterFilterEntryRewriter::ExplicitByte86:
                        case MidiRouterFilterEntryRewriter::ExplicitByte87:
                        case MidiRouterFilterEntryRewriter::ExplicitByte88:
                        case MidiRouterFilterEntryRewriter::ExplicitByte89:
                        case MidiRouterFilterEntryRewriter::ExplicitByte90:
                        case MidiRouterFilterEntryRewriter::ExplicitByte91:
                        case MidiRouterFilterEntryRewriter::ExplicitByte92:
                        case MidiRouterFilterEntryRewriter::ExplicitByte93:
                        case MidiRouterFilterEntryRewriter::ExplicitByte94:
                        case MidiRouterFilterEntryRewriter::ExplicitByte95:
                        case MidiRouterFilterEntryRewriter::ExplicitByte96:
                        case MidiRouterFilterEntryRewriter::ExplicitByte97:
                        case MidiRouterFilterEntryRewriter::ExplicitByte98:
                        case MidiRouterFilterEntryRewriter::ExplicitByte99:
                        case MidiRouterFilterEntryRewriter::ExplicitByte100:
                        case MidiRouterFilterEntryRewriter::ExplicitByte101:
                        case MidiRouterFilterEntryRewriter::ExplicitByte102:
                        case MidiRouterFilterEntryRewriter::ExplicitByte103:
                        case MidiRouterFilterEntryRewriter::ExplicitByte104:
                        case MidiRouterFilterEntryRewriter::ExplicitByte105:
                        case MidiRouterFilterEntryRewriter::ExplicitByte106:
                        case MidiRouterFilterEntryRewriter::ExplicitByte107:
                        case MidiRouterFilterEntryRewriter::ExplicitByte108:
                        case MidiRouterFilterEntryRewriter::ExplicitByte109:
                        case MidiRouterFilterEntryRewriter::ExplicitByte110:
                        case MidiRouterFilterEntryRewriter::ExplicitByte111:
                        case MidiRouterFilterEntryRewriter::ExplicitByte112:
                        case MidiRouterFilterEntryRewriter::ExplicitByte113:
                        case MidiRouterFilterEntryRewriter::ExplicitByte114:
                        case MidiRouterFilterEntryRewriter::ExplicitByte115:
                        case MidiRouterFilterEntryRewriter::ExplicitByte116:
                        case MidiRouterFilterEntryRewriter::ExplicitByte117:
                        case MidiRouterFilterEntryRewriter::ExplicitByte118:
                        case MidiRouterFilterEntryRewriter::ExplicitByte119:
                        case MidiRouterFilterEntryRewriter::ExplicitByte120:
                        case MidiRouterFilterEntryRewriter::ExplicitByte121:
                        case MidiRouterFilterEntryRewriter::ExplicitByte122:
                        case MidiRouterFilterEntryRewriter::ExplicitByte123:
                        case MidiRouterFilterEntryRewriter::ExplicitByte124:
                        case MidiRouterFilterEntryRewriter::ExplicitByte125:
                        case MidiRouterFilterEntryRewriter::ExplicitByte126:
                        case MidiRouterFilterEntryRewriter::ExplicitByte127:
                        default:
                            // The explicit bytes are all some explicit byte value, that we can convert directly like so:
                            rule->m_bufferEvent->buffer[byteIndex] = jack_midi_data_t(byteIndex == 0 ? rule->m_bytes[byteIndex] + 128 : rule->m_bytes[byteIndex]);
                            break;
                    }
                    if (rule->m_bytesAddChannel[byteIndex]) {
                        rule->m_bufferEvent->buffer[byteIndex] += eventChannel;
                    }
                }
                break;
            case MidiRouterFilterEntryRewriter::UIRule:
                // This is done at match time (otherwise we'll end up potentially writing a whole bunch of extra events we don't want)
                switch (rule->m_cuiaEvent) {
                    // These are all the "stardard" events that don't take any parameters
                    case CUIAHelper::PowerOffEvent:
                    case CUIAHelper::RebootEvent:
                    case CUIAHelper::RestartUiEvent:
                    case CUIAHelper::ReloadMidiConfigEvent:
                    case CUIAHelper::ReloadKeybindingsEvent:
                    case CUIAHelper::LastStateActionEvent:
                    case CUIAHelper::AllNotesOffEvent:
                    case CUIAHelper::AllSoundsOffEvent:
                    case CUIAHelper::AllOffEvent:
                    case CUIAHelper::StartAudioRecordEvent:
                    case CUIAHelper::StopAudioRecordEvent:
                    case CUIAHelper::ToggleAudioRecordEvent:
                    case CUIAHelper::StartAudioPlayEvent:
                    case CUIAHelper::StopAudioPlayEvent:
                    case CUIAHelper::ToggleAudioPlayEvent:
                    case CUIAHelper::StartMidiRecordEvent:
                    case CUIAHelper::StopMidiRecordEvent:
                    case CUIAHelper::ToggleMidiRecordEvent:
                    case CUIAHelper::StartMidiPlayEvent:
                    case CUIAHelper::StopMidiPlayEvent:
                    case CUIAHelper::ToggleMidiPlayEvent:
                    case CUIAHelper::ZlPlayEvent:
                    case CUIAHelper::ZlStopEvent:
                    case CUIAHelper::StartRecordEvent:
                    case CUIAHelper::StopRecordEvent:
                    case CUIAHelper::SelectEvent:
                    case CUIAHelper::SelectUpEvent:
                    case CUIAHelper::SelectDownEvent:
                    case CUIAHelper::SelectLeftEvent:
                    case CUIAHelper::SelectRightEvent:
                    case CUIAHelper::NavigateLeftEvent:
                    case CUIAHelper::NavigateRightEvent:
                    case CUIAHelper::BackUpEvent:
                    case CUIAHelper::BackDownEvent:
                    case CUIAHelper::LayerUpEvent:
                    case CUIAHelper::LayerDownEvent:
                    case CUIAHelper::SnapshotUpEvent:
                    case CUIAHelper::SnapshotDownEvent:
                    case CUIAHelper::SceneUpEvent:
                    case CUIAHelper::SceneDownEvent:
                    case CUIAHelper::KeyboardEvent:
                    case CUIAHelper::SwitchLayerShortEvent:
                    case CUIAHelper::SwitchLayerBoldEvent:
                    case CUIAHelper::SwitchLayerLongEvent:
                    case CUIAHelper::SwitchBackShortEvent:
                    case CUIAHelper::SwitchBackBoldEvent:
                    case CUIAHelper::SwitchBackLongEvent:
                    case CUIAHelper::SwitchSnapshotShortEvent:
                    case CUIAHelper::SwitchSnapshotBoldEvent:
                    case CUIAHelper::SwitchSnapshotLongEvent:
                    case CUIAHelper::SwitchSelectShortEvent:
                    case CUIAHelper::SwitchSelectBoldEvent:
                    case CUIAHelper::SwitchSelectLongEvent:
                    case CUIAHelper::ModeSwitchShortEvent:
                    case CUIAHelper::ModeSwitchBoldEvent:
                    case CUIAHelper::ModeSwitchLongEvent:
                    case CUIAHelper::SwitchChannelsModShortEvent:
                    case CUIAHelper::SwitchChannelsModBoldEvent:
                    case CUIAHelper::SwitchChannelsModLongEvent:
                    case CUIAHelper::SwitchMetronomeShortEvent:
                    case CUIAHelper::SwitchMetronomeBoldEvent:
                    case CUIAHelper::SwitchMetronomeLongEvent:
                    case CUIAHelper::ScreenAdminEvent:
                    case CUIAHelper::ScreenAudioSettingsEvent:
                    case CUIAHelper::ScreenBankEvent:
                    case CUIAHelper::ScreenControlEvent:
                    case CUIAHelper::ScreenEditContextualEvent:
                    case CUIAHelper::ScreenLayerEvent:
                    case CUIAHelper::ScreenLayerFxEvent:
                    case CUIAHelper::ScreenMainEvent:
                    case CUIAHelper::ScreenPlaygridEvent:
                    case CUIAHelper::ScreenPresetEvent:
                    case CUIAHelper::ScreenSketchpadEvent:
                    case CUIAHelper::ScreenSongManagerEvent:
                    case CUIAHelper::ModalSnapshotLoadEvent:
                    case CUIAHelper::ModalSnapshotSaveEvent:
                    case CUIAHelper::ModalAudioRecorderEvent:
                    case CUIAHelper::ModalMidiRecorderEvent:
                    case CUIAHelper::ModalAlsaMixerEvent:
                    case CUIAHelper::ModalStepseqEvent:
                    case CUIAHelper::Channel1Event:
                    case CUIAHelper::Channel2Event:
                    case CUIAHelper::Channel3Event:
                    case CUIAHelper::Channel4Event:
                    case CUIAHelper::Channel5Event:
                    case CUIAHelper::Channel6Event:
                    case CUIAHelper::Channel7Event:
                    case CUIAHelper::Channel8Event:
                    case CUIAHelper::Channel9Event:
                    case CUIAHelper::Channel10Event:
                    case CUIAHelper::ChannelPreviousEvent:
                    case CUIAHelper::ChannelNextEvent:
                    case CUIAHelper::Knob0UpEvent:
                    case CUIAHelper::Knob0DownEvent:
                    case CUIAHelper::Knob0TouchedEvent:
                    case CUIAHelper::Knob0ReleasedEvent:
                    case CUIAHelper::Knob1UpEvent:
                    case CUIAHelper::Knob1DownEvent:
                    case CUIAHelper::Knob1TouchedEvent:
                    case CUIAHelper::Knob1ReleasedEvent:
                    case CUIAHelper::Knob2UpEvent:
                    case CUIAHelper::Knob2DownEvent:
                    case CUIAHelper::Knob2TouchedEvent:
                    case CUIAHelper::Knob2ReleasedEvent:
                    case CUIAHelper::Knob3UpEvent:
                    case CUIAHelper::Knob3DownEvent:
                    case CUIAHelper::Knob3TouchedEvent:
                    case CUIAHelper::Knob3ReleasedEvent:
                    case CUIAHelper::IncreaseEvent:
                    case CUIAHelper::DecreaseEvent:
                        m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id());
                        break;
                    // Only need the basics for these, so no need to calculate the value (not very costly, but no need to do it if we don't need to)
                    case CUIAHelper::ActivateTrackEvent:
                        // Set the given track active
                    case CUIAHelper::ToggleTrackMutedEvent:
                        // Toggle the muted state of the given track
                    case CUIAHelper::ToggleTrackSoloedEvent:
                        // Toggle the soloed state of the given track
                    case CUIAHelper::SetClipCurrentEvent:
                        // Sets the given clip as the currently visible one (if given a specific track, this will also change the track)
                    case CUIAHelper::ToggleClipEvent:
                        // Toggle the given clip's active state
                        m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot);
                        break;
                    // These all need a value, so do the calculation work for them
                    case CUIAHelper::SwitchPressedEvent:
                        // Tell the UI that a specific switch has been pressed. The given value indicates a specific switch ID
                    case CUIAHelper::SwitchReleasedEvent:
                        // Tell the UI that a specific switch has been released. The given value indicates a specific switch ID
                    case CUIAHelper::ActivateTrackRelativeEvent:
                        // A convenience function that will activate a track based on the given value (the tracks are split evenly across the 128 value options)
                    case CUIAHelper::SetTrackMutedEvent:
                        // Set whether the given track is muted or not (value of 0 is not muted, any other value is muted)
                    case CUIAHelper::SetTrackSoloedEvent:
                        // Set whether the given track is soloed or not (value of 0 is not soloed, any other value is soloed)
                    case CUIAHelper::SetTrackVolumeEvent:
                        // Set the given track's volume to the given value
                    case CUIAHelper::SetClipCurrentRelativeEvent:
                        // Sets the clip represented by the relative value, split evenly across the 128 values, as the currently visible one (if given a specific track, this will also change the track)
                    case CUIAHelper::SetClipActiveStateEvent:
                        // Sets the clip to either active or inactive (value of 0 is active, 1 is inactive, 2 is that it will be inactive on the next beat, 3 is that it will be active on the next bar)
                    case CUIAHelper::SetTrackPanEvent:
                        // Set the given track's pan to the given value
                    case CUIAHelper::SetTrackSend1AmountEvent:
                        // Set the given track's send 1 amount to the given value
                    case CUIAHelper::SetTrackSend2AmountEvent:
                        // Set the given track's send 2 amount to the given value
                    case CUIAHelper::SetSlotGainEvent:
                        // Set the gain of the given sound slot to the given value
                    case CUIAHelper::SetSlotPanEvent:
                        // Set the pan of the given sound slot to the given value
                    case CUIAHelper::SetFxAmountEvent:
                        // Set the wet/dry mix for the given fx
                    case CUIAHelper::SetTrackClipActiveRelativeEvent:
                        // Sets the currently active track and clip according to the given value (the clips are spread evenly across the 128 possible values, sequentially by track order)
                        switch (rule->m_cuiaValue) {
                            case MidiRouterFilterEntryRewriter::ValueEventChannel:
                                m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot, eventChannel);
                                break;
                            case MidiRouterFilterEntryRewriter::ValueByte1:
                                m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot, event.buffer[0]);
                                break;
                            case MidiRouterFilterEntryRewriter::ValueByte2:
                                m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot, event.buffer[1]);
                                break;
                            case MidiRouterFilterEntryRewriter::ValueByte3:
                                m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot, event.buffer[2]);
                                break;
                            case MidiRouterFilterEntryRewriter::ExplicitValue0:
                            case MidiRouterFilterEntryRewriter::ExplicitValue1:
                            case MidiRouterFilterEntryRewriter::ExplicitValue2:
                            case MidiRouterFilterEntryRewriter::ExplicitValue3:
                            case MidiRouterFilterEntryRewriter::ExplicitValue4:
                            case MidiRouterFilterEntryRewriter::ExplicitValue5:
                            case MidiRouterFilterEntryRewriter::ExplicitValue6:
                            case MidiRouterFilterEntryRewriter::ExplicitValue7:
                            case MidiRouterFilterEntryRewriter::ExplicitValue8:
                            case MidiRouterFilterEntryRewriter::ExplicitValue9:
                            case MidiRouterFilterEntryRewriter::ExplicitValue10:
                            case MidiRouterFilterEntryRewriter::ExplicitValue11:
                            case MidiRouterFilterEntryRewriter::ExplicitValue12:
                            case MidiRouterFilterEntryRewriter::ExplicitValue13:
                            case MidiRouterFilterEntryRewriter::ExplicitValue14:
                            case MidiRouterFilterEntryRewriter::ExplicitValue15:
                            case MidiRouterFilterEntryRewriter::ExplicitValue16:
                            case MidiRouterFilterEntryRewriter::ExplicitValue17:
                            case MidiRouterFilterEntryRewriter::ExplicitValue18:
                            case MidiRouterFilterEntryRewriter::ExplicitValue19:
                            case MidiRouterFilterEntryRewriter::ExplicitValue20:
                            case MidiRouterFilterEntryRewriter::ExplicitValue21:
                            case MidiRouterFilterEntryRewriter::ExplicitValue22:
                            case MidiRouterFilterEntryRewriter::ExplicitValue23:
                            case MidiRouterFilterEntryRewriter::ExplicitValue24:
                            case MidiRouterFilterEntryRewriter::ExplicitValue25:
                            case MidiRouterFilterEntryRewriter::ExplicitValue26:
                            case MidiRouterFilterEntryRewriter::ExplicitValue27:
                            case MidiRouterFilterEntryRewriter::ExplicitValue28:
                            case MidiRouterFilterEntryRewriter::ExplicitValue29:
                            case MidiRouterFilterEntryRewriter::ExplicitValue30:
                            case MidiRouterFilterEntryRewriter::ExplicitValue31:
                            case MidiRouterFilterEntryRewriter::ExplicitValue32:
                            case MidiRouterFilterEntryRewriter::ExplicitValue33:
                            case MidiRouterFilterEntryRewriter::ExplicitValue34:
                            case MidiRouterFilterEntryRewriter::ExplicitValue35:
                            case MidiRouterFilterEntryRewriter::ExplicitValue36:
                            case MidiRouterFilterEntryRewriter::ExplicitValue37:
                            case MidiRouterFilterEntryRewriter::ExplicitValue38:
                            case MidiRouterFilterEntryRewriter::ExplicitValue39:
                            case MidiRouterFilterEntryRewriter::ExplicitValue40:
                            case MidiRouterFilterEntryRewriter::ExplicitValue41:
                            case MidiRouterFilterEntryRewriter::ExplicitValue42:
                            case MidiRouterFilterEntryRewriter::ExplicitValue43:
                            case MidiRouterFilterEntryRewriter::ExplicitValue44:
                            case MidiRouterFilterEntryRewriter::ExplicitValue45:
                            case MidiRouterFilterEntryRewriter::ExplicitValue46:
                            case MidiRouterFilterEntryRewriter::ExplicitValue47:
                            case MidiRouterFilterEntryRewriter::ExplicitValue48:
                            case MidiRouterFilterEntryRewriter::ExplicitValue49:
                            case MidiRouterFilterEntryRewriter::ExplicitValue50:
                            case MidiRouterFilterEntryRewriter::ExplicitValue51:
                            case MidiRouterFilterEntryRewriter::ExplicitValue52:
                            case MidiRouterFilterEntryRewriter::ExplicitValue53:
                            case MidiRouterFilterEntryRewriter::ExplicitValue54:
                            case MidiRouterFilterEntryRewriter::ExplicitValue55:
                            case MidiRouterFilterEntryRewriter::ExplicitValue56:
                            case MidiRouterFilterEntryRewriter::ExplicitValue57:
                            case MidiRouterFilterEntryRewriter::ExplicitValue58:
                            case MidiRouterFilterEntryRewriter::ExplicitValue59:
                            case MidiRouterFilterEntryRewriter::ExplicitValue60:
                            case MidiRouterFilterEntryRewriter::ExplicitValue61:
                            case MidiRouterFilterEntryRewriter::ExplicitValue62:
                            case MidiRouterFilterEntryRewriter::ExplicitValue63:
                            case MidiRouterFilterEntryRewriter::ExplicitValue64:
                            case MidiRouterFilterEntryRewriter::ExplicitValue65:
                            case MidiRouterFilterEntryRewriter::ExplicitValue66:
                            case MidiRouterFilterEntryRewriter::ExplicitValue67:
                            case MidiRouterFilterEntryRewriter::ExplicitValue68:
                            case MidiRouterFilterEntryRewriter::ExplicitValue69:
                            case MidiRouterFilterEntryRewriter::ExplicitValue70:
                            case MidiRouterFilterEntryRewriter::ExplicitValue71:
                            case MidiRouterFilterEntryRewriter::ExplicitValue72:
                            case MidiRouterFilterEntryRewriter::ExplicitValue73:
                            case MidiRouterFilterEntryRewriter::ExplicitValue74:
                            case MidiRouterFilterEntryRewriter::ExplicitValue75:
                            case MidiRouterFilterEntryRewriter::ExplicitValue76:
                            case MidiRouterFilterEntryRewriter::ExplicitValue77:
                            case MidiRouterFilterEntryRewriter::ExplicitValue78:
                            case MidiRouterFilterEntryRewriter::ExplicitValue79:
                            case MidiRouterFilterEntryRewriter::ExplicitValue80:
                            case MidiRouterFilterEntryRewriter::ExplicitValue81:
                            case MidiRouterFilterEntryRewriter::ExplicitValue82:
                            case MidiRouterFilterEntryRewriter::ExplicitValue83:
                            case MidiRouterFilterEntryRewriter::ExplicitValue84:
                            case MidiRouterFilterEntryRewriter::ExplicitValue85:
                            case MidiRouterFilterEntryRewriter::ExplicitValue86:
                            case MidiRouterFilterEntryRewriter::ExplicitValue87:
                            case MidiRouterFilterEntryRewriter::ExplicitValue88:
                            case MidiRouterFilterEntryRewriter::ExplicitValue89:
                            case MidiRouterFilterEntryRewriter::ExplicitValue90:
                            case MidiRouterFilterEntryRewriter::ExplicitValue91:
                            case MidiRouterFilterEntryRewriter::ExplicitValue92:
                            case MidiRouterFilterEntryRewriter::ExplicitValue93:
                            case MidiRouterFilterEntryRewriter::ExplicitValue94:
                            case MidiRouterFilterEntryRewriter::ExplicitValue95:
                            case MidiRouterFilterEntryRewriter::ExplicitValue96:
                            case MidiRouterFilterEntryRewriter::ExplicitValue97:
                            case MidiRouterFilterEntryRewriter::ExplicitValue98:
                            case MidiRouterFilterEntryRewriter::ExplicitValue99:
                            case MidiRouterFilterEntryRewriter::ExplicitValue100:
                            case MidiRouterFilterEntryRewriter::ExplicitValue101:
                            case MidiRouterFilterEntryRewriter::ExplicitValue102:
                            case MidiRouterFilterEntryRewriter::ExplicitValue103:
                            case MidiRouterFilterEntryRewriter::ExplicitValue104:
                            case MidiRouterFilterEntryRewriter::ExplicitValue105:
                            case MidiRouterFilterEntryRewriter::ExplicitValue106:
                            case MidiRouterFilterEntryRewriter::ExplicitValue107:
                            case MidiRouterFilterEntryRewriter::ExplicitValue108:
                            case MidiRouterFilterEntryRewriter::ExplicitValue109:
                            case MidiRouterFilterEntryRewriter::ExplicitValue110:
                            case MidiRouterFilterEntryRewriter::ExplicitValue111:
                            case MidiRouterFilterEntryRewriter::ExplicitValue112:
                            case MidiRouterFilterEntryRewriter::ExplicitValue113:
                            case MidiRouterFilterEntryRewriter::ExplicitValue114:
                            case MidiRouterFilterEntryRewriter::ExplicitValue115:
                            case MidiRouterFilterEntryRewriter::ExplicitValue116:
                            case MidiRouterFilterEntryRewriter::ExplicitValue117:
                            case MidiRouterFilterEntryRewriter::ExplicitValue118:
                            case MidiRouterFilterEntryRewriter::ExplicitValue119:
                            case MidiRouterFilterEntryRewriter::ExplicitValue120:
                            case MidiRouterFilterEntryRewriter::ExplicitValue121:
                            case MidiRouterFilterEntryRewriter::ExplicitValue122:
                            case MidiRouterFilterEntryRewriter::ExplicitValue123:
                            case MidiRouterFilterEntryRewriter::ExplicitValue124:
                            case MidiRouterFilterEntryRewriter::ExplicitValue125:
                            case MidiRouterFilterEntryRewriter::ExplicitValue126:
                            case MidiRouterFilterEntryRewriter::ExplicitValue127:
                                m_routerDevice->cuiaRing.write(rule->m_cuiaEvent, m_routerDevice->id(), rule->m_cuiaTrack, rule->m_cuiaSlot, int(rule->m_cuiaValue));
                                break;
                        }
                        break;
                    default:
                    case CUIAHelper::NoCuiaEvent:
                        // Just Do Nothing:tm:
                        break;
                }
                break;
        }
    }
}

void MidiRouterFilterEntry::writeEventToDevice(MidiRouterDevice* device) const
{
    for (const MidiRouterFilterEntryRewriter * rule : qAsConst(m_rewriteRules)) {
        switch(rule->m_type) {
            case MidiRouterFilterEntryRewriter::TrackRule:
                device->writeEventToOutput(*rule->m_bufferEvent);
                break;
            case MidiRouterFilterEntryRewriter::UIRule:
                // This is done at match time (otherwise we'll end up potentially writing a whole bunch of extra events we don't want)
                break;
        }
    }
}

bool MidiRouterFilterEntry::matchCommand(const CUIAHelper::Event& cuiaEvent, const ZynthboxBasics::Track& track, const ZynthboxBasics::Slot& slot, const int& value) const
{
    if (m_cuiaEvent == cuiaEvent) {
        if (m_originTrack == ZynthboxBasics::AnyTrack || m_originTrack == track) {
            if (m_originSlot == ZynthboxBasics::AnySlot || m_originSlot == slot) {
                if (m_valueMinimum <= value && value <= m_valueMaximum) {
                    return true;
                }
            }
        }
    }
    return false;
}

ZynthboxBasics::Track MidiRouterFilterEntry::targetTrack() const
{
    return m_targetTrack;
}

void MidiRouterFilterEntry::setTargetTrack(const ZynthboxBasics::Track& targetTrack)
{
    if (m_targetTrack != targetTrack) {
        m_targetTrack = targetTrack;
        Q_EMIT targetTrackChanged();
    }
}

int MidiRouterFilterEntry::requiredBytes() const
{
    return m_requiredBytes;
}

void MidiRouterFilterEntry::setRequiredBytes(const int& requiredBytes)
{
    if (m_requiredBytes != requiredBytes) {
        m_requiredBytes = requiredBytes;
        Q_EMIT requiredBytesChanged();
    }
}

bool MidiRouterFilterEntry::requireRange() const
{
    return m_requireRange;
}

void MidiRouterFilterEntry::setRequireRange(const bool& requireRange)
{
    if (m_requireRange != requireRange) {
        m_requireRange = requireRange;
        Q_EMIT requireRangeChanged();
    }
}

int MidiRouterFilterEntry::byte1Minimum() const
{
    return m_byte1Minimum;
}

void MidiRouterFilterEntry::setByte1Minimum(const int& byte1Minimum)
{
    if (m_byte1Minimum != byte1Minimum) {
        m_byte1Minimum = byte1Minimum;
        Q_EMIT byte1MinimumChanged();
        if (m_byte1Maximum < m_byte1Minimum) {
            setByte1Maximum(m_byte1Minimum);
        }
    }
}

int MidiRouterFilterEntry::byte1Maximum() const
{
    return m_byte1Maximum;
}

void MidiRouterFilterEntry::setByte1Maximum(const int& byte1Maximum)
{
    if (m_byte1Maximum != byte1Maximum) {
        m_byte1Maximum = byte1Maximum;
        Q_EMIT byte1MaximumChanged();
        if (m_byte1Maximum < m_byte1Minimum) {
            setByte1Minimum(m_byte1Maximum);
        }
    }
}

int MidiRouterFilterEntry::byte2Minimum() const
{
    return m_byte2Minimum;
}

void MidiRouterFilterEntry::setByte2Minimum(const int& byte2Minimum)
{
    if (m_byte2Minimum != byte2Minimum) {
        m_byte2Minimum = byte2Minimum;
        Q_EMIT byte2MinimumChanged();
        if (m_byte2Maximum < m_byte2Minimum) {
            setByte2Maximum(m_byte2Minimum);
        }
    }
}

int MidiRouterFilterEntry::byte2Maximum() const
{
    return m_byte2Maximum;
}

void MidiRouterFilterEntry::setByte2Maximum(const int& byte2Maximum)
{
    if (m_byte2Maximum != byte2Maximum) {
        m_byte2Maximum = byte2Maximum;
        Q_EMIT byte2MaximumChanged();
        if (m_byte2Maximum < m_byte2Minimum) {
            setByte2Minimum(m_byte2Maximum);
        }
    }
}

int MidiRouterFilterEntry::byte3Minimum() const
{
    return m_byte3Minimum;
}

void MidiRouterFilterEntry::setByte3Minimum(const int& byte3Minimum)
{
    if (m_byte3Minimum != byte3Minimum) {
        m_byte3Minimum = byte3Minimum;
        Q_EMIT byte3MinimumChanged();
        if (m_byte3Maximum < m_byte3Minimum) {
            setByte3Maximum(m_byte3Minimum);
        }
    }
}

int MidiRouterFilterEntry::byte3Maximum() const
{
    return m_byte3Maximum;
}

void MidiRouterFilterEntry::setByte3Maximum(const int& byte3Maximum)
{
    if (m_byte3Maximum != byte3Maximum) {
        m_byte3Maximum = byte3Maximum;
        Q_EMIT byte3MaximumChanged();
        if (m_byte3Maximum < m_byte3Minimum) {
            setByte1Minimum(m_byte3Maximum);
        }
    }
}

CUIAHelper::Event MidiRouterFilterEntry::cuiaEvent() const
{
    return m_cuiaEvent;
}

void MidiRouterFilterEntry::setCuiaEvent(const CUIAHelper::Event& cuiaEvent)
{
    if (m_cuiaEvent != cuiaEvent) {
        m_cuiaEvent = cuiaEvent;
        Q_EMIT cuiaEventChanged();
    }
}

ZynthboxBasics::Track MidiRouterFilterEntry::originTrack() const
{
    return m_originTrack;
}

void MidiRouterFilterEntry::setOriginTrack(const ZynthboxBasics::Track& originTrack)
{
    if (m_originTrack != originTrack) {
        m_originTrack = originTrack;
        Q_EMIT originTrackChanged();
    }
}

ZynthboxBasics::Slot MidiRouterFilterEntry::originSlot() const
{
    return m_originSlot;
}

void MidiRouterFilterEntry::setOriginSlot(const ZynthboxBasics::Slot& originSlot)
{
    if (m_originSlot != originSlot) {
        m_originSlot = originSlot;
        Q_EMIT originSlotChanged();
    }
}

int MidiRouterFilterEntry::valueMinimum() const
{
    return m_valueMinimum;
}

void MidiRouterFilterEntry::setValueMinimum(const int& valueMinimum)
{
    if (m_valueMinimum != valueMinimum) {
        m_valueMinimum = valueMinimum;
        Q_EMIT valueMinimumChanged();
        if (m_valueMinimum > m_valueMaximum) {
            setValueMaximum(m_valueMinimum);
        }
    }
}

int MidiRouterFilterEntry::valueMaximum() const
{
    return m_valueMaximum;
}

void MidiRouterFilterEntry::setValueMaximum(const int& valueMaximum)
{
    if (m_valueMaximum != valueMaximum) {
        m_valueMaximum = valueMaximum;
        Q_EMIT valueMaximumChanged();
        if (m_valueMinimum > m_valueMaximum) {
            setValueMinimum(m_valueMaximum);
        }
    }
}

QList<MidiRouterFilterEntryRewriter *> MidiRouterFilterEntry::rewriteRules() const
{
    return m_rewriteRules;
}

MidiRouterFilterEntryRewriter * MidiRouterFilterEntry::addRewriteRule(const int& index)
{
    MidiRouterFilterEntryRewriter *newRule = new MidiRouterFilterEntryRewriter(this);
    connect(newRule, &MidiRouterFilterEntryRewriter::descripionChanged, this, &MidiRouterFilterEntry::descripionChanged);
    // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
    auto tempList = m_rewriteRules;
    if (-1 < index && index < tempList.length()) {
        tempList.insert(index, newRule);
    } else {
        tempList.append(newRule);
    }
    m_rewriteRules = tempList;
    Q_EMIT rewriteRulesChanged();
    return newRule;
}

void MidiRouterFilterEntry::deleteRewriteRule(const int &index)
{
    if (-1 < index && index < m_rewriteRules.count()) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_rewriteRules;
        MidiRouterFilterEntryRewriter *oldRule = tempList.takeAt(index);
        m_rewriteRules = tempList;
        Q_EMIT rewriteRulesChanged();
        QTimer::singleShot(1000, this, [oldRule](){ oldRule->deleteLater(); });
    }
}

int MidiRouterFilterEntry::indexOf(MidiRouterFilterEntryRewriter* rule) const
{
    return m_rewriteRules.indexOf(rule);
}

void MidiRouterFilterEntry::swapRewriteRules(MidiRouterFilterEntryRewriter* swapThis, MidiRouterFilterEntryRewriter* withThis)
{
    const int firstPosition{m_rewriteRules.indexOf(swapThis)};
    const int secondPosition{m_rewriteRules.indexOf(withThis)};
    if (firstPosition > -1 && secondPosition > -1) {
        // Operating on a temporary copy of the list and reassigning it back, as changing the list is not threadsafe, but replacing it entirely is (and more costly, but that doesn't matter to us here)
        auto tempList = m_rewriteRules;
        tempList.swapItemsAt(firstPosition, secondPosition);
        m_rewriteRules = tempList;
        Q_EMIT rewriteRulesChanged();
    }
}

QString MidiRouterFilterEntry::description() const
{
    QString description;
    const MidiRouterFilter::Direction direction{qobject_cast<MidiRouterFilter*>(parent())->direction()};
    if (direction == MidiRouterFilter::InputDirection) {
        QString firstEvent;
        switch (m_requiredBytes) {
            case 1:
                firstEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Minimum).getDescription().toRawUTF8());
                break;
            case 2:
                firstEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Minimum, m_byte2Minimum).getDescription().toRawUTF8());
                break;
            case 3:
                firstEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Minimum, m_byte2Minimum, m_byte3Minimum).getDescription().toRawUTF8());
                break;
            default:
                firstEvent = QLatin1String{"What in the world, a message with %1 bytes?!"}.arg(m_requiredBytes);
                break;
        }
        if (m_requireRange) {
            QString secondEvent;
            switch (m_requiredBytes) {
                case 1:
                    secondEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Maximum).getDescription().toRawUTF8());
                    break;
                case 2:
                    secondEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Maximum, m_byte2Maximum).getDescription().toRawUTF8());
                    break;
                case 3:
                    secondEvent = QString::fromUtf8(juce::MidiMessage(m_byte1Maximum, m_byte2Maximum, m_byte3Maximum).getDescription().toRawUTF8());
                    break;
                default:
                    secondEvent = QLatin1String{"What in the world, a message with %1 bytes?!"}.arg(m_requiredBytes);
                    break;
            }
            description = QString{"From %1 to %2"}.arg(firstEvent).arg(secondEvent);
        } else {
            description = firstEvent;
        }
        // TODO This would benefit from k18n's plural understanding...
        if (m_rewriteRules.count() == 0) {
            description = QString("%1 with no rewrite rules").arg(description);
        } else if (m_rewriteRules.count() == 1) {
            description = QString("%1 with 1 rewrite rule").arg(description);
        } else {
            description = QString("%1 with %2 rewrite rules").arg(description).arg(m_rewriteRules.count());
        }
    } else {
        if (m_valueMinimum == m_valueMaximum) {
            description = CUIAHelper::instance()->describe(m_cuiaEvent, m_originTrack, m_originSlot, m_valueMinimum);
        } else {
            description = CUIAHelper::instance()->describe(m_cuiaEvent, m_originTrack, m_originSlot, m_valueMinimum, m_valueMaximum);
        }
        if (m_rewriteRules.count() == 0) {
            description = QString("%1 with no rewrite rules (no midi events will be sent to the device)").arg(description);
        } else {
            description = QString("%1 with %2 rewrite rules").arg(description).arg(m_rewriteRules.count());
        }
    }
    return description;
}
