#include "RNBOMIDI.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundDataTypeRegistrationMacro.h"

// Disable constructor pins of triggers
template <>
struct Metasound::TEnableConstructorVertex<RNBOMetasound::FMIDIBuffer>
{
    static constexpr bool Value = false;
};

// Disable arrays of triggers
template <>
struct Metasound::TEnableAutoArrayTypeRegistration<RNBOMetasound::FMIDIBuffer>
{
    static constexpr bool Value = false;
};

// Disable auto-conversions based on FMIDIBuffer implicit converters
template <typename ToDataType>
struct Metasound::TEnableAutoConverterNodeRegistration<RNBOMetasound::FMIDIBuffer, ToDataType>
{
    static constexpr bool Value = false;
};

template <typename FromDataType>
struct Metasound::TEnableAutoConverterNodeRegistration<FromDataType, RNBOMetasound::FMIDIBuffer>
{
    static constexpr bool Value = false;
};

REGISTER_METASOUND_DATATYPE(RNBOMetasound::FMIDIBuffer, "MIDIBuffer")

namespace RNBOMetasound {
FMIDIPacket::FMIDIPacket(int32 frame, size_t length, const uint8_t* data)
    : mFrame(frame)
    , mLength(static_cast<uint8_t>(std::min(length, mData.size())))
{
    for (auto i = 0; i < std::min(length, mData.size()); i++) {
        mData[i] = data[i];
    }
}

FMIDIBuffer::FMIDIBuffer(const Metasound::FOperatorSettings& InSettings)
    : NumFramesPerBlock(InSettings.GetNumFramesPerBlock())
{
}

void FMIDIBuffer::AdvanceBlock()
{
    Packets.RemoveAt(0, CountInBlock, false);
    CountInBlock = 0;

    const int32 cnt = Packets.Num();
    for (int32 i = 0; i < cnt; i++) {
        auto& packet = Packets[i];
        packet.Advance(NumFramesPerBlock);
        if (packet.Frame() < NumFramesPerBlock) {
            CountInBlock++;
        }
    }
}

int32 FMIDIBuffer::NumInBlock() const
{
    return CountInBlock;
}

const FMIDIPacket& FMIDIBuffer::operator[](int32 index) const
{
    return Packets[index];
}

void FMIDIBuffer::Push(FMIDIPacket packet)
{
    if (packet.Frame() < NumFramesPerBlock) {
        CountInBlock++;
    }
    Packets.Push(packet);
}

} // namespace RNBOMetasound
