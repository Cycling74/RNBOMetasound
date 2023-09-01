#pragma once

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundParamHelper.h"

//visual studio warnings we're having trouble with
#pragma warning ( disable : 4800 4065 4668 4804 4018 4060 4554 4018 )
#include "RNBO.h"

namespace RNBOMetasound {

    using Metasound::FTime;

#define LOCTEXT_NAMESPACE "FRNBOMetasoundModule"
    METASOUND_PARAM(ParamTransport, "Transport", "The transport.")
#undef LOCTEXT_NAMESPACE

        class RNBOMETASOUND_API FTransport {
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
}

DECLARE_METASOUND_DATA_REFERENCE_TYPES(RNBOMetasound::FTransport, RNBOMETASOUND_API, FTransportTypeInfo, FTransportReadRef, FTransportWriteRef);
