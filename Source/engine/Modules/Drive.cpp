#include "Drive.h"

namespace vk
{
void DriveModule::prepare (double sampleRate)
{
    sr = sampleRate;
    amountSm.setTime (0.03f, sr);
    postSm.setTime (0.03f, sr);
    fade.prepare (sr);
    for (auto& d : dc) d.prepare (sr);
    reset();
}

void DriveModule::reset()
{
    amountSm.reset (target.type == DriveType::off ? 0.0f : target.amount);
    postSm.reset (std::log2 (std::max (200.0f, target.postLP)));
    type = target.type;
    fade.current = fade.pending = (int) type;
    fade.gain = 1.0f;
    for (auto& p : post) p.reset();
    for (auto& d : dc) d.reset();
    lastPost = -1.0f;
}

float DriveModule::shape (DriveType t, float x, float g) noexcept
{
    const float v = x * g;
    switch (t)
    {
        case DriveType::tape:
            return std::tanh (v + 0.12f) - std::tanh (0.12f);
        case DriveType::fuzz:
            return v >= 0.0f ? 1.0f - std::exp (-v * 1.2f)
                             : -0.8f * (1.0f - std::exp (v * 1.6f));
        case DriveType::clip:
            return std::clamp (v, -0.9f, 0.9f);
        case DriveType::fold:
            return std::sin (v * 1.3f) * 0.85f;
        case DriveType::bit:
        {
            const float s = std::tanh (v);
            return std::round (s * 7.0f) / 7.0f;
        }
        default:
            return x;
    }
}

void DriveModule::process (float& l, float& r) noexcept
{
    const float amtTarget = target.type == DriveType::off ? 0.0f : target.amount;
    const float amount = amountSm.process (amtTarget);

    float typeLP = 20000.0f;
    switch (target.type)
    {
        case DriveType::fuzz: typeLP = 7500.0f; break;
        case DriveType::clip: typeLP = 12000.0f; break;
        case DriveType::bit:  typeLP = 10000.0f; break;
        case DriveType::fold: typeLP = 11000.0f; break;
        default: break;
    }
    const float postHz = std::exp2 (postSm.process (std::log2 (std::max (200.0f, std::min (target.postLP, typeLP)))));

    if (fade.tick())
    {
        type = (DriveType) fade.current;
        for (auto& p : post) p.reset();
        for (auto& d : dc) d.reset();
    }

    if (type == DriveType::off || (amount < 1.0e-4f && amtTarget <= 0.0f))
        return;

    const float gain = dbToGain (amount * 30.0f);
    const float comp = dbToGain (-amount * 15.0f) * (type == DriveType::fold ? 1.4f : 1.0f);
    const float blend = std::min (1.0f, amount * 5.0f) * fade.gain;

    float in[2] = { l, r };
    float out[2];
    for (int c = 0; c < 2; ++c)
        out[c] = dc[c].process (shape (type, in[c], gain)) * comp;

    if (postHz < 19000.0f)
    {
        if (--counter <= 0)
        {
            counter = 16;
            if (std::abs (postHz - lastPost) > 1.0f)
            {
                lastPost = postHz;
                post[0].set (postHz, 0.707f, sr);
                post[1].set (postHz, 0.707f, sr);
            }
        }
        out[0] = post[0].process (out[0], Svf::Mode::lp);
        out[1] = post[1].process (out[1], Svf::Mode::lp);
    }

    l = in[0] + (out[0] - in[0]) * blend;
    r = in[1] + (out[1] - in[1]) * blend;
}
} // namespace vk
