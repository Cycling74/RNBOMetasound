#include "RNBOTransport.h"
#include "RNBONode.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundDataTypeRegistrationMacro.h"

// Disable constructor pins of triggers
template <>
struct Metasound::TEnableConstructorVertex<RNBOMetasound::FTransport>
{
    static constexpr bool Value = false;
};

// Disable arrays of triggers
template <>
struct Metasound::TEnableAutoArrayTypeRegistration<RNBOMetasound::FTransport>
{
    static constexpr bool Value = false;
};

// Disable auto-conversions based on FTransport implicit converters
template <typename ToDataType>
struct Metasound::TEnableAutoConverterNodeRegistration<RNBOMetasound::FTransport, ToDataType>
{
    static constexpr bool Value = false;
};

template <typename FromDataType>
struct Metasound::TEnableAutoConverterNodeRegistration<FromDataType, RNBOMetasound::FTransport>
{
    static constexpr bool Value = false;
};

namespace RNBOMetasound {
FTransport::FTransport(bool bRun, float bBPM, int32 bTimeSigNum, int32 bTimeSigDen)
    : BeatTime(0.0)
    , Run(bRun)
    , BPM(std::max(0.0f, bBPM))
    , TimeSig(std::make_tuple(std::max(1, bTimeSigNum), std::max(1, bTimeSigDen)))
{
}

FTime FTransport::GetBeatTime() const
{
    return BeatTime;
}
bool FTransport::GetRun() const
{
    return Run;
}
float FTransport::GetBPM() const
{
    return BPM;
}
std::tuple<int32, int32> FTransport::GetTimeSig() const
{
    return TimeSig;
}

void FTransport::SetBeatTime(FTime v)
{
    BeatTime = v;
}
void FTransport::SetRun(bool v)
{
    Run = v;
}
void FTransport::SetBPM(float v)
{
    BPM = std::max(0.0f, v);
}
void FTransport::SetTimeSig(std::tuple<int32, int32> v)
{
    TimeSig = std::make_tuple(std::max(1, std::get<0>(v)), std::max(1, std::get<1>(v)));
}
} // namespace RNBOMetasound

REGISTER_METASOUND_DATATYPE(RNBOMetasound::FTransport, "Transport", ::Metasound::ELiteralType::Boolean)

#define LOCTEXT_NAMESPACE "FRNBOTransport"

namespace {
using namespace Metasound;
using namespace RNBOMetasound;

namespace {
METASOUND_PARAM(ParamTransportBPM, "BPM", "The tempo of the transport in beats per minute.")
METASOUND_PARAM(ParamTransportRun, "Run", "The run state of the transport.")
METASOUND_PARAM(ParamTransportNum, "Numerator", "The transport time signature numerator.")
METASOUND_PARAM(ParamTransportDen, "Denominator", "The transport time signature denominator.")
} // namespace

class FTransportOperator : public TExecutableOperator<FTransportOperator>
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.ClassName = { TEXT("UE"), TEXT("Transport"), TEXT("Audio") };
            Info.MajorVersion = 1;
            Info.MinorVersion = 0;
            Info.DisplayName = LOCTEXT("Metasound_TransportDisplayName", "Transport");
            Info.Description = LOCTEXT("Metasound_TransportNodeDescription", "Transport generator.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_TransportNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;
            inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBPM), 120.0f));
            inputs.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportRun), true));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportNum), 4));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportDen), 4));

            FOutputVertexInterface outputs;
            outputs.Add(TOutputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));

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

        return MakeUnique<FTransportOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FTransportOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : BlockSize(InSettings.GetNumFramesPerBlock())
        , SampleRate(static_cast<double>(InSettings.GetSampleRate()))
        , BlockPeriod(InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
        , PeriodMul(8.0 / 480.0 * InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
        , CurTransportBeatTime(0.0)
        , TransportBPM(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
        , TransportRun(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
        , TransportNum(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
        , TransportDen(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
        , Transport(FTransportWriteRef::CreateNew(false))
    {
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportNum), TransportNum);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportDen), TransportDen);
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
    }

    void Execute()
    {
        FTransport Cur(*TransportRun, *TransportBPM, *TransportNum, *TransportDen);
        if (Cur.GetBPM()) {
            FTime offset(PeriodMul * static_cast<double>(Cur.GetBPM()));
            CurTransportBeatTime += offset;
        }
        Cur.SetBeatTime(CurTransportBeatTime);
        *Transport = Cur;
    }

  private:
    FSampleCount BlockSize;
    FSampleRate SampleRate;
    FTime BlockPeriod;
    double PeriodMul;

    FTime CurTransportBeatTime;
    float LastTransportBPM = 0.0f;
    bool LastTransportRun = false;

    FFloatReadRef TransportBPM;
    FBoolReadRef TransportRun;
    FInt32ReadRef TransportNum;
    FInt32ReadRef TransportDen;

    FTransportWriteRef Transport;
};

using TransportOperatorNode = FGenericNode<FTransportOperator>;
METASOUND_REGISTER_NODE(TransportOperatorNode)
} // namespace

#undef LOCTEXT_NAMESPACE
