#pragma once

#include "RNBOWrapper.h"
#include "RNBO.h"
#include "MetasoundPrimitives.h"
#include "Internationalization/Text.h"
#include <unordered_map>

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

        static std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> InputFloat(const RNBO::Json& desc) {
            std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> params;

            for (auto& p: desc["parameters"]) {
                if (p["type"].get<std::string>().compare("ParameterTypeNumber") != 0) {
                    continue;
                }

                if (p.contains("visible") && p["visible"].get<bool>() == false) {
                    continue;
                }

                RNBO::ParameterIndex index = static_cast<RNBO::ParameterIndex>(p["index"].get<int>());
                std::string name = p["name"].get<std::string>();
                std::string displayName = p["displayName"].get<std::string>();
                std::string id = p["paramId"].get<std::string>();
                float initialValue = p["initialValue"].get<float>();
                if (displayName.size() == 0) {
                    displayName = name;
                }
                params.emplace(
                        index, 
                        FRNBOMetasoundParam(FString(name.c_str()), FText::AsCultureInvariant(id.c_str()), FText::AsCultureInvariant(displayName.c_str()), initialValue)
                        );
            }

            return params;
        }

        static std::vector<FRNBOMetasoundParam> InputAudio(const RNBO::Json& desc) {
            //TODO param~
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


#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FRNBOOperator"

//https://en.cppreference.com/w/cpp/language/template_parameters
template<const RNBO::Json& desc, RNBO::PatcherFactoryFunctionPtr(*FactoryFunction)(RNBO::PlatformInterface* platformInterface)>
class FRNBOOperator : public TExecutableOperator<FRNBOOperator<desc, FactoryFunction>>
{
    private:
        RNBO::CoreObject CoreObject;
        RNBO::ParameterEventInterfaceUniquePtr ParamInterface;

        int32 mNumFrames;

        std::unordered_map<RNBO::ParameterIndex, Metasound::FFloatReadRef> mInputFloatParams;

        std::vector<Metasound::FAudioBufferReadRef> mInputAudioParams;
        std::vector<const float *> mInputAudioBuffers;

        std::vector<Metasound::FAudioBufferWriteRef> mOutputAudioParams;
        std::vector<float *> mOutputAudioBuffers;

        TOptional<FTransportReadRef> Transport;

        double LastTransportBeatTime = -1.0;
        float LastTransportBPM = 0.0f;
        bool LastTransportRun = false;
        int32 LastTransportNum = 0;
        int32 LastTransportDen = 0;

        static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& InputFloatParams() {
            static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputFloat(desc);
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

        static const bool WithTransport() {
            //TODO config based on description
            return true;
        }

    public:
            static const Metasound::FNodeClassMetadata& GetNodeInfo() {
                auto InitNodeInfo = []() -> Metasound::FNodeClassMetadata
                {
                    auto meta = desc["meta"];
                    std::string classname = meta["rnboobjname"];
                    std::string name;
                    std::string description = "RNBO Generated";
                    std::string category = "RNBO";

                    if (meta.contains("name")) {
                        name = meta["name"];
                    }
                    if (name.size() == 0 || name.compare("untitled") == 0) {
                        name = classname;
                    }
                    //TODO description and category from meta?

                    FName ClassName(FString(classname.c_str()));
                    FText DisplayName = FText::AsCultureInvariant(name.c_str());
                    FText Description = FText::AsCultureInvariant(description.c_str());
                    FText Category = FText::AsCultureInvariant(category.c_str());

                    Metasound::FNodeClassMetadata Info;
                    Info.ClassName = { TEXT("UE"), ClassName, TEXT("Audio") };
                    Info.MajorVersion = 1;
                    Info.MinorVersion = 1;
                    Info.DisplayName = DisplayName;
                    Info.Description = Description;
                    Info.Author = PluginAuthor;
                    Info.PromptIfMissing = PluginNodeMissingPrompt;
                    Info.DefaultInterface = GetVertexInterface();
                    Info.CategoryHierarchy = { Category };
                    return Info;
                };

                static const Metasound::FNodeClassMetadata Info = InitNodeInfo();

                return Info;
            }

            static const Metasound::FVertexInterface& GetVertexInterface() {
                auto Init = []() -> Metasound::FVertexInterface
                {
                    Metasound::FInputVertexInterface inputs;

                    for (auto& it: InputFloatParams()) {
                        auto& p = it.second;
                        inputs.Add(TInputDataVertex<float>(p.Name(), p.MetaData(), p.InitialValue()));
                    }

                    if (WithTransport()) {
                        inputs.Add(TInputDataVertex<Metasound::FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));
                    }

                    for (auto& p: InputAudioParams()) {
                        inputs.Add(TInputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
                    }

                    Metasound::FOutputVertexInterface outputs;

                    for (auto& p: OutputAudioParams()) {
                        outputs.Add(TOutputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
                    }

                    Metasound::FVertexInterface interface(inputs, outputs);
                    return interface;
                };
                static const Metasound::FVertexInterface Interface = Init();

                return Interface;
            }

            static TUniquePtr<IOperator> CreateOperator(const Metasound::FCreateOperatorParams& InParams, Metasound::FBuildErrorArray& OutErrors) {
                const Metasound::FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
                const Metasound::FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

                return MakeUnique<FRNBOOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
            }

            FRNBOOperator(
                    const Metasound::FCreateOperatorParams& InParams,
                    const Metasound::FOperatorSettings& InSettings,
                    const Metasound::FDataReferenceCollection& InputCollection,
                    const Metasound::FInputVertexInterface& InputInterface,
                    Metasound::FBuildErrorArray& OutErrors
                    ) :
                CoreObject(RNBO::UniquePtr<RNBO::PatcherInterface>(FactoryFunction(RNBO::Platform::get())())),
                mNumFrames(InSettings.GetNumFramesPerBlock())

                    {
                        CoreObject.prepareToProcess(InSettings.GetSampleRate(), InSettings.GetNumFramesPerBlock());
                        //all params are handled in the audio thread
                        ParamInterface = CoreObject.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, nullptr);

                        for (auto& it: InputFloatParams()) {
                            mInputFloatParams.emplace(it.first, InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, it.second.Name(), InSettings));
                        }

                        for (auto& p: InputAudioParams()) {
                            mInputAudioParams.emplace_back(InputCollection.GetDataReadReferenceOrConstruct<Metasound::FAudioBuffer>(p.Name(), InSettings));
                            mInputAudioBuffers.emplace_back(nullptr);
                        }

                        for (auto& p: OutputAudioParams()) {
                            mOutputAudioParams.emplace_back(Metasound::FAudioBufferWriteRef::CreateNew(InSettings));
                            mOutputAudioBuffers.emplace_back(nullptr);
                        }
                        if (WithTransport()) {
                            Transport = {InputCollection.GetDataReadReferenceOrConstruct<FTransport>(METASOUND_GET_PARAM_NAME(ParamTransport))};
                        }
                    }

            virtual void BindInputs(Metasound::FInputVertexInterfaceData& InOutVertexData) override
            {
                {
                    auto lookup = InputFloatParams();
                    for (auto& [index, p]: mInputFloatParams) {
                        auto it = lookup.find(index);
                        //should never fail
                        if (it != lookup.end()) {
                            InOutVertexData.BindReadVertex(it->second.Name(), p);
                        }
                    }
                }
                if (Transport.IsSet()) {
                    InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport.GetValue());
                }
                {
                    auto lookup = InputAudioParams();
                    for (size_t i = 0; i < mInputAudioParams.size(); i++) {
                        auto& p = lookup[i];
                        InOutVertexData.BindReadVertex(p.Name(), mInputAudioParams[i]);
                    }
                }
            }

            virtual void BindOutputs(Metasound::FOutputVertexInterfaceData& InOutVertexData) override
            {
                {
                    auto lookup = OutputAudioParams();
                    for (size_t i = 0; i < mOutputAudioParams.size(); i++) {
                        auto& p = lookup[i];
                        InOutVertexData.BindReadVertex(p.Name(), mOutputAudioParams[i]);
                    }
                }
            }

            void Execute()
            {
                //setup audio buffers
                for (size_t i = 0; i < mInputAudioBuffers.size(); i++) {
                    mInputAudioBuffers[i] = mInputAudioParams[i]->GetData();
                }

                for (size_t i = 0; i < mOutputAudioBuffers.size(); i++) {
                    mOutputAudioBuffers[i] = mOutputAudioParams[i]->GetData();
                }

                if (Transport.IsSet()) {
                    auto& transport = Transport.GetValue();
                    double btime = std::max(0.0, transport->GetBeatTime().GetSeconds()); //not actually seconds
                    if (LastTransportBeatTime != btime)
                    { 
                        LastTransportBeatTime = btime;
                        RNBO::BeatTimeEvent event(0, btime);

                        ParamInterface->scheduleEvent(event);
                    }

                    float bpm = std::max(0.0f, transport->GetBPM());
                    if (LastTransportBPM != bpm)
                    { 
                        LastTransportBPM = bpm;

                        RNBO::TempoEvent event(0, bpm);
                        ParamInterface->scheduleEvent(event);
                    }

                    if (LastTransportRun != transport->GetRun())
                    { 
                        LastTransportRun = transport->GetRun();
                        RNBO::TransportEvent event(0, LastTransportRun ? RNBO::TransportState::RUNNING : RNBO::TransportState::STOPPED);
                        ParamInterface->scheduleEvent(event);
                    }

                    auto timesig = transport->GetTimeSig();
                    auto num = std::get<0>(timesig);
                    auto den = std::get<1>(timesig);
                    if (LastTransportNum != num || LastTransportDen != den)
                    { 
                        LastTransportNum = num;
                        LastTransportDen = den;

                        RNBO::TimeSignatureEvent event(0, num, den);
                        ParamInterface->scheduleEvent(event);
                    }
                }

                for (auto& [index, p]: mInputFloatParams) {
                    double v = static_cast<double>(*p);
                    if (v != ParamInterface->getParameterValue(index)) {
                        ParamInterface->setParameterValue(index, v);
                    }
                }

                CoreObject.process(static_cast<const float * const *>(mInputAudioBuffers.data()), mInputAudioBuffers.size(), mOutputAudioBuffers.data(), mOutputAudioBuffers.size(), mNumFrames);
            }
};

#undef LOCTEXT_NAMESPACE
