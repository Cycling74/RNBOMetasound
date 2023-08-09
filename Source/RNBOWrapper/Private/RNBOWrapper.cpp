// Copyright Epic Games, Inc. All Rights Reserved.

#include "RNBOWrapper.h"
#include "MetasoundFrontendRegistries.h"

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

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRNBOWrapperModule, RNBOWrapper)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTime.h"
#include "MetasoundAudioBuffer.h"
#include "DSP/Delay.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodesPitchShift"

namespace Metasound
{
	namespace
	{
		METASOUND_PARAM(InParamAudioInput, "In", "Audio input.")
		METASOUND_PARAM(InParamDelayTime, "Delay Time", "The amount of time to delay the audio, in seconds.")
		METASOUND_PARAM(InParamDryLevel, "Dry Level", "The dry level of the delay.")
		METASOUND_PARAM(InParamWetLevel, "Wet Level", "The wet level of the delay.")
		METASOUND_PARAM(InParamFeedbackAmount, "Feedback", "Feedback amount TEST.")
		METASOUND_PARAM(OutParamAudio, "Out", "Audio output.")

		// TODO: make this a static vertex
		static float MaxDelaySeconds = 5.0f;
	}

	class FPitchShiftOperator : public TExecutableOperator<FPitchShiftOperator>
	{
		public:

			static const FNodeClassMetadata& GetNodeInfo();
			static const FVertexInterface& GetVertexInterface();
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

			FPitchShiftOperator(const FOperatorSettings& InSettings,
					const FAudioBufferReadRef& InAudioInput,
					const FTimeReadRef& InDelayTime,
					const FFloatReadRef& InDryLevel,
					const FFloatReadRef& InWetLevel,
					const FFloatReadRef& InFeedback);

			virtual FDataReferenceCollection GetInputs() const override;
			virtual FDataReferenceCollection GetOutputs() const override;
			void Execute();

		private:
			float GetInputDelayTimeMsec() const;

			// The input audio buffer
			FAudioBufferReadRef AudioInput;

			// The amount of delay time
			FTimeReadRef DelayTime;

			// The dry level
			FFloatReadRef DryLevel;

			// The wet level
			FFloatReadRef WetLevel;

			// The feedback amount
			FFloatReadRef Feedback;

			// The audio output
			FAudioBufferWriteRef AudioOutput;

			// The internal delay buffer
			//Audio::FDelay DelayBuffer;

			// The previous delay time
			float PrevDelayTimeMsec;

			// Feedback sample
			float FeedbackSample;
	};

	FPitchShiftOperator::FPitchShiftOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudioInput,
			const FTimeReadRef& InDelayTime,
			const FFloatReadRef& InDryLevel,
			const FFloatReadRef& InWetLevel,
			const FFloatReadRef& InFeedback)

		: AudioInput(InAudioInput)
		, DelayTime(InDelayTime)
		, DryLevel(InDryLevel)
		, WetLevel(InWetLevel)
		, Feedback(InFeedback)
		, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
		, PrevDelayTimeMsec(GetInputDelayTimeMsec())
		, FeedbackSample(0.0f)
		{
			// DelayBuffer.Init(InSettings.GetSampleRate(), PitchShift::MaxDelaySeconds);
			// DelayBuffer.SetDelayMsec(PrevDelayTimeMsec);
		}

	FDataReferenceCollection FPitchShiftOperator::GetInputs() const
	{
		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamAudioInput), FAudioBufferReadRef(AudioInput));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamDelayTime), FTimeReadRef(DelayTime));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamDryLevel), FFloatReadRef(DryLevel));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamWetLevel), FFloatReadRef(WetLevel));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamFeedbackAmount), FFloatReadRef(Feedback));

		return InputDataReferences;
	}

	FDataReferenceCollection FPitchShiftOperator::GetOutputs() const
	{
		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutParamAudio), FAudioBufferReadRef(AudioOutput));
		return OutputDataReferences;
	}

	float FPitchShiftOperator::GetInputDelayTimeMsec() const
	{
		// Clamp the delay time to the max delay allowed
		return 1000.0f * FMath::Clamp((float)DelayTime->GetSeconds(), 0.0f, MaxDelaySeconds);
	}

	void FPitchShiftOperator::Execute()
	{
		// Get clamped delay time
		float CurrentInputDelayTime = GetInputDelayTimeMsec();

		// Check to see if our delay amount has changed
		if (!FMath::IsNearlyEqual(PrevDelayTimeMsec, CurrentInputDelayTime))
		{
			PrevDelayTimeMsec = CurrentInputDelayTime;
			//DelayBuffer.SetEasedDelayMsec(PrevDelayTimeMsec);
		}

		const float* InputAudio = AudioInput->GetData();

		float* OutputAudio = AudioOutput->GetData();
		int32 NumFrames = AudioInput->Num();

		// Clamp the feedback amount to make sure it's bounded. Clamp to a number slightly less than 1.0
		float FeedbackAmount = FMath::Clamp(*Feedback, 0.0f, 1.0f - SMALL_NUMBER);
		float CurrentDryLevel = FMath::Clamp(*DryLevel, 0.0f, 1.0f);
		float CurrentWetLevel = FMath::Clamp(*WetLevel, 0.0f, 1.0f);

		if (FMath::IsNearlyZero(FeedbackAmount))
		{
			FeedbackSample = 0.0f;

			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				OutputAudio[FrameIndex] = 0;//CurrentWetLevel * DelayBuffer.ProcessAudioSample(InputAudio[FrameIndex]) + CurrentDryLevel * InputAudio[FrameIndex];
			}
		}
		else
		{
			// There is some amount of feedback so we do the feedback mixing
			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				OutputAudio[FrameIndex] = 0;//CurrentWetLevel * DelayBuffer.ProcessAudioSample(InputAudio[FrameIndex] + FeedbackSample * FeedbackAmount) + CurrentDryLevel * InputAudio[FrameIndex];
				FeedbackSample = OutputAudio[FrameIndex];
			}
		}
	}

	const FVertexInterface& FPitchShiftOperator::GetVertexInterface()
	{
		/*
		static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioInput)),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDelayTime), 1.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDryLevel), 0.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamWetLevel), 1.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamFeedbackAmount), 0.0f)
					),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamAudio))
					)
				);

		return Interface;
		*/

        auto InitVertexInterface = []() -> FVertexInterface {
            FInputVertexInterface inputs;
			inputs.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioInput)));
			inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDelayTime), 1.0f));
			inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDryLevel), 0.0f));
			inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamWetLevel), 1.0f));
			inputs.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamFeedbackAmount), 0.0f));

            FOutputVertexInterface outputs;
			outputs.Add(TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamAudio)));

            FVertexInterface Interface(inputs, outputs);

            return Interface;
        };

        static const FVertexInterface Interface = InitVertexInterface();
        return Interface;
	}

	const FNodeClassMetadata& FPitchShiftOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { TEXT("UE"), "Pitch Shift", TEXT("Audio") };
			Info.MajorVersion = 1;
			Info.MinorVersion = 1;
			Info.DisplayName = METASOUND_LOCTEXT("DelayNode_DisplayName", "Pitch Shift");
			Info.Description = METASOUND_LOCTEXT("DelayNode_Description", "Pitch shifts the audio buffer using a doppler shift method.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Delays);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	TUniquePtr<IOperator> FPitchShiftOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FAudioBufferReadRef AudioIn = InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InParamAudioInput), InParams.OperatorSettings);
		FTimeReadRef DelayTime = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InParamDelayTime), InParams.OperatorSettings);
		FFloatReadRef DryLevel = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamDryLevel), InParams.OperatorSettings);
		FFloatReadRef WetLevel = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamWetLevel), InParams.OperatorSettings);
		FFloatReadRef Feedback = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamFeedbackAmount), InParams.OperatorSettings);

		return MakeUnique<FPitchShiftOperator>(InParams.OperatorSettings, AudioIn, DelayTime, DryLevel, WetLevel, Feedback);
	}

    using Node = FGenericNode<FPitchShiftOperator>;
	METASOUND_REGISTER_NODE(Node)
}

#undef LOCTEXT_NAMESPACE
