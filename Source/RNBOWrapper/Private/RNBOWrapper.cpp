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

	namespace {
		METASOUND_PARAM(ParamTransportBPM, "BPM", "The tempo of the transport in beats per minute.")
		METASOUND_PARAM(ParamTransportRun, "Run", "The run state of the transport.")
		METASOUND_PARAM(ParamTransportNum, "Numerator", "The transport time signature numerator.")
		METASOUND_PARAM(ParamTransportDen, "Denominator", "The transport time signature denominator.")
	}

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
					inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportNum), 4));
					inputs.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTransportDen), 4));

					FOutputVertexInterface outputs;
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
				 BlockSize(InSettings.GetNumFramesPerBlock())
				, SampleRate(static_cast<double>(InSettings.GetSampleRate()))
				, BlockPeriod(InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
				, PeriodMul(8.0 / 480.0 * InSettings.GetNumFramesPerBlock() / InSettings.GetSampleRate())
				, CurTransportBeatTime(0.0)
				, TransportBPM(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportBPM), InSettings))
				, TransportRun(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportRun), InSettings))
				, TransportNum(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportNum), InSettings))
				, TransportDen(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<int32>(InputInterface, METASOUND_GET_PARAM_NAME(ParamTransportDen), InSettings))
				, Transport(FTransportWriteRef::CreateNew(false))
			{
			}

			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportBPM), TransportBPM);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportRun), TransportRun);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportNum), TransportNum);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransportDen), TransportDen);
			}

			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTransport), Transport);
			}

			void Execute() {
				FTransport Cur(*TransportRun, *TransportBPM, *TransportNum, *TransportDen);
				if (Cur.GetBPM()) {
					FTime offset(PeriodMul * static_cast<double>(Cur.GetBPM()));
					CurTransportBeatTime += offset;
				}
				Cur.SetBeatTime(CurTransportBeatTime);
				*Transport = Cur;
			}

		private:
			FSampleCount BlockSize;
			FSampleRate SampleRate;
			FTime BlockPeriod;
			double PeriodMul;

			FTime CurTransportBeatTime;
			float LastTransportBPM = 0.0f;
			bool LastTransportRun = false;

			FFloatReadRef TransportBPM;
			FBoolReadRef TransportRun;
			FInt32ReadRef TransportNum;
			FInt32ReadRef TransportDen;

			FTransportWriteRef Transport;
	};

	using TransportOperatorNode = FGenericNode<FTransportOperator>;
	METASOUND_REGISTER_NODE(TransportOperatorNode)
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRNBOWrapperModule, RNBOWrapper)
