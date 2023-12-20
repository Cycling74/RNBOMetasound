#include "RNBOMIDI.h"
#include "RNBONode.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

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

class FMakeNoteOperator : public TExecutableOperator<FMakeNoteOperator>
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
            outputs.Add(TOutputDataVertex<RNBOMetasound::FMIDIBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMakeNoteMIDI)));
            FVertexInterface Interface(inputs, outputs);

            return Interface;
        };

        static const FVertexInterface Interface = InitVertexInterface();
        return Interface;
    }

    static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
    {
        const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
        const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FMakeNoteOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FMakeNoteOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : SampleRate(InSettings.GetSampleRate())

        , Trigger(InputCollection.GetDataReadReferenceOrConstruct<Metasound::FTrigger>(METASOUND_GET_PARAM_NAME(ParamMakeNoteTrig), InSettings))

        , NoteNum(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamMakeNoteNote), InSettings))
        , NoteVel(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamMakeNoteVel), InSettings))
        , NoteOffVel(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamMakeNoteOffVel), InSettings))
        , NoteChan(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamMakeNoteChan), InSettings))

        , NoteDur(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(ParamMakeNoteDur), InSettings))

        , MIDIOut(FMIDIBufferWriteRef::CreateNew(InSettings))
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
        MIDIOut->AdvanceBlock();

        const auto num = Trigger->NumTriggeredInBlock();

        if (num > 0) {
            const auto note = static_cast<uint8_t>(std::clamp(*NoteNum, 0, 127));
            const auto vel = static_cast<uint8_t>(std::clamp(*NoteVel, 1, 127));
            const auto offvel = static_cast<uint8_t>(std::clamp(*NoteOffVel, 0, 127));
            const auto chan = static_cast<uint8_t>(std::clamp(*NoteChan, 0, 15));
            const int32 dur = std::max(static_cast<int32>(ceil(SampleRate.GetSeconds() * NoteDur->GetSeconds())), 1);

            for (auto i = 0; i < num; i++) {
                auto start = (*Trigger)[i];
                MIDIOut->PushNote(start, dur, chan, note, vel, offvel);
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

    FMIDIBufferWriteRef MIDIOut;
};

#undef LOCTEXT_NAMESPACE

using MakeNoteOperatorNode = FGenericNode<FMakeNoteOperator>;
METASOUND_REGISTER_NODE(MakeNoteOperatorNode)
} // namespace
