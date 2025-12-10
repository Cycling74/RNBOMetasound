// Copyright Epic Games, Inc. All Rights Reserved.

#include "RNBOMetasound.h"
#include "RNBOTransport.h"
#include "MetasoundFrontendModuleRegistrationMacros.h"

void FRNBOMetasoundModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    METASOUND_REGISTER_ITEMS_IN_MODULE
}

void FRNBOMetasoundModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    METASOUND_UNREGISTER_ITEMS_IN_MODULE
}

IMPLEMENT_MODULE(FRNBOMetasoundModule, RNBOMetasound)
