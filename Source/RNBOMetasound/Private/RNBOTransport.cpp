#include "RNBOTransport.h"
#include "RNBONode.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundDataTypeRegistrationMacro.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"

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
METASOUND_PARAM(ParamTransportLatch, "Latch", "Latch the input values.")
METASOUND_PARAM(ParamTransportBPM, "BPM", "The tempo of the transport in beats per minute.")
METASOUND_PARAM(ParamTransportRun, "Run", "The run state of the transport.")
METASOUND_PARAM(ParamTransportNum, "Numerator", "The transport time signature numerator.")
METASOUND_PARAM(ParamTransportDen, "Denominator", "The transport time signature denominator.")

METASOUND_PARAM(ParamTransportBeatTime, "BeatTime", "The transport beat time.")
METASOUND_PARAM(ParamTransportSeek, "Seek", "Read the BeatTime input and jump there.")

FCriticalSection GlobalTransportMutex;

FTime GlobalTransportBeatTime(0.0);

bool GlobalTransportRun = true;
float GlobalTransportBPM = 100.0f;
int32 GlobalTransportNum = 4.0;
int32 GlobalTransportDen = 4.0;

double GlobalTransportNextBeatTime = -1.0;
bool GlobalTransportNextRun = true;
float GlobalTransportNextBPM = 100.0f;
int32 GlobalTransportNextNum = 4.0;
int32 GlobalTransportNextDen = 4.0;

double GlobalTransportTimeLast = -1.0;
uint32 GlobalTransportWatchers = 0;

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
            inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBeatTime), 0.0f));
            inputs.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportSeek)));

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
        : PeriodMul(8.0 / 480.0 * static_cast<double>(InSettings.GetNumFramesPerBlock()) / static_cast<double>(InSettings.GetSampleRate()))
        , CurTransportBeatTime(0.0)
        , TransportBPM(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
        , TransportRun(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
        , TransportNum(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
        , TransportDen(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
        , TransportBeatTime(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), InSettings))
        , TransportSeek(InputCollection.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportSeek), InSettings))
        , Transport(FTransportWriteRef::CreateNew(false))
    {
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportNum), TransportNum);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportDen), TransportDen);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), TransportBeatTime);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportSeek), TransportSeek);
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
    }

    void Execute()
    {
        FTransport Cur(*TransportRun, *TransportBPM, *TransportNum, *TransportDen);
        // seek or advance
        if (TransportSeek->IsTriggeredInBlock()) {
            CurTransportBeatTime = FTime::FromSeconds(std::max(0.0, TransportBeatTime->GetSeconds()));
        }
        else if (Cur.GetRun()) {
            FTime offset(PeriodMul * static_cast<double>(Cur.GetBPM()));
            CurTransportBeatTime += offset;
        }
        Cur.SetBeatTime(CurTransportBeatTime);
        *Transport = Cur;
    }

  private:
    double PeriodMul;

    FTime CurTransportBeatTime;

    FFloatReadRef TransportBPM;
    FBoolReadRef TransportRun;
    FInt32ReadRef TransportNum;
    FInt32ReadRef TransportDen;

    FTimeReadRef TransportBeatTime;
    FTriggerReadRef TransportSeek;

    FTransportWriteRef Transport;
};

class FGlobalTransportOperator : public TExecutableOperator<FGlobalTransportOperator>
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.ClassName = { TEXT("UE"), TEXT("GlobalTransport"), TEXT("Audio") };
            Info.MajorVersion = 1;
            Info.MinorVersion = 0;
            Info.DisplayName = LOCTEXT("Metasound_GlobalTransportDisplayName", "Global Transport");
            Info.Description = LOCTEXT("Metasound_GlobalTransportNodeDescription", "Global Transport generator.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_GlobalTransportNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;

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

        return MakeUnique<FGlobalTransportOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FGlobalTransportOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : Transport(FTransportWriteRef::CreateNew(false))
    {
        GetEnvInfo(InParams);

        FScopeLock Guard(&GlobalTransportMutex);
        if (GlobalTransportWatchers == 0) {
            auto device = Device();
            GlobalTransportTimeLast = device ? device->GetAudioClock() : 0.0;
            UE_LOG(LogMetaSound, Verbose, TEXT("FGlobalTransportOperator setting TransportTimeLast == %f"), GlobalTransportTimeLast);
        }
        GlobalTransportWatchers++;
    }

    virtual ~FGlobalTransportOperator()
    {
        FScopeLock Guard(&GlobalTransportMutex);
        GlobalTransportWatchers--;
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
    }

    FAudioDevice* Device()
    {
        FAudioDeviceManager* manager = FAudioDeviceManager::Get();
        return manager ? manager->GetAudioDeviceRaw(AudioDeviceId) : nullptr;
    }

    void Execute()
    {
        FAudioDevice* device = Device();

        FScopeLock Guard(&GlobalTransportMutex);
        FTransport Cur(GlobalTransportRun, GlobalTransportBPM, GlobalTransportNum, GlobalTransportDen);

        if (device != nullptr)
        {
            auto c = device->GetAudioClock();
            if (c > GlobalTransportTimeLast) {

                // copy over the next
                GlobalTransportRun = GlobalTransportNextRun;
                GlobalTransportBPM = GlobalTransportNextBPM;
                GlobalTransportNum = GlobalTransportNextNum;
                GlobalTransportDen = GlobalTransportNextDen;

                Cur = { GlobalTransportRun, GlobalTransportBPM, GlobalTransportNum, GlobalTransportDen };

                // seek or advance
                if (GlobalTransportNextBeatTime >= 0.0) {
                    GlobalTransportBeatTime = FTime::FromSeconds(GlobalTransportNextBeatTime);
                    GlobalTransportNextBeatTime = -1.0;
                }
                else {
                    auto diff = c - GlobalTransportTimeLast;
                    GlobalTransportTimeLast = c;
                    if (Cur.GetRun()) {
                        double PeriodMul = diff * 8.0 / 480.0;
                        FTime offset(PeriodMul * static_cast<double>(Cur.GetBPM()));
                        GlobalTransportBeatTime += offset;
                    }
                }
            }
        }
        else {
            UE_LOG(LogMetaSound, Error, TEXT("FGlobalTransportOperator Failed to get audio device"));
        }

        Cur.SetBeatTime(GlobalTransportBeatTime);
        *Transport = Cur;
    }

    void GetEnvInfo(const IOperator::FResetParams& InParams)
    {
        using namespace Frontend;

        if (InParams.Environment.Contains<Audio::FDeviceId>(SourceInterface::Environment::DeviceID))
        {
            AudioDeviceId = InParams.Environment.GetValue<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
        }
    }

    void Reset(const IOperator::FResetParams& InParams)
    {
        GetEnvInfo(InParams);
    }

  private:
    Audio::FDeviceId AudioDeviceId = INDEX_NONE;

    FTransportWriteRef Transport;
};

class FGlobalTransportControlOperator : public TExecutableOperator<FGlobalTransportControlOperator>
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.ClassName = { TEXT("UE"), TEXT("GlobalTransportControl"), TEXT("Audio") };
            Info.MajorVersion = 1;
            Info.MinorVersion = 0;
            Info.DisplayName = LOCTEXT("Metasound_GlobalTransportControlDisplayName", "Global Transport Control");
            Info.Description = LOCTEXT("Metasound_GlobalTransportControlNodeDescription", "Global Transport Controller.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_GlobalTransportControlNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;
            inputs.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportLatch)));
            inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBPM), 120.0f));
            inputs.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportRun), true));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportNum), 4));
            inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportDen), 4));
            inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBeatTime), 0.0f));
            inputs.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportSeek)));

            FOutputVertexInterface outputs;

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

        return MakeUnique<FGlobalTransportControlOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FGlobalTransportControlOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : LatchTrigger(InputCollection.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportLatch), InSettings))
        , TransportBPM(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
        , TransportRun(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
        , TransportNum(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
        , TransportDen(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
        , TransportBeatTime(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), InSettings))
        , TransportSeek(InputCollection.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportSeek), InSettings))
    {
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportLatch), LatchTrigger);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportNum), TransportNum);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportDen), TransportDen);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), TransportBeatTime);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportSeek), TransportSeek);
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
    }

    void Execute()
    {
        auto latch = LatchTrigger->IsTriggeredInBlock();
        auto seek = TransportSeek->IsTriggeredInBlock();
        if (latch || seek)
        {
            FScopeLock Guard(&GlobalTransportMutex);

            if (seek) {
                GlobalTransportNextBeatTime = std::max(0.0, TransportBeatTime->GetSeconds());
            }

            if (latch) {
                GlobalTransportNextRun = *TransportRun;
                GlobalTransportNextBPM = std::max(*TransportBPM, 0.0f);
                GlobalTransportNextNum = std::max(*TransportNum, 1);
                GlobalTransportNextDen = std::max(*TransportDen, 1);
            }
        }
    }

  private:
    FTriggerReadRef LatchTrigger;
    FFloatReadRef TransportBPM;
    FBoolReadRef TransportRun;
    FInt32ReadRef TransportNum;
    FInt32ReadRef TransportDen;

    FTimeReadRef TransportBeatTime;
    FTriggerReadRef TransportSeek;
};

using TransportOperatorNode = FGenericNode<FTransportOperator>;
METASOUND_REGISTER_NODE(TransportOperatorNode)

using GlobalTransportOperatorNode = FGenericNode<FGlobalTransportOperator>;
METASOUND_REGISTER_NODE(GlobalTransportOperatorNode)

using GlobalTransportControlOperatorNode = FGenericNode<FGlobalTransportControlOperator>;
METASOUND_REGISTER_NODE(GlobalTransportControlOperatorNode)
} // namespace

#undef LOCTEXT_NAMESPACE
