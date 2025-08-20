#pragma once

#include "MetasoundFacade.h"
#include "RNBOTransport.h"

// visual studio warnings we're having trouble with
#pragma warning(disable : 4800 4065 4668 4804 4018 4060 4554 4018)
#include "RNBO.h"
#include "RNBO_TimeConverter.h"

#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundVertex.h"
#include "MetasoundLog.h"

#include "Internationalization/Text.h"
#include <unordered_map>

#include "DSP/BufferVectorOperations.h"
#include "DSP/ConvertDeinterleave.h"
#include "DSP/MultichannelBuffer.h"
#include "DSP/MultichannelLinearResampler.h"
#include "MetasoundWave.h"
#include "Sound/SampleBufferIO.h"
#include "AudioStreaming.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiConstants.h"
#include "HarmonixMidi/MidiVoiceId.h"

#include "AudioDecompress.h"
#include "Interfaces/IAudioFormat.h"
#include "Tasks/Pipe.h"

namespace RNBOMetasound {

#define LOCTEXT_NAMESPACE "FRNBOMetasoundModule"
METASOUND_PARAM(ParamMIDIIn, "MIDI In", "MIDI data input.")
METASOUND_PARAM(ParamMIDIOut, "MIDI Out", "MIDI data output.")
#undef LOCTEXT_NAMESPACE

using Metasound::FDataVertexMetadata;

struct WaveAssetDataRef
{
    RNBO::CoreObject& CoreObject;
    const char* Id;
    RNBO::DataRefIndex Index;
    Metasound::FWaveAssetReadRef WaveAsset;
    FObjectKey WaveAssetProxyKey;
    UE::Tasks::FTask Task;
    TArray<UE::Tasks::FTask> Cleanup; // make sure not to leave running tasks dangling

    WaveAssetDataRef(
        RNBO::CoreObject& coreObject,
        const char* id,
        const TCHAR* Name,
        const Metasound::FOperatorSettings& InSettings,
        const Metasound::FInputVertexInterfaceData& InputCollection);
    ~WaveAssetDataRef();
    void Update();
};

bool IsBoolParam(const RNBO::Json& p);
bool IsIntParam(const RNBO::Json& p);
bool IsFloatParam(const RNBO::Json& p);
bool IsInputParam(const RNBO::Json& p);
bool IsOutputParam(const RNBO::Json& p);

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

    static std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> InportTrig(const RNBO::Json& desc);
    static std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> OutportTrig(const RNBO::Json& desc);
    static std::vector<FRNBOMetasoundParam> InputAudio(const RNBO::Json& desc);
    static std::vector<FRNBOMetasoundParam> OutputAudio(const RNBO::Json& desc);
    static std::vector<FRNBOMetasoundParam> DataRef(const RNBO::Json& desc);
    static bool MIDIIn(const RNBO::Json& desc);
    static bool MIDIOut(const RNBO::Json& desc);
    static std::vector<FRNBOMetasoundParam> Signals(const RNBO::Json& desc, std::string selector);
    static void NumericParams(const RNBO::Json& desc, std::function<void(const RNBO::Json& param, RNBO::ParameterIndex index, const std::string& name, const std::string& displayName, const std::string& id)> func);
    static std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> NumericParamsFiltered(const RNBO::Json& desc, std::function<bool(const RNBO::Json& p)> filter);

    const FString mName;
    float mInitialValue;
    const FText mTooltip;
    const FText mDisplayName;
};

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FRNBOOperator"

// https://en.cppreference.com/w/cpp/language/template_parameters
template <const RNBO::Json& desc, RNBO::PatcherFactoryFunctionPtr (*FactoryFunction)()>
class FRNBOOperator : public Metasound::TExecutableOperator<FRNBOOperator<desc, FactoryFunction>>
    , public RNBO::EventHandler
    , public FMidiVoiceGeneratorBase
{
  private:
    RNBO::CoreObject CoreObject;
    RNBO::TimeConverter Converter = RNBO::TimeConverter(44100.0, 0.0);
    RNBO::ParameterEventInterfaceUniquePtr ParamInterface;

    int32 mNumFrames;
    float mSampleRate;

    std::unordered_map<RNBO::ParameterIndex, Metasound::FFloatReadRef> mInputFloatParams;
    std::unordered_map<RNBO::ParameterIndex, Metasound::FInt32ReadRef> mInputIntParams;
    std::unordered_map<RNBO::ParameterIndex, Metasound::FBoolReadRef> mInputBoolParams;
    std::unordered_map<RNBO::MessageTag, Metasound::FTriggerReadRef> mInportTriggerParams;
    std::vector<WaveAssetDataRef> mDataRefParams;

    std::vector<Metasound::FAudioBufferReadRef> mInputAudioParams;
    std::vector<const float*> mInputAudioBuffers;

    std::unordered_map<RNBO::ParameterIndex, Metasound::FFloatWriteRef> mOutputFloatParams;
    std::unordered_map<RNBO::ParameterIndex, Metasound::FInt32WriteRef> mOutputIntParams;
    std::unordered_map<RNBO::ParameterIndex, Metasound::FBoolWriteRef> mOutputBoolParams;
    std::unordered_map<RNBO::MessageTag, Metasound::FTriggerWriteRef> mOutportTriggerParams;
    std::vector<Metasound::FAudioBufferWriteRef> mOutputAudioParams;
    std::vector<float*> mOutputAudioBuffers;

    TOptional<FTransportReadRef> Transport;

    TOptional<HarmonixMetasound::FMidiStreamReadRef> MIDIIn;
    TOptional<HarmonixMetasound::FMidiStreamWriteRef> MIDIOut;

    double LastTransportBeatTime = -1.0;
    float LastTransportBPM = 0.0f;
    bool LastTransportRun = false;
    int32 LastTransportNum = 0;
    int32 LastTransportDen = 0;

    static const size_t ParamCount()
    {
        static const size_t count = desc["parameters"].size();
        return count;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& InputFloatParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsInputParam(p) && IsFloatParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& InputIntParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsInputParam(p) && IsIntParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& InputBoolParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsInputParam(p) && IsBoolParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& OutputFloatParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsOutputParam(p) && IsFloatParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& OutputIntParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsOutputParam(p) && IsIntParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam>& OutputBoolParams()
    {
        static const auto Params = FRNBOMetasoundParam::NumericParamsFiltered(desc, [](const RNBO::Json& p) -> bool { return IsOutputParam(p) && IsBoolParam(p); });
        return Params;
    }

    static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam>& InportTrig()
    {
        static const std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> Params = FRNBOMetasoundParam::InportTrig(desc);
        return Params;
    }

    static const std::vector<FRNBOMetasoundParam>& DataRefParams()
    {
        static const std::vector<FRNBOMetasoundParam> Params = FRNBOMetasoundParam::DataRef(desc);
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
        const std::string key = "transportUsed";
        return !desc.contains(key) || desc[key].get<bool>();
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

            /*
             * https://github.com/Cycling74/RNBOMetasound/issues/25
             *  audio
             *  midi
             *  trigger
             *  params (sorted by param index)
             *  wave
             *  transport
             */

            for (auto& p : InputAudioParams()) {
                inputs.Add(TInputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
            }

            if (WithMIDIIn()) {
                inputs.Add(TInputDataVertex<HarmonixMetasound::FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMIDIIn)));
            }

            for (auto& it : InportTrig()) {
                auto& p = it.second;
                inputs.Add(TInputDataVertex<Metasound::FTrigger>(p.Name(), p.MetaData()));
            }

            // add params in order
            {
                auto& lookupFloat = InputFloatParams();
                auto& lookupInt = InputIntParams();
                auto& lookupBool = InputBoolParams();
                auto count = ParamCount();
                for (auto i = 0; i < count; i++) {
                    auto it = lookupFloat.find(i);
                    if (it != lookupFloat.end()) {
                        auto& p = it->second;
                        inputs.Add(TInputDataVertex<float>(p.Name(), p.MetaData(), p.InitialValue()));
                        continue;
                    }
                    it = lookupInt.find(i);
                    if (it != lookupInt.end()) {
                        auto& p = it->second;
                        inputs.Add(TInputDataVertex<int32>(p.Name(), p.MetaData(), p.InitialValue()));
                        continue;
                    }
                    it = lookupBool.find(i);
                    if (it != lookupBool.end()) {
                        auto& p = it->second;
                        inputs.Add(TInputDataVertex<bool>(p.Name(), p.MetaData(), p.InitialValue() != 0.0f));
                        continue;
                    }
                    // this is okay, we might have non mapped params
                }
            }

            for (auto& p : DataRefParams()) {
                inputs.Add(TInputDataVertex<Metasound::FWaveAsset>(p.Name(), p.MetaData()));
            }

            if (WithTransport()) {
                inputs.Add(TInputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));
            }

            Metasound::FOutputVertexInterface outputs;

            for (auto& p : OutputAudioParams()) {
                outputs.Add(TOutputDataVertex<Metasound::FAudioBuffer>(p.Name(), p.MetaData()));
            }

            if (WithMIDIOut()) {
                outputs.Add(TOutputDataVertex<HarmonixMetasound::FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamMIDIOut)));
            }

            for (auto& it : OutportTrig()) {
                auto& p = it.second;
                outputs.Add(TOutputDataVertex<Metasound::FTrigger>(p.Name(), p.MetaData()));
            }

            // add params in order
            {
                auto& lookupFloat = OutputFloatParams();
                auto& lookupInt = OutputIntParams();
                auto& lookupBool = OutputBoolParams();
                auto count = ParamCount();
                for (auto i = 0; i < count; i++) {
                    auto it = lookupFloat.find(i);
                    if (it != lookupFloat.end()) {
                        auto& p = it->second;
                        outputs.Add(TOutputDataVertex<float>(p.Name(), p.MetaData()));
                        continue;
                    }
                    it = lookupInt.find(i);
                    if (it != lookupInt.end()) {
                        auto& p = it->second;
                        outputs.Add(TOutputDataVertex<int32>(p.Name(), p.MetaData()));
                        continue;
                    }
                    it = lookupBool.find(i);
                    if (it != lookupBool.end()) {
                        auto& p = it->second;
                        outputs.Add(TOutputDataVertex<bool>(p.Name(), p.MetaData()));
                        continue;
                    }
                    // this is okay, we might have non mapped params
                }
            }

            Metasound::FVertexInterface interface(inputs, outputs);
            return interface;
        };
        static const Metasound::FVertexInterface Interface = Init();

        return Interface;
    }

    static TUniquePtr<Metasound::IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
    {
        const Metasound::FInputVertexInterfaceData& InputCollection = InParams.InputData;
        const Metasound::FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

        return MakeUnique<FRNBOOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutResults);
    }

    FRNBOOperator(
        const Metasound::FBuildOperatorParams& InParams,
        const Metasound::FOperatorSettings& InSettings,
        const Metasound::FInputVertexInterfaceData& InputCollection,
        const Metasound::FInputVertexInterface& InputInterface,
        Metasound::FBuildResults& OutResults)
        : FMidiVoiceGeneratorBase()
        , CoreObject(RNBO::UniquePtr<RNBO::PatcherInterface>(FactoryFunction()()))
        , mNumFrames(InSettings.GetNumFramesPerBlock())
        , mSampleRate(InSettings.GetSampleRate())

    {
        CoreObject.prepareToProcess(InSettings.GetSampleRate(), InSettings.GetNumFramesPerBlock());
        // all params are handled in the audio thread, single producer seems to have better performance than NotThreadSafe
        ParamInterface = CoreObject.createParameterInterface(RNBO::ParameterEventInterface::SingleProducer, this);

        // INPUTS
        for (auto& it : InportTrig()) {
            mInportTriggerParams.emplace(it.first, InputCollection.GetOrCreateDefaultDataReadReference<Metasound::FTrigger>(it.second.Name(), InSettings));
        }

        if (WithMIDIIn()) {
            MIDIIn = { InputCollection.GetOrCreateDefaultDataReadReference<HarmonixMetasound::FMidiStream>(METASOUND_GET_PARAM_NAME(ParamMIDIIn), InSettings) };
        }

        for (auto& it : InputFloatParams()) {
            mInputFloatParams.emplace(it.first, InputCollection.GetOrCreateDefaultDataReadReference<float>(it.second.Name(), InSettings));
        }

        for (auto& it : InputIntParams()) {
            mInputIntParams.emplace(it.first, InputCollection.GetOrCreateDefaultDataReadReference<int32>(it.second.Name(), InSettings));
        }

        for (auto& it : InputBoolParams()) {
            mInputBoolParams.emplace(it.first, InputCollection.GetOrCreateDefaultDataReadReference<bool>(it.second.Name(), InSettings));
        }

        {
            RNBO::DataRefIndex index = 0;
            for (auto& p : DataRefParams()) {
                auto id = CoreObject.getExternalDataId(index++);
                WaveAssetDataRef ref(CoreObject, id, p.Name(), InSettings, InputCollection);
                // TODO could maybe even load the data in the main thread?
                ref.Update();
                mDataRefParams.push_back(std::move(ref));
            }
        }

        for (auto& p : InputAudioParams()) {
            mInputAudioParams.emplace_back(InputCollection.GetOrCreateDefaultDataReadReference<Metasound::FAudioBuffer>(p.Name(), InSettings));
            mInputAudioBuffers.emplace_back(nullptr);
        }

        // OUTPUTS

        for (auto& it : OutportTrig()) {
            mOutportTriggerParams.emplace(it.first, Metasound::FTriggerWriteRef::CreateNew(InSettings));
        }

        if (WithMIDIOut()) {
            MIDIOut = HarmonixMetasound::FMidiStreamWriteRef::CreateNew();
        }

        for (auto& it : OutputFloatParams()) {
            mOutputFloatParams.emplace(it.first, Metasound::FFloatWriteRef::CreateNew(it.second.InitialValue()));
        }

        for (auto& it : OutputIntParams()) {
            mOutputIntParams.emplace(it.first, Metasound::FInt32WriteRef::CreateNew(static_cast<int32>(it.second.InitialValue())));
        }

        for (auto& it : OutputBoolParams()) {
            mOutputBoolParams.emplace(it.first, Metasound::FBoolWriteRef::CreateNew(it.second.InitialValue() != 0.0f));
        }

        for (auto& p : OutputAudioParams()) {
            mOutputAudioParams.emplace_back(Metasound::FAudioBufferWriteRef::CreateNew(InSettings));
            mOutputAudioBuffers.emplace_back(nullptr);
        }

        if (WithTransport()) {
            Transport = { InputCollection.GetOrCreateDefaultDataReadReference<FTransport>(METASOUND_GET_PARAM_NAME(ParamTransport), InSettings) };
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
        {
            auto lookup = InputIntParams();
            for (auto& [index, p] : mInputIntParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }
        {
            auto lookup = InputBoolParams();
            for (auto& [index, p] : mInputBoolParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }
        {
            auto lookup = DataRefParams();
            for (size_t i = 0; i < mDataRefParams.size(); i++) {
                auto& p = lookup[i];
                InOutVertexData.BindReadVertex(p.Name(), mDataRefParams[i].WaveAsset);
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
            auto lookup = OutputFloatParams();
            for (auto& [index, p] : mOutputFloatParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }
        {
            auto lookup = OutputIntParams();
            for (auto& [index, p] : mOutputIntParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
        }
        {
            auto lookup = OutputBoolParams();
            for (auto& [index, p] : mOutputBoolParams) {
                auto it = lookup.find(index);
                // should never fail
                if (it != lookup.end()) {
                    InOutVertexData.BindReadVertex(it->second.Name(), p);
                }
            }
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
            MIDIOut.GetValue()->PrepareBlock();
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
            for (const HarmonixMetasound::FMidiStreamEvent& Event : midiin->GetEventsInBlock()) {
                auto& msg = Event.MidiMessage;
                if (msg.IsStd()) {
                    auto ms = Converter.convertSampleOffsetToMilliseconds(static_cast<RNBO::SampleOffset>(Event.BlockSampleFrameIndex));
                    auto status = msg.GetStdStatus();
                    std::array<uint8_t, 3> data = { status, msg.GetStdData1(), msg.GetStdData2() };
                    size_t len = data.size();

                    switch (msg.GetStdStatusType()) {
                      case Harmonix::Midi::Constants::GNoteOff:
                      case Harmonix::Midi::Constants::GNoteOn:
                      case Harmonix::Midi::Constants::GPolyPres:
                      case Harmonix::Midi::Constants::GControl:
                      case Harmonix::Midi::Constants::GPitch: //bend
                        len = 3;
                        break;
                      case Harmonix::Midi::Constants::GProgram:
                      case Harmonix::Midi::Constants::GChanPres:
                        len = 2;
                        break;
                      case Harmonix::Midi::Constants::GSystem:
                        switch (status) {
                          case 0xF2:
                            len = 3; //song position
                            break;
                          case 0xF1: //quarter frame
                          case 0xF3: //song select
                            len = 2;
                            break;
                          case 0xF0:
                          case 0xF7:
                            //TODO how to handle sysex? does HarmonixMidi support it?
                            continue;
                          default:
                            len = 1;
                            break;
                        }
                        break;
                      default:
                        //unknown
                        continue;
                    }

                    RNBO::MidiEvent event(ms, 0, data.data(), len);
                    ParamInterface->scheduleEvent(event);
                }
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

        for (auto& [index, p] : mInputFloatParams) {
            double v = static_cast<double>(*p);
            if (v != ParamInterface->getParameterValue(index)) {
                ParamInterface->setParameterValue(index, v);
            }
        }
        for (auto& [index, p] : mInputIntParams) {
            double v = static_cast<double>(*p);
            if (v != ParamInterface->getParameterValue(index)) {
                ParamInterface->setParameterValue(index, v);
            }
        }
        for (auto& [index, p] : mInputBoolParams) {
            double v = *p ? 1.0 : 0.0;
            if (v != ParamInterface->getParameterValue(index)) {
                ParamInterface->setParameterValue(index, v);
            }
        }
        for (auto& [tag, p] : mInportTriggerParams) {
            for (int32 i = 0; i < p->NumTriggeredInBlock(); i++) {
                auto frame = (*p)[i];
                ParamInterface->sendMessage(tag, 0, Converter.convertSampleOffsetToMilliseconds(static_cast<RNBO::SampleOffset>(frame)));
            }
        }
        for (auto& p : mDataRefParams) {
            p.Update();
        }

        CoreObject.process(static_cast<const float* const*>(mInputAudioBuffers.data()), mInputAudioBuffers.size(), mOutputAudioBuffers.data(), mOutputAudioBuffers.size(), mNumFrames);
    }

    // does this ever get called?
    void Reset(const Metasound::IOperator::FResetParams& InParams)
    {
        for (auto it : mOutportTriggerParams) {
            it.second->Reset();
        }
        if (MIDIOut.IsSet()) {
            auto m = MIDIOut.GetValue();
            m->PrepareBlock();
            m->ResetClock();
        }
    }

    virtual void eventsAvailable()
    {
        drainEvents();
    }

    virtual void handleParameterEvent(const RNBO::ParameterEvent& event) override
    {
        {
            auto it = mOutputBoolParams.find(event.getIndex());
            if (it != mOutputBoolParams.end()) {
                (*it->second) = static_cast<bool>(event.getValue() != 0.0f);
                return;
            }
        }
        {
            auto it = mOutputFloatParams.find(event.getIndex());
            if (it != mOutputFloatParams.end()) {
                (*it->second) = static_cast<float>(event.getValue());
                return;
            }
        }
        {
            auto it = mOutputIntParams.find(event.getIndex());
            if (it != mOutputIntParams.end()) {
                (*it->second) = static_cast<int32>(event.getValue());
                return;
            }
        }
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

        auto data = event.getData();
        uint8 status = 0, data1 = 0, data2 = 0;
        switch (event.getLength()) {
            case 3:
                data2 = data[2];
                //fall thru
            case 2:
                data1 = data[1];
                //fall thru
            case 1:
                status = data[0];
                break;
            default:
                break;
        };

        HarmonixMetasound::FMidiStreamEvent packet(this, FMidiMsg(status, data1, data2));
        packet.BlockSampleFrameIndex = frame;
        packet.TrackIndex = 1; // as per rec from Harmonix
        MIDIOut.GetValue()->AddMidiEvent(packet);
    }
};
} // namespace RNBOMetasound

#undef LOCTEXT_NAMESPACE
