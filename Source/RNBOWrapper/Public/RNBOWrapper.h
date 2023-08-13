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
		METASOUND_PARAM(ParamTransportBeatTime, "Transport Beat Time", "The beat time of the transport.")
		METASOUND_PARAM(ParamTransportBPM, "Transport BPM", "The tempo of the transport in beats per minute.")
		METASOUND_PARAM(ParamTransportRun, "Transport Run", "The run state of the transport.")
		METASOUND_PARAM(ParamTransportNum, "Transport Numerator", "The transport time signature numerator.")
		METASOUND_PARAM(ParamTransportDen, "Transport Denominator", "The transport time signature denominator.")
		METASOUND_PARAM(ParamTransport, "Transport", "The transport.")
#undef LOCTEXT_NAMESPACE
}

namespace Metasound {
  //TODO  METASOUNDFRONTEND_API ?
  class METASOUNDFRONTEND_API FTransport {
    public:
      FTransport() : BeatTime(0.0) { }
			explicit FTransport(const FOperatorSettings& InSettings, bool bShouldRun) : 
        BeatTime(0.0),
        Run(bShouldRun) {
      }
    private:
      FTime BeatTime;
      bool Run = false;
      float BPM = 120.0f;
      int32 TimeSigNum = 4;
      int32 TimeSigDen = 4;
  };
  //XXX what about the METASOUNDFRONTEND_API ?
	DECLARE_METASOUND_DATA_REFERENCE_TYPES(FTransport, METASOUNDFRONTEND_API, FTransportTypeInfo, FTransportReadRef, FTransportWriteRef);
}

