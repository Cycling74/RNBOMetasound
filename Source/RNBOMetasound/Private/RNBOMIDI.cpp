#include "RNBOMIDI.h"

#include "MetasoundParamHelper.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundDataTypeRegistrationMacro.h"

#include <algorithm>

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

FMIDIPacket FMIDIPacket::NoteOn(int32 frame, uint8_t num, uint8_t vel, uint8_t chan)
{
    std::array<uint8_t, 3> data = { 0x90 | (chan & 0x0F), num, vel };
    return FMIDIPacket(frame, data.size(), data.data());
}

FMIDIPacket FMIDIPacket::NoteOff(int32 frame, uint8_t num, uint8_t vel, uint8_t chan)
{
    std::array<uint8_t, 3> data = { 0x80 | (chan & 0x0F), num, vel };
    return FMIDIPacket(frame, data.size(), data.data());
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

bool FMIDIPacket::IsNoteOff(uint8_t chan, uint8_t num) const
{
    return mLength == 3 && mData[0] == (chan | 0x80) && mData[1] == num;
}

bool FMIDIPacket::IsNoteOn(uint8_t chan, uint8_t num) const
{
    return mLength == 3 && mData[0] == (chan | 0x90) && mData[1] == num;
}

FMIDIPacket FMIDIPacket::CloneTo(int32 frame) const
{
    FMIDIPacket r = *this;
    r.mFrame = frame;
    return r;
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

void FMIDIBuffer::PushNote(int32 start, int32 dur, uint8_t chan, uint8_t note, uint8_t onvel, uint8_t offvel)
{
    /* situations where we have to move an OFF
     * 1. Our new note happens inside another scheduled duration
     *    * off after our off without an on after our off, move old off right before our new on
     * 2. Our new note spans the off of another note
     *    * off between our on and our off, move old off to right before our new on
     * 3. Our new note spans the an entire note
     *    * on and off come between our new on and off, move old off right before our new on
     *
     */
    const auto count = Packets.Num();
    const auto end = start + dur;
    for (auto i = 0; i < count; i++) {
        auto& p = Packets[i];
        const auto f = p.Frame();
        if (f < start) {
            continue;
        }
        // in range
        if (p.IsNoteOn(chan, note)) {
            // existing note starts before our note ends, we need to shorten our note
            if (f < end) {
                // insert our Off before the existing On, updated time
                Packets.Insert(FMIDIPacket::NoteOff(f, note, offvel, chan), i);
                // adjust count
                if (f < NumFramesPerBlock) {
                    CountInBlock++;
                }
                // push our on after so we don't screw up above index
                Push(FMIDIPacket::NoteOn(start, note, onvel, chan));
                return;
            }
            else {
                break;
            }
        }
        else if (p.IsNoteOff(chan, note) && f > start) {
            // should only hit this case if a note on isn't found either within the new note bounds or after it.
            // our new note either spans the off of an existing on note or is contained within an existing note
            // move the off just before our on
            auto off = p.CloneTo(start);
            Packets.RemoveAt(i);

            // adjust count
            if (f < NumFramesPerBlock) {
                CountInBlock--; // will get incremented in the Push below
            }
            // adjust last frame
            if (Packets.Num() > 0) {
                LastFrame = Packets.Last(0).Frame();
            }
            else {
                LastFrame = -1;
            }
            Push(off); // XXX assumes there isn't another matching On at the exact time
            break;
        }
    }

    Push(FMIDIPacket::NoteOn(start, note, onvel, chan));
    Push(FMIDIPacket::NoteOff(end, note, offvel, chan));
}

void FMIDIBuffer::Reset()
{
    Packets.Reset();
    CountInBlock = 0;
}

} // namespace RNBOMetasound
