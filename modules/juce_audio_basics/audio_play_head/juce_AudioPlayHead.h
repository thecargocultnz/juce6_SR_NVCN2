/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
/**
    A subclass of AudioPlayHead can supply information about the position and
    status of a moving play head during audio playback.

    One of these can be supplied to an AudioProcessor object so that it can find
    out about the position of the audio that it is rendering.

    @see AudioProcessor::setPlayHead, AudioProcessor::getPlayHead

    @tags{Audio}
*/
class JUCE_API  AudioPlayHead
{
protected:
    //==============================================================================
    AudioPlayHead() = default;

public:
    virtual ~AudioPlayHead() = default;

    //==============================================================================
    /** Frame rate types. */
    enum FrameRateType
    {
        fps23976        = 0,
        fps24           = 1,
        fps25           = 2,
        fps2997         = 3,
        fps30           = 4,
        fps2997drop     = 5,
        fps30drop       = 6,
        fps60           = 7,
        fps60drop       = 8,
        fps47952        = 9,
        fps48           = 10,
        fps50           = 11,
        fps5994         = 12,
        fps5994drop     = 13,
        fps100          = 16,
        fps11988        = 17,
        fps11988drop    = 18,
        fps120          = 19,
        fps120drop      = 20,
        fpsUnknown      = 99
    };

    //==============================================================================
    /** This structure is filled-in by the AudioPlayHead::getCurrentPosition() method.
    */
    struct JUCE_API  CurrentPositionInfo
    {
        /** The tempo in BPM */
        double bpm;

        /** Time signature numerator, e.g. the 3 of a 3/4 time sig */
        int timeSigNumerator;
        /** Time signature denominator, e.g. the 4 of a 3/4 time sig */
        int timeSigDenominator;

        /** The current play position, in samples from the start of the timeline. */
        int64 timeInSamples;
        /** The current play position, in seconds from the start of the timeline. */
        double timeInSeconds;

        /** For timecode, the position of the start of the timeline, in seconds from 00:00:00:00. */
        double editOriginTime;

        /** The current play position, in samples from the start of processing.
            Without looping.
            Note - this value may be unavailable on some hosts.
            @see isContinuousValid.
         */
        int64 continuousTimeInSamples;

        /** The current play position, in units of quarter-notes. */
        double ppqPosition;

        /** The position of the start of the last bar, in units of quarter-notes.

            This is the time from the start of the timeline to the start of the current
            bar, in ppq units.

            Note - this value may be unavailable on some hosts, e.g. Pro-Tools. If
            it's not available, the value will be 0.
        */
        double ppqPositionOfLastBarStart;

        /** The video frame rate, if applicable. */
        FrameRateType frameRate;

        /** True if the transport is currently playing. */
        bool isPlaying;

        /** True if the transport is currently recording.

            (When isRecording is true, then isPlaying will also be true).
        */
        bool isRecording;

        /** The current cycle start position in units of quarter-notes.
            Note that not all hosts or plugin formats may provide this value.
            @see isLooping
        */
        double ppqLoopStart;

        /** The current cycle end position in units of quarter-notes.
            Note that not all hosts or plugin formats may provide this value.
            @see isLooping
        */
        double ppqLoopEnd;

        /** True if the transport is currently looping. */
        bool isLooping;

        /** True if the continuous time is valid/supported. */
        bool isContinuousValid;

        //==============================================================================
        bool operator== (const CurrentPositionInfo& other) const noexcept
        {
            return timeInSamples == other.timeInSamples
                && ppqPosition == other.ppqPosition
                && editOriginTime == other.editOriginTime
                && ppqPositionOfLastBarStart == other.ppqPositionOfLastBarStart
                && frameRate == other.frameRate
                && isPlaying == other.isPlaying
                && isRecording == other.isRecording
                && bpm == other.bpm
                && timeSigNumerator == other.timeSigNumerator
                && timeSigDenominator == other.timeSigDenominator
                && ppqLoopStart == other.ppqLoopStart
                && ppqLoopEnd == other.ppqLoopEnd
                && isLooping == other.isLooping;
        }

        bool operator!= (const CurrentPositionInfo& other) const noexcept
        {
            return ! operator== (other);
        }

        void resetToDefault()
        {
            zerostruct (*this);
            timeSigNumerator = 4;
            timeSigDenominator = 4;
            bpm = 120;
        }
    };

    //==============================================================================
    /** Fills-in the given structure with details about the transport's
        position at the start of the current processing block. If this method returns
        false then the current play head position is not available and the given
        structure will be undefined.

        You can ONLY call this from your processBlock() method! Calling it at other
        times will produce undefined behaviour, as the host may not have any context
        in which a time would make sense, and some hosts will almost certainly have
        multithreading issues if it's not called on the audio thread.
    */
    virtual bool getCurrentPosition (CurrentPositionInfo& result) = 0;

    /** Returns true if this object can control the transport. */
    virtual bool canControlTransport()                         { return false; }

    /** Starts or stops the audio. */
    virtual void transportPlay (bool shouldStartPlaying)       { ignoreUnused (shouldStartPlaying); }

    /** Starts or stops recording the audio. */
    virtual void transportRecord (bool shouldStartRecording)   { ignoreUnused (shouldStartRecording); }

    /** Rewinds the audio. */
    virtual void transportRewind()                             {}
};

} // namespace juce
