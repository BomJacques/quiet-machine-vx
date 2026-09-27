#pragma once

#include <array>
#include <cstddef>

namespace qm
{
enum class SourceIndex : std::size_t
{
    tape1,
    tape2,
    tape3,
    tape4,
    tape5,
    swell,
    rumble,
    piano,
    drone,
    sub,
    staticNoise,
    hiss,
    count
};

inline constexpr auto sourceCount = static_cast<std::size_t>(SourceIndex::count);

struct SourceOutput
{
    std::array<float*, sourceCount> left {};
    std::array<float*, sourceCount> right {};

    void add(SourceIndex source, int sample, float leftSample, float rightSample) const noexcept
    {
        const auto index = static_cast<std::size_t>(source);
        if (left[index] != nullptr)
            left[index][sample] += leftSample;
        if (right[index] != nullptr)
            right[index][sample] += rightSample;
    }
};
}
