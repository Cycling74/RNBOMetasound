#include "RNBOTransport.h"
#include "RNBONode.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"

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
METASOUND_PARAM(ParamTransport, "Transport", "The transport.")

METASOUND_PARAM(ParamTransportLatch, "Latch", "Latch the input values.")
METASOUND_PARAM(ParamTransportBPM, "BPM", "The tempo of the transport in beats per minute.")
METASOUND_PARAM(ParamTransportRun, "Run", "The run state of the transport.")
METASOUND_PARAM(ParamTransportNum, "Numerator", "The transport time signature numerator.")
METASOUND_PARAM(ParamTransportDen, "Denominator", "The transport time signature denominator.")

METASOUND_PARAM(ParamTransportBeatTime, "BeatTime", "The transport beat time.")

METASOUND_PARAM(ParamTransportBar, "Bar", "The transport bar.")
METASOUND_PARAM(ParamTransportBeat, "Beat", "The transport beat.")
METASOUND_PARAM(ParamTransportTick, "Tick", "The transport tick.")

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

    static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
    {
        const FInputVertexInterfaceData& InputCollection = InParams.InputData;
        const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FTransportOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutResults);
    }

    FTransportOperator(
        const FBuildOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FInputVertexInterfaceData& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildResults& OutResults)
        : PeriodMul(8.0 / 480.0 * static_cast<double>(InSettings.GetNumFramesPerBlock()) / static_cast<double>(InSettings.GetSampleRate()))
        , CurTransportBeatTime(0.0)
        , TransportBPM(InputCollection.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
        , TransportRun(InputCollection.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
        , TransportNum(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
        , TransportDen(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
        , TransportBeatTime(InputCollection.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), InSettings))
        , TransportSeek(InputCollection.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportSeek), InSettings))
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

    static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
    {
        const FInputVertexInterfaceData& InputCollection = InParams.InputData;
        const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FGlobalTransportOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutResults);
    }

    FGlobalTransportOperator(
        const FBuildOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FInputVertexInterfaceData& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildResults& OutResults)
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

    static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
    {
        const FInputVertexInterfaceData& InputCollection = InParams.InputData;
        const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FGlobalTransportControlOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutResults);
    }

    FGlobalTransportControlOperator(
        const FBuildOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FInputVertexInterfaceData& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildResults& OutResults)
        : LatchTrigger(InputCollection.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportLatch), InSettings))
        , TransportBPM(InputCollection.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
        , TransportRun(InputCollection.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
        , TransportNum(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
        , TransportDen(InputCollection.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
        , TransportBeatTime(InputCollection.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), InSettings))
        , TransportSeek(InputCollection.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTransportSeek), InSettings))
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

class FTransportGetOperator : public TExecutableOperator<FTransportGetOperator>
{
  public:
    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> FNodeClassMetadata {
            FNodeClassMetadata Info;

            Info.ClassName = { TEXT("UE"), TEXT("TransportGet"), TEXT("Audio") };
            Info.MajorVersion = 1;
            Info.MinorVersion = 0;
            Info.DisplayName = LOCTEXT("Metasound_TransportGetDisplayName", "Transport Get");
            Info.Description = LOCTEXT("Metasound_TransportGetNodeDescription", "Transport Get Utility.");
            Info.Author = PluginAuthor;
            Info.PromptIfMissing = PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { LOCTEXT("Metasound_TransportGetNodeCategory", "Utils") };

            return Info;
        };

        static const FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }
    static const FVertexInterface& GetVertexInterface()
    {
        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;
            inputs.Add(TInputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));

            FOutputVertexInterface outputs;

            outputs.Add(TOutputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportRun)));

            outputs.Add(TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBPM)));

            outputs.Add(TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBar)));
            outputs.Add(TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBeat)));
            outputs.Add(TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportTick)));

            outputs.Add(TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportNum)));
            outputs.Add(TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportDen)));

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

        return MakeUnique<FTransportGetOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FTransportGetOperator(
        const FCreateOperatorParams& InParams,
        const FOperatorSettings& InSettings,
        const FDataReferenceCollection& InputCollection,
        const FInputVertexInterface& InputInterface,
        FBuildErrorArray& OutErrors)
        : Transport(InputCollection.GetDataReadReferenceOrConstruct<FTransport>(METASOUND_GET_PARAM_NAME(ParamTransport)))
        , TransportRun(FBoolWriteRef::CreateNew(false))
        , TransportBPM(FFloatWriteRef::CreateNew(100.0))
        , TransportBar(FInt32WriteRef::CreateNew(0))
        , TransportBeat(FInt32WriteRef::CreateNew(0))
        , TransportTick(FInt32WriteRef::CreateNew(0))
        , TransportNum(FInt32WriteRef::CreateNew(4))
        , TransportDen(FInt32WriteRef::CreateNew(4))
    {
    }

    virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
    }

    virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
    {
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);

        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBar), TransportBar);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBeat), TransportBeat);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportTick), TransportTick);

        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportNum), TransportNum);
        InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportDen), TransportDen);
    }

    void Execute()
    {
        *TransportRun = Transport->GetRun();
        *TransportBPM = Transport->GetBPM();

        auto beattime = Transport->GetBeatTime().GetSeconds();
        auto [num, den] = Transport->GetTimeSig();

        double intpart;
        double fractpart = modf(beattime, &intpart);

        int64 beat = static_cast<int64>(intpart);
        int32 bar = static_cast<int32>(floor(beat / num)); // always round down

        // deal with negative
        if (bar >= 0) {
            beat = beat % num;
        }
        else {
            // may never be hit because as of this writing, transport is clamped above 0
            // ticks are always positive
            if (fractpart < 0.0) {
                fractpart = 1.0 + fractpart;
            }
            // beat should always be positive
            beat = beat - bar * num;
        }

        // bar and beat are 1 based
        *TransportBar = bar + 1;
        *TransportBeat = static_cast<int32>(beat) + 1;
        *TransportTick = static_cast<int32>(fractpart * 480.0); // rnbo and max use 480 for ppq

        *TransportNum = num;
        *TransportDen = den;
    }

  private:
    FTransportReadRef Transport;

    FBoolWriteRef TransportRun;
    FFloatWriteRef TransportBPM;

    FInt32WriteRef TransportBar;
    FInt32WriteRef TransportBeat;
    FInt32WriteRef TransportTick;

    FInt32WriteRef TransportNum;
    FInt32WriteRef TransportDen;
};

using TransportOperatorNode = FGenericNode<FTransportOperator>;
METASOUND_REGISTER_NODE(TransportOperatorNode)

using GlobalTransportOperatorNode = FGenericNode<FGlobalTransportOperator>;
METASOUND_REGISTER_NODE(GlobalTransportOperatorNode)

using GlobalTransportControlOperatorNode = FGenericNode<FGlobalTransportControlOperator>;
METASOUND_REGISTER_NODE(GlobalTransportControlOperatorNode)

using TransportGetOperatorNode = FGenericNode<FTransportGetOperator>;
METASOUND_REGISTER_NODE(TransportGetOperatorNode)
} // namespace

#undef LOCTEXT_NAMESPACE
