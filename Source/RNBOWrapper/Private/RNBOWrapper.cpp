// Copyright Epic Games, Inc. All Rights Reserved.

#include "RNBOWrapper.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundSampleCounter.h"

#include "MetasoundDataTypeRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "FRNBOWrapperModule"

void FRNBOWrapperModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
}

void FRNBOWrapperModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}


namespace Metasound {

	// Disable constructor pins of triggers 
	template<>
	struct TEnableConstructorVertex<FTransport>
	{
		static constexpr bool Value = false;
	};

	// Disable arrays of triggers
	template<>
	struct TEnableAutoArrayTypeRegistration<FTransport>
	{
		static constexpr bool Value = false;
	};

	// Disable auto-conversions based on FTransport implicit converters
	template<typename ToDataType>
	struct TEnableAutoConverterNodeRegistration<FTransport, ToDataType>
	{
		static constexpr bool Value = !std::is_arithmetic<ToDataType>::value;
	};

  REGISTER_METASOUND_DATATYPE(FTransport, "Transport", ::Metasound::ELiteralType::Boolean)
}

namespace 
{
	using namespace Metasound;
	using namespace RNBOWrapper;

	class FTransportOperator : public TExecutableOperator<FTransportOperator>
	{
		public:
			static const FNodeClassMetadata& GetNodeInfo() {
				auto InitNodeInfo = []() -> FNodeClassMetadata
				{
					FNodeClassMetadata Info;

					Info.ClassName        = { TEXT("UE"), TEXT("Transport"), TEXT("Audio") };
					Info.MajorVersion     = 1;
					Info.MinorVersion     = 0;
					Info.DisplayName      = LOCTEXT("Metasound_TransportDisplayName", "Transport");
					Info.Description      = LOCTEXT("Metasound_TransportNodeDescription", "Transport generator.");
					Info.Author           = PluginAuthor;
					Info.PromptIfMissing  = PluginNodeMissingPrompt;
					Info.DefaultInterface = GetVertexInterface();
					Info.CategoryHierarchy = { LOCTEXT("Metasound_TransportNodeCategory", "Utils") };

					return Info;
				};

				static const FNodeClassMetadata Info = InitNodeInfo();

				return Info;
			}
			static const FVertexInterface& GetVertexInterface() {
				auto InitVertexInterface = []() -> FVertexInterface {
					FInputVertexInterface inputs;
					inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBPM), 120.0f));
					inputs.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportRun), true));

					FOutputVertexInterface outputs;
					outputs.Add(TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportBeatTime)));
					outputs.Add(TOutputDataVertex<FTransport>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransport)));

					FVertexInterface Interface(inputs, outputs);

					return Interface;
				};

				static const FVertexInterface Interface = InitVertexInterface();
				return Interface;
			}

			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors) {
				const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
				const FInputVertexInterface& InputInterface     = GetVertexInterface().GetInputInterface();

				return MakeUnique<FTransportOperator>(InParams, InParams.OperatorSettings, InputCollection, InputInterface, OutErrors);
			}

			FTransportOperator(
					const FCreateOperatorParams& InParams,
					const FOperatorSettings& InSettings,
					const FDataReferenceCollection& InputCollection,
					const FInputVertexInterface& InputInterface,
					FBuildErrorArray& OutErrors)
				:
					Foo(FTransportWriteRef::CreateNew(false))
				, BlockSize(InSettings.GetNumFramesPerBlock())
				, SampleRate(static_cast<double>(InSettings.GetSampleRate()))
				, BlockPeriod(InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
				, PeriodMul(8.0 / 480.0 * InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
				, TransportBeatTime(FTimeWriteRef::CreateNew(0.0))
				, TransportBPM(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
				, TransportRun(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
				, CurTransportBeatTime(0.0)
			{
			}

			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
			}

			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBeatTime), TransportBeatTime);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Foo);
			}

			void Execute() {
				if (*TransportRun) {
					FTime offset(PeriodMul * static_cast<double>(*TransportBPM));
					CurTransportBeatTime += offset;
				}
				*TransportBeatTime = CurTransportBeatTime;
			}

		private:
			FTransportWriteRef Foo;

			FSampleCount BlockSize;
			FSampleRate SampleRate;
			FTime BlockPeriod;
			double PeriodMul;

			FTimeWriteRef TransportBeatTime;
			FFloatReadRef TransportBPM;
			FBoolReadRef TransportRun;

			FTime CurTransportBeatTime;
			float LastTransportBPM = 0.0f;
			bool LastTransportRun = false;
	};

	using TransportOperatorNode = FGenericNode<FTransportOperator>;
	METASOUND_REGISTER_NODE(TransportOperatorNode)
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRNBOWrapperModule, RNBOWrapper)
