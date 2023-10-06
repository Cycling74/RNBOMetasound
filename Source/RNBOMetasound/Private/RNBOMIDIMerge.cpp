#include "RNBOMIDI.h"
#include "RNBONode.h"
#include <vector>
#include <array>

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"
#include "Internationalization/Text.h"

namespace {

using namespace Metasound;
using namespace RNBOMetasound;

#define LOCTEXT_NAMESPACE "FRNBOMIDIMerge"

namespace {

const std::array<const TCHAR*, 8> InputNames = {
    TEXT("In 1"),
    TEXT("In 2"),
    TEXT("In 3"),
    TEXT("In 4"),
    TEXT("In 5"),
    TEXT("In 6"),
    TEXT("In 7"),
    TEXT("In 8"),
};
const std::array<const FText, 8> InputToolTips = {
    LOCTEXT("ParamMIDIMergeIn1ToolTip", "MIDI Input 1"),
    LOCTEXT("ParamMIDIMergeIn2ToolTip", "MIDI Input 2"),
    LOCTEXT("ParamMIDIMergeIn3ToolTip", "MIDI Input 3"),
    LOCTEXT("ParamMIDIMergeIn4ToolTip", "MIDI Input 4"),
    LOCTEXT("ParamMIDIMergeIn5ToolTip", "MIDI Input 5"),
    LOCTEXT("ParamMIDIMergeIn6ToolTip", "MIDI Input 6"),
    LOCTEXT("ParamMIDIMergeIn7ToolTip", "MIDI Input 7"),
    LOCTEXT("ParamMIDIMergeIn8ToolTip", "MIDI Input 8"),
};
const std::array<const FText, 8> InputDisplayNames = {
    LOCTEXT("ParamMIDIMergeIn1DisplayName", "In 1"),
    LOCTEXT("ParamMIDIMergeIn2DisplayName", "In 2"),
    LOCTEXT("ParamMIDIMergeIn3DisplayName", "In 3"),
    LOCTEXT("ParamMIDIMergeIn4DisplayName", "In 4"),
    LOCTEXT("ParamMIDIMergeIn5DisplayName", "In 5"),
    LOCTEXT("ParamMIDIMergeIn6DisplayName", "In 6"),
    LOCTEXT("ParamMIDIMergeIn7DisplayName", "In 7"),
    LOCTEXT("ParamMIDIMergeIn8DisplayName", "In 8"),
};

METASOUND_PARAM(ParamMIDIMergeMIDI, "Out", "The merged MIDI.")

} // namespace

template <size_t N>
class FMIDIMergeOperator : public TExecutableOperator<FMIDIMergeOperator<N>>
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.MajorVersion = 1;
            Info.MinorVersion = 0;

            std::string classname = std::string("MIDIMerge") + std::to_string(N);
            std::string name = std::string("MIDI Merge ") + std::to_string(N);

            FName ClassName(FString(classname.c_str()));
            FText DisplayName = FText::AsCultureInvariant(name.c_str());

            Info.ClassName = { TEXT("UE"), ClassName, TEXT("Audio") };
            Info.DisplayName = DisplayName;
            Info.Description = LOCTEXT("Metasound_MIDIMergeNodeDescription", "MIDI Merge Utility.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_MIDIMergeNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;

            for (size_t i = 0; i < N; i++) {
                inputs.Add(TInputDataVertex<RNBOMetasound::FMIDIBuffer>(InputNames[i], { InputToolTips[i], InputDisplayNames[i] }));
            }

            FOutputVertexInterface outputs;
            outputs.Add(TOutputDataVertex<RNBOMetasound::FMIDIBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMIDIMergeMIDI)));
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

        return MakeUnique<FMIDIMergeOperator<N>>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FMIDIMergeOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : MIDIOut(FMIDIBufferWriteRef::CreateNew(InSettings))
    {
        for (size_t i = 0; i < N; i++) {
            MIDIIn.push_back(InputCollection.GetDataReadReferenceOrConstruct<FMIDIBuffer>(InputNames[i], InSettings));
        }
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        for (size_t i = 0; i < N; i++) {
            InOutVertexData.BindReadVertex(InputNames[i], MIDIIn[i]);
        }
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMIDIMergeMIDI), MIDIOut);
    }

    void Execute()
    {
        MIDIOut->AdvanceBlock();
        for (auto& m : MIDIIn) {
            for (int32 i = 0; i < m->NumInBlock(); i++) {
                MIDIOut->Push((*m)[i]);
            }
        }
    }

  private:
    FMIDIBufferWriteRef MIDIOut;
    std::vector<FMIDIBufferReadRef> MIDIIn;
};

#undef LOCTEXT_NAMESPACE

template class FMIDIMergeOperator<2>;
using MIDIMergeOperatorNode2 = FGenericNode<FMIDIMergeOperator<2>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode2)

template class FMIDIMergeOperator<3>;
using MIDIMergeOperatorNode3 = FGenericNode<FMIDIMergeOperator<3>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode3)

template class FMIDIMergeOperator<4>;
using MIDIMergeOperatorNode4 = FGenericNode<FMIDIMergeOperator<4>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode4)

template class FMIDIMergeOperator<5>;
using MIDIMergeOperatorNode5 = FGenericNode<FMIDIMergeOperator<5>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode5)

template class FMIDIMergeOperator<6>;
using MIDIMergeOperatorNode6 = FGenericNode<FMIDIMergeOperator<6>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode6)

template class FMIDIMergeOperator<7>;
using MIDIMergeOperatorNode7 = FGenericNode<FMIDIMergeOperator<7>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode7)

template class FMIDIMergeOperator<8>;
using MIDIMergeOperatorNode8 = FGenericNode<FMIDIMergeOperator<8>>;
METASOUND_REGISTER_NODE(MIDIMergeOperatorNode8)
} // namespace
