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
	class RNBOWRAPPER_API FTransport {
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
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FTransport, RNBOWRAPPER_API, FTransportTypeInfo, FTransportReadRef, FTransportWriteRef);
}

