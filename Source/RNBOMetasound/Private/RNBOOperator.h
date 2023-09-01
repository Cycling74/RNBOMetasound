#pragma once

#include "RNBONode.h"
#include "RNBOMIDI.h"
#include "RNBOTransport.h"

// visual studio warnings we're having trouble with
#pragma warning(disable : 4800 4065 4668 4804 4018 4060 4554 4018)
#include "RNBO.h"
#include "RNBO_TimeConverter.h"

#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundVertex.h"

#include "Internationalization/Text.h"
#include <unordered_map>

namespace RNBOMetasound {

#define LOCTEXT_NAMESPACE "FRNBOMetasoundModule"
METASOUND_PARAM(ParamMIDIIn, "MIDI In", "MIDI data input.")
METASOUND_PARAM(ParamMIDIOut, "MIDI Out", "MIDI data output.")
#undef LOCTEXT_NAMESPACE

using Metasound::FDataVertexMetadata;

class FRNBOMetasoundParam
{
  public:
    FRNBOMetasoundParam(const FString name, const FText tooltip, const FText displayName, float initialValue = 0.0f)
        : mName(name)
        , mInitialValue(initialValue)
        ,
#if WITH_EDITOR
        mTooltip(tooltip)
        , mDisplayName(displayName)
#else
        mTooltip(FText::GetEmpty())
        , mDisplayName(FText::GetEmpty())
#endif
    {
    }

    Metasound::FDataVertexMetadata MetaData() const
    {
        return { Tooltip(), DisplayName() };
    }

    const TCHAR* Name() const { return mName.GetCharArray().GetData(); }
    const FText Tooltip() const { return mTooltip; }
    const FText DisplayName() const { return mDisplayName; }
    float InitialValue() const { return mInitialValue; }

    static std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> InputFloat(const RNBO::Json& desc)
    {
        std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> params;

        for (auto& p : desc["parameters"]) {
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
                FRNBOMetasoundParam(FString(name.c_str()), FText::AsCultureInvariant(id.c_str()), FText::AsCultureInvariant(displayName.c_str()), initialValue));
        }

        return params;
    }

    static std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> InportTrig(const RNBO::Json& desc)
    {
        std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> params;
        for (auto& p : desc["inports"]) {
            std::string tag = p["tag"];
            std::string description = tag;
            std::string displayName = tag;
            if (p.contains("meta")) {
                // TODO get description and display name

                // TODO
                /*
                   if (p["meta"].contains("trigger") && p["meta"]["trigger"].is_bool() && p["meta"]["trigger"].get<bool>()) {
                   }
                   */
            }
            RNBO::MessageTag id = RNBO::TAG(tag.c_str());
            params.emplace(
                id,
                FRNBOMetasoundParam(FString(tag.c_str()), FText::AsCultureInvariant(description.c_str()), FText::AsCultureInvariant(displayName.c_str())));
        }

        return params;
    }

    static std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> OutportTrig(const RNBO::Json& desc)
    {
        std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> params;
        for (auto& p : desc["outports"]) {
            std::string tag = p["tag"];
            std::string description = tag;
            std::string displayName = tag;
            if (p.contains("meta")) {
                // TODO get description and display name

                // TODO
                /*
                   if (p["meta"].contains("trigger") && p["meta"]["trigger"].is_bool() && p["meta"]["trigger"].get<bool>()) {
                   }
                   */
            }
            RNBO::MessageTag id = RNBO::TAG(tag.c_str());
            params.emplace(
                id,
                FRNBOMetasoundParam(FString(tag.c_str()), FText::AsCultureInvariant(description.c_str()), FText::AsCultureInvariant(displayName.c_str())));
        }

        return params;
    }

    static std::vector<FRNBOMetasoundParam> InputAudio(const RNBO::Json& desc)
    {
        // TODO param~
        return Signals(desc, "inlets");
    }

    static std::vector<FRNBOMetasoundParam> OutputAudio(const RNBO::Json& desc)
    {
        return Signals(desc, "outlets");
    }

    static bool MIDIIn(const RNBO::Json& desc)
    {
        return desc["numMidiInputPorts"].get<int>() > 0;
    }

    static bool MIDIOut(const RNBO::Json& desc)
    {
        return desc["numMidiOutputPorts"].get<int>() > 0;
    }

  private:
    static std::vector<FRNBOMetasoundParam> Signals(const RNBO::Json& desc, std::string selector)
    {
        std::vector<FRNBOMetasoundParam> params;

        const std::string sig("signal");
        for (auto& p : desc[selector]) {
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

// https://en.cppreference.com/w/cpp/language/template_parameters
template <const RNBO::Json& desc, RNBO::PatcherFactoryFunctionPtr (*FactoryFunction)(RNBO::PlatformInterface* platformInterface)>
class FRNBOOperator : public Metasound::TExecutableOperator<FRNBOOperator<desc, FactoryFunction>>
    , public RNBO::EventHandler
{
  private:
    RNBO::CoreObject CoreObject;
    RNBO::TimeConverter Converter = RNBO::TimeConverter(44100.0, 0.0);
    RNBO::ParameterEventInterfaceUniquePtr ParamInterface;

    int32 mNumFrames;

    std::unordered_map<RNBO::ParameterIndex, Metasound::FFloatReadRef> mInputFloatParams;
    std::unordered_map<RNBO::MessageTag, Metasound::FTriggerReadRef> mInportTriggerParams;

    std::vector<Metasound::FAudioBufferReadRef> mInputAudioParams;
    std::vector<const float*> mInputAudioBuffers;

    std::unordered_map<RNBO::MessageTag, Metasound::FTriggerWriteRef> mOutportTriggerParams;
    std::vector<Metasound::FAudioBufferWriteRef> mOutputAudioParams;
    std::vector<float*> mOutputAudioBuffers;

    TOptional<FTransportReadRef> Transport;

    TOptional<FMIDIBufferReadRef> MIDIIn;
    TOptional<FMIDIBufferWriteRef> MIDIOut;

    double LastTransportBeatTime = -1.0;
    float LastTransportBPM = 0.0f;
    bool LastTransportRun = false;
    int32 LastTransportNum = 0;
    int32 LastTransportDen = 0;

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& InputFloatParams()
    {
        static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputFloat(desc);
        return Params;
    }

    static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam>& InportTrig()
    {
        static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InportTrig(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& InputAudioParams()
    {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InputAudio(desc);
        return Params;
    }

    static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam>& OutportTrig()
    {
        static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> Params = FRNBOMetasoundParam::OutportTrig(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& OutputAudioParams()
    {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::OutputAudio(desc);
        return Params;
    }

    static const bool WithTransport()
    {
        // TODO config based on description
        return true;
    }

    static const bool WithMIDIIn()
    {
        static const bool v = FRNBOMetasoundParam::FRNBOMetasoundParam::MIDIIn(desc);
        return v;
    }

    static const bool WithMIDIOut()
    {
        static const bool v = FRNBOMetasoundParam::FRNBOMetasoundParam::MIDIOut(desc);
        return v;
    }

  public:
    static const Metasound::FNodeClassMetadata& GetNodeInfo()
    {
        auto InitNodeInfo = []() -> Metasound::FNodeClassMetadata {
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
            // TODO description and category from meta?

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
            Info.Author = Metasound::PluginAuthor;
            Info.PromptIfMissing = Metasound::PluginNodeMissingPrompt;
            Info.DefaultInterface = GetVertexInterface();
            Info.CategoryHierarchy = { Category };
            return Info;
        };

        static const Metasound::FNodeClassMetadata Info = InitNodeInfo();

        return Info;
    }

    static const Metasound::FVertexInterface& GetVertexInterface()
    {
        using Metasound::TInputDataVertex;
        using Metasound::TOutputDataVertex;

        auto Init = []() -> Metasound::FVertexInterface {
            Metasound::FInputVertexInterface inputs;

            for (auto& it : InportTrig()) {
                auto& p = it.second;
                inputs.Add(TInputDataVertex<Metasound::FTrigger>(p.Name(), p.MetaData()));
            }

            if (WithMIDIIn()) {
                inputs.Add(TInputDataVertex<FMIDIBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMIDIIn)));
            }

            for (auto& it : InputFloatParams()) {
                auto& p = it.second;
                inputs.Add(TInputDataVertex<float>(p.Name(), p.MetaData(), p.InitialValue()));
            }

            if (WithTransport()) {
                inputs.Add(TInputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));
            }

            for (auto& p : InputAudioParams()) {
                inputs.Add(TInputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
            }

            Metasound::FOutputVertexInterface outputs;

            for (auto& it : OutportTrig()) {
                auto& p = it.second;
                outputs.Add(TOutputDataVertex<Metasound::FTrigger>(p.Name(), p.MetaData()));
            }

            if (WithMIDIOut()) {
                outputs.Add(TOutputDataVertex<FMIDIBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMIDIOut)));
            }

            for (auto& p : OutputAudioParams()) {
                outputs.Add(TOutputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
            }

            Metasound::FVertexInterface interface(inputs, outputs);
            return interface;
        };
        static const Metasound::FVertexInterface Interface = Init();

        return Interface;
    }

    static TUniquePtr<Metasound::IOperator> CreateOperator(const Metasound::FCreateOperatorParams& InParams, Metasound::FBuildErrorArray& OutErrors)
    {
        const Metasound::FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
        const Metasound::FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FRNBOOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
    }

    FRNBOOperator(
        const Metasound::FCreateOperatorParams& InParams,
        const Metasound::FOperatorSettings& InSettings,
        const Metasound::FDataReferenceCollection& InputCollection,
        const Metasound::FInputVertexInterface& InputInterface,
        Metasound::FBuildErrorArray& OutErrors)
        : CoreObject(RNBO::UniquePtr<RNBO::PatcherInterface>(FactoryFunction(RNBO::Platform::get())()))
        , mNumFrames(InSettings.GetNumFramesPerBlock())

    {
        CoreObject.prepareToProcess(InSettings.GetSampleRate(), InSettings.GetNumFramesPerBlock());
        // all params are handled in the audio thread
        ParamInterface = CoreObject.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, this);

        for (auto& it : InportTrig()) {
            mInportTriggerParams.emplace(it.first, InputCollection.GetDataReadReferenceOrConstruct<Metasound::FTrigger>(it.second.Name(), InSettings));
        }

        if (WithMIDIIn()) {
            MIDIIn = { InputCollection.GetDataReadReferenceOrConstruct<FMIDIBuffer>(METASOUND_GET_PARAM_NAME(ParamMIDIIn), InSettings) };
        }

        for (auto& it : InputFloatParams()) {
            mInputFloatParams.emplace(it.first, InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, it.second.Name(), InSettings));
        }

        for (auto& p : InputAudioParams()) {
            mInputAudioParams.emplace_back(InputCollection.GetDataReadReferenceOrConstruct<Metasound::FAudioBuffer>(p.Name(), InSettings));
            mInputAudioBuffers.emplace_back(nullptr);
        }

        for (auto& it : OutportTrig()) {
            mOutportTriggerParams.emplace(it.first, Metasound::FTriggerWriteRef::CreateNew(InSettings));
        }

        if (WithMIDIOut()) {
            MIDIOut = FMIDIBufferWriteRef::CreateNew(InSettings);
        }

        for (auto& p : OutputAudioParams()) {
            mOutputAudioParams.emplace_back(Metasound::FAudioBufferWriteRef::CreateNew(InSettings));
            mOutputAudioBuffers.emplace_back(nullptr);
        }
        if (WithTransport()) {
            Transport = { InputCollection.GetDataReadReferenceOrConstruct<FTransport>(METASOUND_GET_PARAM_NAME(ParamTransport)) };
        }
    }

    virtual void BindInputs(Metasound::FInputVertexInterfaceData& InOutVertexData) override
    {
        {
            auto lookup = InportTrig();
            for (auto& [index, p] : mInportTriggerParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }

        if (MIDIIn.IsSet()) {
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMIDIIn), MIDIIn.GetValue());
        }

        {
            auto lookup = InputFloatParams();
            for (auto& [index, p] : mInputFloatParams) {
                auto it = lookup.find(index);
                // should never fail
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
            auto lookup = OutportTrig();
            for (auto& [index, p] : mOutportTriggerParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }

        if (MIDIOut.IsSet()) {
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamMIDIOut), MIDIOut.GetValue());
        }

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
        Converter = { CoreObject.getSampleRate(), CoreObject.getCurrentTime() };

        if (MIDIOut.IsSet()) {
            MIDIOut.GetValue()->AdvanceBlock();
        }

        // update outport triggers
        for (auto it : mOutportTriggerParams) {
            it.second->AdvanceBlock();
        }

        // setup audio buffers
        for (size_t i = 0; i < mInputAudioBuffers.size(); i++) {
            mInputAudioBuffers[i] = mInputAudioParams[i]->GetData();
        }

        for (size_t i = 0; i < mOutputAudioBuffers.size(); i++) {
            mOutputAudioBuffers[i] = mOutputAudioParams[i]->GetData();
        }

        if (MIDIIn.IsSet()) {
            auto& midiin = MIDIIn.GetValue();
            const int32 num = midiin->NumInBlock();
            for (int32 i = 0; i < num; i++) {
                const auto& e = (*midiin)[i];
                auto ms = Converter.convertSampleOffsetToMilliseconds(static_cast<RNBO::SampleOffset>(e.Frame()));

                RNBO::MidiEvent event(ms, 0, e.Data().data(), e.Length());
                ParamInterface->scheduleEvent(event);
            }
        }

        if (Transport.IsSet()) {
            auto& transport = Transport.GetValue();
            double btime = std::max(0.0, transport->GetBeatTime().GetSeconds()); // not actually seconds
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

        {
            for (auto& [tag, p] : mInportTriggerParams) {
                for (int32 i = 0; i < p->NumTriggeredInBlock(); i++) {
                    auto frame = (*p)[i];
                    ParamInterface->sendMessage(tag, 0, Converter.convertSampleOffsetToMilliseconds(static_cast<RNBO::SampleOffset>(frame)));
                }
            }
        }

        for (auto& [index, p] : mInputFloatParams) {
            double v = static_cast<double>(*p);
            if (v != ParamInterface->getParameterValue(index)) {
                ParamInterface->setParameterValue(index, v);
            }
        }

        CoreObject.process(static_cast<const float* const*>(mInputAudioBuffers.data()), mInputAudioBuffers.size(), mOutputAudioBuffers.data(), mOutputAudioBuffers.size(), mNumFrames);
    }

    // does this ever get called?
    void Reset(const Metasound::IOperator::FResetParams& InParams)
    {
        for (auto it : mOutportTriggerParams) {
            it.second->Reset();
        }
    }

    virtual void eventsAvailable()
    {
        drainEvents();
    }

    virtual void handleMessageEvent(const RNBO::MessageEvent& event) override
    {
        switch (event.getType()) {
            case RNBO::MessageEvent::Type::Bang:
            {
                auto it = mOutportTriggerParams.find(event.getTag());
                if (it != mOutportTriggerParams.end()) {
                    RNBO::SampleOffset frame = Converter.convertMillisecondsToSampleOffset(event.getTime());
                    it->second->TriggerFrame(static_cast<int32>(frame));
                }
            } break;
            default:
                // TODO
                break;
        }
    }

    virtual void handleMidiEvent(const RNBO::MidiEvent& event) override
    {
        if (!MIDIOut.IsSet()) {
            return;
        }
        RNBO::SampleOffset frame = Converter.convertMillisecondsToSampleOffset(event.getTime());
        FMIDIPacket packet(frame, event.getLength(), event.getData());
        MIDIOut.GetValue()->Push(packet);
    }
};
} // namespace RNBOMetasound

#undef LOCTEXT_NAMESPACE
