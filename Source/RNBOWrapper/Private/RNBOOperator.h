#pragma once

#include "RNBOWrapper.h"
#include "RNBO.h"
#include "MetasoundPrimitives.h"
#include "Internationalization/Text.h"

class FRNBOMetasoundParam {
    public:
        FRNBOMetasoundParam(const FString name, const FText tooltip, const FText displayName, float initialValue = 0.0f) :
            mName(name), mInitialValue(initialValue),
#if WITH_EDITOR
            mTooltip(tooltip), mDisplayName(displayName)
#else
                mTooltip(FText::GetEmpty()), mDisplayName(FText::GetEmpty())
#endif
                {}

        FDataVertexMetadata MetaData() const {
            return { Tooltip(), DisplayName() };
        }

        const TCHAR * Name() const { return mName.GetCharArray().GetData(); }
        const FText Tooltip() const { return mTooltip; }
        const FText DisplayName() const { return mDisplayName; }
        float InitialValue() const { return mInitialValue; }

        static std::vector<FRNBOMetasoundParam> InputFloat(const RNBO::Json& desc) {
            std::vector<FRNBOMetasoundParam> params;

            for (auto& p: desc["parameters"]) {
                std::string name = p["name"].get<std::string>();
                std::string displayName = p["displayName"].get<std::string>();
                std::string id = p["paramId"].get<std::string>();
                float initialValue = p["initialValue"].get<float>();
                if (displayName.size() == 0) {
                    displayName = name;
                }
                params.emplace_back(FString(name.c_str()), FText::AsCultureInvariant(id.c_str()), FText::AsCultureInvariant(displayName.c_str()), initialValue);
            }

            return params;
        }

        static std::vector<FRNBOMetasoundParam> InputAudio(const RNBO::Json& desc) {
            return Signals(desc, "inlets");
        }

        static std::vector<FRNBOMetasoundParam> OutputAudio(const RNBO::Json& desc) {
            return Signals(desc, "outlets");
        }

    private:

        static std::vector<FRNBOMetasoundParam> Signals(const RNBO::Json& desc, std::string selector) {
            std::vector<FRNBOMetasoundParam> params;

            const std::string sig("signal");
            for (auto& p: desc[selector]) {
                if (p.contains("type") && sig.compare(p["type"].get<std::string>()) == 0) {
                    std::string name = p["tag"].get<std::string>();
                    params.emplace_back(FString(name.c_str()), FText::AsCultureInvariant(name.c_str()), FText::AsCultureInvariant(name.c_str()), 0.0f);
                }
            }

            return params;
        }

        const FString mName;
        float mInitialValue;
        const FText mTooltip;
        const FText mDisplayName;
};


/*
namespace 
{
    const RNBO::Json desc = RNBO::Json::parse(_OPERATOR_DESC_);

    static const std::vector<FRNBOMetasoundParam>& InputFloatParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputFloat(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& InputAudioParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputAudio(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& OutputAudioParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::OutputAudio(desc);
        return Params;
    }

    _OPERATOR_PARAM_DECL_
}
*/

//UNDO
using namespace Metasound;

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FRNBOOperator"

template<
const RNBO::Json& desc,
typename FactoryFunction
//extern "C" RNBO::PatcherFactoryFunctionPtr(FactoryFunction*)(RNBO::PlatformInterface* platformInterface)
>
class FRNBOOperator : public TExecutableOperator<FRNBOOperator<desc, FactoryFunction>>
{
    private:
        RNBO::CoreObject CoreObject;
        RNBO::ParameterEventInterfaceUniquePtr ParamInterface;

#if 0
        FTransportReadRef Transport;

        double LastTransportBeatTime = -1.0;
        float LastTransportBPM = 0.0f;
        bool LastTransportRun = false;
        int32 LastTransportNum = 0;
        int32 LastTransportDen = 0;
#endif

    static const std::vector<FRNBOMetasoundParam>& InputFloatParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputFloat(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& InputAudioParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputAudio(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& OutputAudioParams() {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::OutputAudio(desc);
        return Params;
    }

    public:
            static const FNodeClassMetadata& GetNodeInfo() {
                auto InitNodeInfo = []() -> FNodeClassMetadata
                {
                    FNodeClassMetadata Info;
                    Info.ClassName = { TEXT("UE"), "_OPERATOR_DISPLAYNAME_", TEXT("Audio") };
                    Info.MajorVersion = 1;
                    Info.MinorVersion = 1;
                    Info.DisplayName = METASOUND_LOCTEXT("_OPERATOR_NAME__DisplayName", "_OPERATOR_DISPLAYNAME_");
                    Info.Description = METASOUND_LOCTEXT("_OPERATOR_NAME__Description", "_OPERATOR_DESCRIPTION_");
                    Info.Author = PluginAuthor;
                    Info.PromptIfMissing = PluginNodeMissingPrompt;
                    Info.DefaultInterface = GetVertexInterface();
                    Info.CategoryHierarchy = { LOCTEXT("_OPERATOR_NAME__Category", "_OPERATOR_CATEGORY_") };
                    return Info;
                };

                static const FNodeClassMetadata Info = InitNodeInfo();

                return Info;
            }

            static const FVertexInterface& GetVertexInterface() {
                auto Init = []() -> FVertexInterface
                {
                    FInputVertexInterface inputs;//(_OPERATOR_VERTEX_INPUTS_);

                    for (auto& p: InputFloatParams()) {
                        inputs.Add(TInputDataVertex<float>(p.Name(), p.MetaData(), p.InitialValue()));
                    }

                    for (auto& p: InputAudioParams()) {
                        inputs.Add(TInputDataVertex<FAudioBuffer>(p.Name(), p.MetaData()));
                    }

#if 0
                    inputs.Add(TInputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));
#endif
                    FOutputVertexInterface outputs;//(_OPERATOR_VERTEX_OUTPUTS_);

                    for (auto& p: OutputAudioParams()) {
                        outputs.Add(TOutputDataVertex<FAudioBuffer>(p.Name(), p.MetaData()));
                    }

                    FVertexInterface interface(inputs, outputs);
                    return interface;
                };
                static const FVertexInterface Interface = Init();

                return Interface;
            }

            static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors) {
                const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
                const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

                return MakeUnique<FRNBOOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
            }

            FRNBOOperator(
                    const FCreateOperatorParams& InParams,
                    const FOperatorSettings& InSettings,
                    const FDataReferenceCollection& InputCollection,
                    const FInputVertexInterface& InputInterface,
                    FBuildErrorArray& OutErrors
                    ) :
                CoreObject(RNBO::UniquePtr<RNBO::PatcherInterface>(FactoryFunction(RNBO::Platform::get())()))

#if 0
                , Transport(InputCollection.GetDataReadReferenceOrConstruct<FTransport>(METASOUND_GET_PARAM_NAME(ParamTransport)))
#endif

                    {
                        CoreObject.prepareToProcess(InSettings.GetSampleRate(), InSettings.GetNumFramesPerBlock());
                        //all params are handled in the audio thread
                        ParamInterface = CoreObject.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, nullptr);
                    }

            virtual FDataReferenceCollection GetInputs() const override
            {
                FDataReferenceCollection InputDataReferences;
#if 0
                InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
#endif
                //TODO _OPERATOR_GET_INPUTS_
                    return InputDataReferences;
            }

            virtual FDataReferenceCollection GetOutputs() const override
            {
                FDataReferenceCollection OutputDataReferences;
                //TODO _OPERATOR_GET_OUTPUTS_
                    return OutputDataReferences;
            }

            void UpdateParam(RNBO::ParameterIndex i, float f) {
                double v = static_cast<double>(f);
                if (v != ParamInterface->getParameterValue(i)) {
                    ParamInterface->setParameterValue(i, v);
                }
            }

            void Execute()
            {
                /*
                const std::array<const float *, _OPERATOR_AUDIO_INPUT_COUNT_> ins = { _OPERATOR_AUDIO_INPUT_INIT_ };
                std::array<float*, _OPERATOR_AUDIO_OUTPUT_COUNT_> outs = { _OPERATOR_AUDIO_OUTPUT_INIT_ };

                int32 NumFrames = _OPERATOR_AUDIO_NUMFRAMES_MEMBER_->Num();
#if 0

                double btime = std::max(0.0, Transport->GetBeatTime().GetSeconds()); //not actually seconds
                if (LastTransportBeatTime != btime)
                { 
                    LastTransportBeatTime = btime;
                    RNBO::BeatTimeEvent event(0, btime);

                    ParamInterface->scheduleEvent(event);
                }

                float bpm = std::max(0.0f, Transport->GetBPM());
                if (LastTransportBPM != bpm)
                { 
                    LastTransportBPM = bpm;

                    RNBO::TempoEvent event(0, bpm);
                    ParamInterface->scheduleEvent(event);
                }

                if (LastTransportRun != Transport->GetRun())
                { 
                    LastTransportRun = Transport->GetRun();
                    RNBO::TransportEvent event(0, LastTransportRun ? RNBO::TransportState::RUNNING : RNBO::TransportState::STOPPED);
                    ParamInterface->scheduleEvent(event);
                }

                auto timesig = Transport->GetTimeSig();
                auto num = std::get<0>(timesig);
                auto den = std::get<1>(timesig);
                if (LastTransportNum != num || LastTransportDen != den)
                { 
                    LastTransportNum = num;
                    LastTransportDen = den;

                    RNBO::TimeSignatureEvent event(0, num, den);
                    ParamInterface->scheduleEvent(event);
                }
#endif

                _OPERATOR_PARAM_UPDATE_
                    CoreObject.process(ins.data(), ins.size(), outs.data(), outs.size(), NumFrames);
                    */
            }
};

#undef LOCTEXT_NAMESPACE
