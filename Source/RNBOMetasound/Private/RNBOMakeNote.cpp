#include "RNBONode.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMidi/MidiVoiceId.h"

namespace {

using namespace Metasound;
using namespace RNBOMetasound;

#define LOCTEXT_NAMESPACE "FRNBOMakeNote"

namespace {
METASOUND_PARAM(ParamMakeNoteTrig, "Trigger", "Create a note.")
METASOUND_PARAM(ParamMakeNoteNote, "Note", "The note number (0-127).")
METASOUND_PARAM(ParamMakeNoteVel, "Velocity", "The note velocity (1-127).")
METASOUND_PARAM(ParamMakeNoteOffVel, "Off Velocity", "The note-off velocity (0-127).")
METASOUND_PARAM(ParamMakeNoteChan, "Channel", "The note channel (0-15).")
METASOUND_PARAM(ParamMakeNoteDur, "Duration", "The note duration (seconds).")

METASOUND_PARAM(ParamMakeNoteMIDI, "MIDI", "The resultant MIDI.")
} // namespace

class FMakeNoteOperator : public TExecutableOperator<FMakeNoteOperator>, public FMidiVoiceGeneratorBase
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.ClassName = { TEXT("UE"), TEXT("MakeNote"), TEXT("Audio") };
            Info.MajorVersion = 1;
            Info.MinorVersion = 0;
            Info.DisplayName = LOCTEXT("Metasound_MakeNoteDisplayName", "Make Note");
            Info.Description = LOCTEXT("Metasound_MakeNoteNodeDescription", "MIDI Note generator.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_MakeNoteNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;

            inputs.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteTrig)));

            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteNote), 63));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteVel), 100));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteOffVel), 0));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteChan), 0));

            inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteDur), 0.05f));

            FOutputVertexInterface outputs;
            outputs.Add(TOutputDataVertex<HarmonixMetasound::FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteMIDI)));
            FVertexInterface Interface(inputs, outputs);

            return Interface;
        };

        static const FVertexInterface Interface = InitVertexInterface();
        return Interface;
    }

    static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
    {
        const FInputVertexInterfaceData& InputCollection = InParams.InputData;
        const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FMakeNoteOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutResults);
    }

    FMakeNoteOperator(
        const FBuildOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FInputVertexInterfaceData& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildResults& OutResults)
        : FMidiVoiceGeneratorBase()

        , SampleRate(InSettings.GetSampleRate())
        , Trigger(InputCollection.GetOrConstructDataReadReference<Metasound::FTrigger>(METASOUND_GET_PARAM_NAME(ParamMakeNoteTrig), InSettings))

        , NoteNum(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamMakeNoteNote), InSettings))
        , NoteVel(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamMakeNoteVel), InSettings))
        , NoteOffVel(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamMakeNoteOffVel), InSettings))
        , NoteChan(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamMakeNoteChan), InSettings))

        , NoteDur(InputCollection.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(ParamMakeNoteDur), InSettings))

        , MIDIOut(HarmonixMetasound::FMidiStreamWriteRef::CreateNew())
    {
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteTrig), Trigger);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteNote), NoteNum);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteVel), NoteVel);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteOffVel), NoteOffVel);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteChan), NoteChan);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteDur), NoteDur);
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMakeNoteMIDI), MIDIOut);
    }

    void Execute()
    {
        MIDIOut->PrepareBlock();

        const auto num = Trigger->NumTriggeredInBlock();

        uint32 id = 0; //XXX do we need some other ID?
        if (num > 0) {
            const auto note = static_cast<int8>(std::clamp(*NoteNum, 0, 127));
            const auto vel = static_cast<int8>(std::clamp(*NoteVel, 1, 127));
            const auto offvel = static_cast<int8>(std::clamp(*NoteOffVel, 0, 127));
            const auto chan = static_cast<int8>(std::clamp(*NoteChan, 0, 15));
            const int32 dur = std::max(static_cast<int32>(ceil(SampleRate.GetSeconds() * NoteDur->GetSeconds())), 1);

            for (auto i = 0; i < num; i++) {
                auto start = (*Trigger)[i];
                HarmonixMetasound::FMidiStreamEvent noteon(this, FMidiMsg::CreateNoteOn(chan, note, vel));
                HarmonixMetasound::FMidiStreamEvent noteoff(this, FMidiMsg(chan | 0x80, note, offvel));

                // as per rec from Harmonix, could eventually make this user configurable with an input
                noteon.TrackIndex = 1;
                noteoff.TrackIndex = 1;

                noteoff.BlockSampleFrameIndex = dur;
                MIDIOut->AddMidiEvent(noteon);
                MIDIOut->AddMidiEvent(noteoff);
            }
        }
    }

  private:
    FTime SampleRate;

    FTriggerReadRef Trigger;

    FInt32ReadRef NoteNum;
    FInt32ReadRef NoteVel;
    FInt32ReadRef NoteOffVel;
    FInt32ReadRef NoteChan;

    FTimeReadRef NoteDur;

    HarmonixMetasound::FMidiStreamWriteRef MIDIOut;
};

#undef LOCTEXT_NAMESPACE

using MakeNoteOperatorNode = FGenericNode<FMakeNoteOperator>;
METASOUND_REGISTER_NODE(MakeNoteOperatorNode)
} // namespace
