#pragma once

#include "RNBOOperator.h"

namespace {
UE::Tasks::FPipe AsyncTaskPipe{ TEXT("RNBODatarefLoader") };
}

namespace RNBOMetasound {

WaveAssetDataRef::WaveAssetDataRef(
    RNBO::CoreObject& coreObject,
    const char* id,
    const TCHAR* Name,
    const Metasound::FOperatorSettings& InSettings,
    const Metasound::FDataReferenceCollection& InputCollection)
    : CoreObject(coreObject)
    , Id(id)
    , WaveAsset(InputCollection.GetDataReadReferenceOrConstruct<Metasound::FWaveAsset>(Name))
{
}

WaveAssetDataRef::~WaveAssetDataRef()
{
    Cleanup.Push(Task);
    for (auto& t : Cleanup) {
        if (t.IsValid() && !t.IsCompleted()) {
            t.BusyWait();
        }
    }
}

void WaveAssetDataRef::Update()
{
    auto WaveProxy = WaveAsset->GetSoundWaveProxy();
    if (WaveProxy.IsValid()) {
        auto key = WaveProxy->GetFObjectKey();
        if (key == WaveAssetProxyKey) {
            return;
        }
        WaveAssetProxyKey = key;

        // TODO remove completed tasks from Cleanup
        // TODO optionally release the existing dataref from the core object to reduce memory usage?

        if (Task.IsValid() && !Task.IsCompleted()) {
            Cleanup.Push(Task);
        }

        Task = AsyncTaskPipe.Launch(
            UE_SOURCE_LOCATION,
            [this, WaveProxy]() {
                double sr = WaveProxy->GetSampleRate();
                size_t chans = WaveProxy->GetNumChannels();
                // int32 frames = WaveProxy->GetNumFrames();
                // double duration = WaveProxy->GetDuration();

                FName Format = WaveProxy->GetRuntimeFormat();
                IAudioInfoFactory* Factory = IAudioInfoFactoryRegistry::Get().Find(Format);
                if (Factory == nullptr) {
                    UE_LOG(LogMetaSound, Error, TEXT("IAudioInfoFactoryRegistry::Get().Find(%s) failed"), Format);
                    return;
                }

                ICompressedAudioInfo* Decompress = Factory->Create();
                FSoundQualityInfo quality;
                TArray<uint8> Buf;
                int32 ValidBytes = 0;
                if (WaveProxy->IsStreaming()) {
                    if (!Decompress->StreamCompressedInfo(WaveProxy, &quality)) {
                        UE_LOG(LogMetaSound, Error, TEXT("RNBO Failed to get compressed stream info"));
                        return;
                    }
                    Buf.AddZeroed(quality.SampleDataSize);
                    Decompress->StreamCompressedData(Buf.GetData(), false, Buf.Num(), ValidBytes);
                }
                else {
                    if (!Decompress->ReadCompressedInfo(WaveProxy->GetResourceData(), WaveProxy->GetResourceSize(), &quality)) {
                        UE_LOG(LogMetaSound, Error, TEXT("RNBO Failed to get compressed info"));
                        return;
                    }
                    Buf.AddZeroed(quality.SampleDataSize);
                    if (Decompress->ReadCompressedData(Buf.GetData(), false, Buf.Num())) {
                        ValidBytes = Buf.Num();
                    }
                    else {
                        UE_LOG(LogMetaSound, Error, TEXT("RNBO Failed to read compressed data"));
                        return;
                    }
                }

                TArrayView<const int16> Data(reinterpret_cast<const int16*>(Buf.GetData()), Buf.Num() / sizeof(int16));

                TSharedPtr<TArray<float>> Samples(new TArray<float>());
                Samples->Reserve(Data.Num());

                const float div = static_cast<float>(INT16_MAX);
                for (auto i = 0; i < Data.Num(); i++) {
                    Samples->Push(static_cast<float>(Data[i]) / div);
                }
                size_t SizeInBytes = Buf.Num();
                char* DataPtr = reinterpret_cast<char*>(Samples->GetData());

                RNBO::Float32AudioBuffer bufferType(chans, sr);
                CoreObject.setExternalData(Id, DataPtr, sizeof(float) * static_cast<size_t>(Samples->Num()), bufferType, [Samples](RNBO::ExternalDataId, char*) mutable {
                    Samples.Reset();
                });
            },
            UE::Tasks::ETaskPriority::BackgroundNormal);
    }
}

bool IsBoolParam(const RNBO::Json& p)
{
    if (p["steps"].get<int>() == 2 && p["enumValues"].is_array()) {
        auto e = p["enumValues"];
        return e[0].is_number() && e[1].is_number() && e[0].get<float>() == 0.0f && e[1].get<float>() == 1.0f;
    }
    return false;
}

bool IsIntParam(const RNBO::Json& p)
{
    return !IsBoolParam(p) && p["isEnum"].get<bool>();
}

bool IsFloatParam(const RNBO::Json& p)
{
    return !(IsBoolParam(p) || IsIntParam(p));
}

bool IsInputParam(const RNBO::Json& p)
{
    if (p["meta"].is_object() && p["meta"]["in"].is_boolean()) {
        return p["meta"]["in"].get<bool>();
    }
    // default true
    return true;
}

bool IsOutputParam(const RNBO::Json& p)
{
    if (p["meta"].is_object() && p["meta"]["out"].is_boolean()) {
        return p["meta"]["out"].get<bool>();
    }
    // default false
    return false;
}

std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> FRNBOMetasoundParam::InportTrig(const RNBO::Json& desc)
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

std::unordered_map<RNBO::MessageTag, FRNBOMetasoundParam> FRNBOMetasoundParam::OutportTrig(const RNBO::Json& desc)
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

std::vector<FRNBOMetasoundParam> FRNBOMetasoundParam::InputAudio(const RNBO::Json& desc)
{
    // TODO param~
    return Signals(desc, "inlets");
}

std::vector<FRNBOMetasoundParam> FRNBOMetasoundParam::OutputAudio(const RNBO::Json& desc)
{
    return Signals(desc, "outlets");
}

std::vector<FRNBOMetasoundParam> FRNBOMetasoundParam::DataRef(const RNBO::Json& desc)
{
    std::vector<FRNBOMetasoundParam> params;
    for (auto& p : desc["externalDataRefs"]) {
        // only supporting buffer~ for now
        if (p.contains("tag") && p["tag"].get<std::string>() != "buffer~") {
            continue;
        }
        std::string id = p["id"];
        std::string description = id;
        std::string displayName = id;
        params.emplace_back(FString(id.c_str()), FText::AsCultureInvariant(description.c_str()), FText::AsCultureInvariant(displayName.c_str()));
    }

    return params;
}

bool FRNBOMetasoundParam::MIDIIn(const RNBO::Json& desc)
{
    return desc["numMidiInputPorts"].get<int>() > 0;
}

bool FRNBOMetasoundParam::MIDIOut(const RNBO::Json& desc)
{
    return desc["numMidiOutputPorts"].get<int>() > 0;
}

std::vector<FRNBOMetasoundParam> FRNBOMetasoundParam::Signals(const RNBO::Json& desc, std::string selector)
{
    std::vector<FRNBOMetasoundParam> params;

    const std::string sig("signal");
    for (auto& p : desc[selector]) {
        if (p.contains("type") && sig.compare(p["type"].get<std::string>()) == 0) {
            std::string name = p["tag"].get<std::string>();
            std::string tooltip = name;
            std::string displayName = name;

            // read comment and populate displayName if it exists
            if (p.contains("comment") && p["comment"].is_string()) {
                displayName = p["comment"].get<std::string>();
            }
            if (p.contains("meta") && p["meta"].is_object()) {
                const RNBO::Json& meta = p["meta"];
                if (meta.contains("displayname") && meta["displayname"].is_string()) {
                    displayName = meta["displayname"].get<std::string>();
                }
                if (meta.contains("tooltip") && meta["tooltip"].is_string()) {
                    tooltip = meta["tooltip"].get<std::string>();
                }
            }

            params.emplace_back(FString(name.c_str()), FText::AsCultureInvariant(tooltip.c_str()), FText::AsCultureInvariant(displayName.c_str()), 0.0f);
        }
    }

    return params;
}

void FRNBOMetasoundParam::NumericParams(const RNBO::Json& desc, std::function<void(const RNBO::Json& param, RNBO::ParameterIndex index, const std::string& name, const std::string& displayName, const std::string& id)> func)
{
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
        if (displayName.size() == 0) {
            displayName = name;
        }
        std::string id = p["paramId"].get<std::string>();
        func(p, index, name, displayName, id);
    }
}

std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> FRNBOMetasoundParam::NumericParamsFiltered(const RNBO::Json& desc, std::function<bool(const RNBO::Json& p)> filter)
{
    std::unordered_map<RNBO::ParameterIndex, FRNBOMetasoundParam> params;
    NumericParams(desc, [&params, &filter](const RNBO::Json& p, RNBO::ParameterIndex index, const std::string& name, const std::string& displayName, const std::string& id) {
        if (filter(p)) {
            float initialValue = p["initialValue"].get<float>();
            params.emplace(
                index,
                FRNBOMetasoundParam(FString(name.c_str()), FText::AsCultureInvariant(id.c_str()), FText::AsCultureInvariant(displayName.c_str()), initialValue));
        }
    });
    return params;
}

} // namespace RNBOMetasound
