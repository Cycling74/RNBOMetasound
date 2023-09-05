#pragma once

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundParamHelper.h"

// visual studio warnings we're having trouble with
#pragma warning(disable : 4800 4065 4668 4804 4018 4060 4554 4018)
#include "RNBO.h"

namespace RNBOMetasound {

using Metasound::FTime;

#define LOCTEXT_NAMESPACE "FRNBOMetasoundModule"
METASOUND_PARAM(ParamTransport, "Transport", "The transport.")
#undef LOCTEXT_NAMESPACE

class RNBOMETASOUND_API FTransport
{
  public:
    FTransport(bool bRun = true, float bBPM = 120.0, int32 bTimeSigNum = 4, int32 bTimeSigDen = 4);

    FTime GetBeatTime() const;
    bool GetRun() const;
    float GetBPM() const;
    std::tuple<int32, int32> GetTimeSig() const;

    void SetBeatTime(FTime v);
    void SetRun(bool v);
    void SetBPM(float v);
    void SetTimeSig(std::tuple<int32, int32> v);

  private:
    FTime BeatTime;
    bool Run = false;
    float BPM = 120.0f;
    std::tuple<int32, int32> TimeSig;
};
} // namespace RNBOMetasound

DECLARE_METASOUND_DATA_REFERENCE_TYPES(RNBOMetasound::FTransport, RNBOMETASOUND_API, FTransportTypeInfo, FTransportReadRef, FTransportWriteRef);
