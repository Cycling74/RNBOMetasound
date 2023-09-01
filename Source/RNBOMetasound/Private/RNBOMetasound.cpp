// Copyright Epic Games, Inc. All Rights Reserved.

#include "RNBOMetasound.h"
#include "RNBOTransport.h"
#include "MetasoundFrontendRegistries.h"

void FRNBOMetasoundModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
}

void FRNBOMetasoundModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

IMPLEMENT_MODULE(FRNBOMetasoundModule, RNBOMetasound)
