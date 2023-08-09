// Based on code Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MetasoundFacade.h"

class FRNBOWrapperModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

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
