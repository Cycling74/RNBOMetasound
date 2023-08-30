// Based on code Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"

#include "MetasoundVertex.h"
#include "RNBO.h"

class FRNBOWrapperModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace RNBOWrapper {
  template <class Op>
		class FGenericNode : public Metasound::FNodeFacade
	{
		public:
			/**
			 * Constructor used by the Metasound Frontend.
			 */
			FGenericNode(const Metasound::FNodeInitData& InitData)
				: Metasound::FNodeFacade(InitData.InstanceName, InitData.InstanceID, Metasound::TFacadeOperatorClass<Op>())
			{
			}
  };

#define LOCTEXT_NAMESPACE "FRNBOWrapperModule"
		METASOUND_PARAM(ParamTransport, "Transport", "The transport.")
#undef LOCTEXT_NAMESPACE
}

namespace Metasound {

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

  //TODO  METASOUNDFRONTEND_API ?
	class METASOUNDFRONTEND_API FTransport {
		public:
			FTransport(bool bRun = true, float bBPM = 120.0, int32 bTimeSigNum = 4, int32 bTimeSigDen = 4) : 
				BeatTime(0.0),
				Run(bRun),
				BPM(std::max(0.0f, bBPM)),
				TimeSig(std::make_tuple(std::max(1, bTimeSigNum), std::max(1, bTimeSigDen)))
				{
				}

			FTime GetBeatTime() const { return BeatTime; }
			bool GetRun() const { return Run; }
			float GetBPM() const { return BPM; }
			std::tuple<int32, int32> GetTimeSig() const { return TimeSig; }

			void SetBeatTime(FTime v) { BeatTime = v; }
			void SetRun(bool v) { Run = v; }
			void SetBPM(float v) { BPM = std::max(0.0f, v); }
			void SetTimeSig(std::tuple<int32, int32> v) { 
				TimeSig = std::make_tuple(std::max(1, std::get<0>(v)), std::max(1, std::get<1>(v))); 
			}

		private:
			FTime BeatTime;
			bool Run = false;
			float BPM = 120.0f;
			std::tuple<int32, int32> TimeSig;
  };
  //XXX what about the METASOUNDFRONTEND_API ?
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FTransport, METASOUNDFRONTEND_API, FTransportTypeInfo, FTransportReadRef, FTransportWriteRef);
}

