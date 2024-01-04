#pragma once

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include <array>

namespace RNBOMetasound {

class RNBOMETASOUND_API FMIDIPacket
{
  public:
    FMIDIPacket(int32 frame, size_t length, const uint8_t* data);
    FMIDIPacket(const FMIDIPacket&) = default;

    // expects values to be in range
    static FMIDIPacket NoteOn(int32 frame, uint8_t num, uint8_t vel, uint8_t chan);
    static FMIDIPacket NoteOff(int32 frame, uint8_t num, uint8_t vel, uint8_t chan);

    int32 Frame() const;
    const std::array<uint8_t, 3>& Data() const;
    size_t Length() const;

    /** Advance internal frame counters by specific frame count. */
    void Advance(int32 InNumFrames);

    bool IsNoteOff(uint8_t chan, uint8_t num) const;
    bool IsNoteOn(uint8_t chan, uint8_t num) const;

    // Make a copy with an updated frame
    FMIDIPacket CloneTo(int32 frame) const;

  private:
    int32 mFrame;
    std::array<uint8_t, 3> mData;
    uint8_t mLength;
};

class RNBOMETASOUND_API FMIDIBuffer
{
  public:
    FMIDIBuffer(const Metasound::FOperatorSettings& InSettings);

    /** Advance internal frame counters by block size. */
    void AdvanceBlock();

    int32 NumInBlock() const;

    /** Returns packet for a given index.
     *
     * @param InIndex - Index of packet. Must be a value between 0 and Num().
     *
     * @return The packet
     */
    const FMIDIPacket& operator[](int32 index) const;

    void Push(FMIDIPacket packet);

    // Pushes a new note and manages updating any overlapping matching notes
    void PushNote(int32 start, int32 dur, uint8_t chan, uint8_t note, uint8_t onvel, uint8_t offvel);

    void Reset();

  private:
    int32 NumFramesPerBlock = 0;
    int32 CountInBlock = 0;
    int32 LastFrame = -1;

    TArray<FMIDIPacket> Packets;
};
} // namespace RNBOMetasound

DECLARE_METASOUND_DATA_REFERENCE_TYPES(RNBOMetasound::FMIDIBuffer, RNBOMETASOUND_API, FMIDIBufferTypeInfo, FMIDIBufferReadRef, FMIDIBufferWriteRef);
