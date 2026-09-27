#pragma once

#include "ModuleCommon.h"

namespace vk
{
/** DRIVE: tape / fuzz / clip / fold / bit waveshapers with level compensation and post low-pass. */
class DriveModule
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const DriveParams& p) noexcept { target = p; fade.request ((int) p.type); }
    void process (float& l, float& r) noexcept;

    static float shape (DriveType type, float x, float gain) noexcept;

private:
    double sr = 44100.0;
    DriveParams target;
    DriveType type = DriveType::off;
    SwitchFade fade;
    OnePole amountSm, postSm;
    Svf post[2];
    DcBlocker dc[2];
    float lastPost = -1.0f;
    int counter = 0;
};
} // namespace vk
