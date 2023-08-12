// Based on code Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

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
#undef LOCTEXT_NAMESPACE
}
