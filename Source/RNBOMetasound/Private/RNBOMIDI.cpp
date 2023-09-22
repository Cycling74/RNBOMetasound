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

int32 FMIDIPacket::Frame() const
{
    return mFrame;
}
const std::array<uint8_t, 3>& FMIDIPacket::Data() const
{
    return mData;
}
size_t FMIDIPacket::Length() const
{
    return static_cast<size_t>(mLength);
}

void FMIDIPacket::Advance(int32 InNumFrames)
{
    mFrame -= InNumFrames;
}

FMIDIBuffer::FMIDIBuffer(const Metasound::FOperatorSettings& InSettings)
    : NumFramesPerBlock(InSettings.GetNumFramesPerBlock())
{
}

void FMIDIBuffer::AdvanceBlock()
{
    Packets.RemoveAt(0, CountInBlock, false);
    CountInBlock = 0;
    LastFrame = -1;

    const int32 cnt = Packets.Num();
    for (int32 i = 0; i < cnt; i++) {
        auto& packet = Packets[i];
        packet.Advance(NumFramesPerBlock);
        if (packet.Frame() < NumFramesPerBlock) {
            CountInBlock++;
        }
        LastFrame = packet.Frame();
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
    auto frame = packet.Frame();
    if (frame < NumFramesPerBlock) {
        CountInBlock++;
    }

    // simple push back or insertion sort
    if (frame >= LastFrame) {
        LastFrame = frame;
        Packets.Push(packet);
    }
    else {
        // insertion sort
        auto cnt = Packets.Num();
        for (auto i = 0; i < cnt; i++) {
            // insert before a packet that has a larger frame
            // new packets at the same frame will always come last
            // since the above if failed, we know this will never be the last packet
            // so we will always have an insert
            if (Packets[i].Frame() > frame) {
                Packets.Insert(packet, i);
                return;
            }
        }
        // assert false?
    }
}

void FMIDIBuffer::Reset()
{
    Packets.Reset();
    CountInBlock = 0;
}

} // namespace RNBOMetasound
