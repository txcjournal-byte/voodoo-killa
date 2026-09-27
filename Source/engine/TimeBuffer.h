#pragma once

#include "DspUtil.h"
#include "Preset.h"
#include <vector>

namespace vk
{
/** Stereo circular recording buffer. Always records the input; read heads index it by absolute sample number. */
class TimeBuffer
{
public:
    void prepare (double sampleRate, double seconds);
    void clear();

    void write (float l, float r) noexcept
    {
        const auto i = (size_t) (writeIndex & mask);
        left[i] = l;
        right[i] = r;
        ++writeIndex;
    }

    /** Absolute index of the most recently written sample. */
    int64_t newest() const noexcept { return writeIndex - 1; }
    int64_t capacity() const noexcept { return mask + 1; }

    /** Reads at an absolute (fractional) position, clamped to the valid recorded range. */
    void read (double pos, float& l, float& r) const noexcept
    {
        const double hi = (double) newest();
        const double lo = hi - (double) (mask - 8);
        pos = std::min (hi, std::max (lo, pos));
        l = readHermite (left, mask, pos);
        r = readHermite (right, mask, pos);
    }

private:
    std::vector<float> left, right;
    int64_t mask = 0;
    int64_t writeIndex = 0;
};

/**
    TIME module: owns the buffer and the read heads.

    For each sample the owner supplies the elapsed position inside the current trigger segment.
    The head position is always computed from the segment start S (= "now" - elapsed), so every
    segment starts in sync with the input and the engine can never drift.
    Changes of mode / segment / slice spawn a new head; the previous one keeps running at its last
    speed and is crossfaded out (two read heads -> no clicks).
*/
class TimeEngine
{
public:
    struct Frame
    {
        bool   active = false;     // trigger allows the effect
        double segStartPpq = 0.0;
        double elapsedBeats = 0.0; // ppq - segStartPpq
        double segLenBeats = 4.0;
        double samplesPerBeat = 22050.0;
    };

    void prepare (double sampleRate, double bufferSeconds = 34.0);
    void reset();

    /** Records the input sample, then replaces l/r with the time-manipulated output. */
    void process (float& l, float& r, const TimeParams& params, const Frame& frame) noexcept;

    /** Forces a crossfade to a freshly started head on the next sample (transport jump etc.). */
    void resync() noexcept { forceNewHead = true; }

    const TimeBuffer& getBuffer() const noexcept { return buffer; }

    /** Current read-head offset relative to "now" in samples (0 = in sync). For tests and UI. */
    double getCurrentOffset() const noexcept { return lastOffsetFromNow; }

private:
    struct Head
    {
        bool   grain = false;
        int    dir = 1;
        double pos = 0.0;      // direct: read position, grain: time position
        double speed = 1.0;
        float  gain = 1.0f;
        float  fade = 1.0f;
        float  fadeStep = 0.0f;
        double grainSrc[2] { 0.0, 0.0 };
        int    grainAge[2] { 0, 0 };
        int    grainCounter = 0;

        void startGrains (int grainLen) noexcept;
        void render (const TimeBuffer& buf, int grainLen, float& l, float& r) noexcept;
        void advance (int grainLen) noexcept;   // free-running (fading head)
    };

    struct Key
    {
        int     mode = 0;
        int64_t segment = 0;
        int64_t slice = 0;
        bool operator!= (const Key& o) const noexcept { return mode != o.mode || segment != o.segment || slice != o.slice; }
    };

    struct Target
    {
        double offset = 0.0;   // relative to segment start S (samples)
        double speed = 1.0;
        float  gain = 1.0f;
        bool   grain = false;
        int    dir = 1;
        int64_t slice = 0;
        double sliceLen = 0.0;
    };

    Target computeTarget (const TimeParams& p, double e, double L, double spb, int64_t segKey) noexcept;

    TimeBuffer buffer;
    double sr = 44100.0;
    int grainLen = 2048;
    int xfadeSamples = 441;

    Head current, previous;
    bool previousActive = false;
    bool started = false;
    Key currentKey;
    bool forceNewHead = true;

    // repeat state (per segment)
    int64_t repeatSegment = -1;
    double sliceStartE = 0.0, sliceLenE = 0.0;
    int64_t sliceIndex = 0;

    OnePole muteSmoother;
    double lastOffsetFromNow = 0.0;
};
} // namespace vk
