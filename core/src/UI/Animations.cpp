#include "StdInclude.hpp"
#include "Animations.hpp"

namespace IWXMVM::UI
{
    struct AnimInfo
    {
        float duration;  // in seconds
        float startVal;
        float endVal;
        float t;  // in the range [0, 1]
        float (*interp)(float a, float b, float t);
        bool ending;
    };

    static constexpr std::size_t MAX_ANIMS = 512;
    static constexpr std::size_t INVALID_ANIM_ID = 0;

    static AnimInfo anims[MAX_ANIMS];
    static std::size_t curr_id = 0;

    Animation Animation::Create(float duration, float startVal, float endVal,
                                float (*interp)(float a, float b, float t))
    {
        curr_id = (curr_id + 1) % MAX_ANIMS;
        if (curr_id == 0)
            curr_id = 1;

        // Try to find an available slot for a new animation
        bool failedToFind = false;
        if (anims[curr_id].interp != nullptr)
        {
            failedToFind = true;
            for (std::size_t i = 1; i < MAX_ANIMS; i++)
            {
                std::size_t new_id = (curr_id + i) % MAX_ANIMS;
                if (new_id == 0)
                {
                    new_id = 1;
                }

                if (anims[new_id].interp == nullptr)
                {
                    curr_id = new_id;
                    break;
                }
            }
        }

        if (anims[curr_id].interp == nullptr && failedToFind)
        {
            // Failed to find an available slot
            return INVALID_ANIM_ID;
        }

        anims[curr_id] = {
            .duration = duration,
            .startVal = startVal,
            .endVal = endVal,
            .t = 0.0f,
            .interp = interp,
            .ending = false,
        };

        return curr_id;
    }

    void Animation::UpdateAll(float dt)
    {
        for (std::size_t i = 1; i < MAX_ANIMS; i++)
        {
            if (!anims[i].interp || anims[i].duration <= 0.0f)
                continue;

            if (!anims[i].ending)
            {
                anims[i].t += dt / anims[i].duration;

                if (anims[i].t > 1.0f)
                    anims[i].t = 1.0f;
            }
            else
            {
                anims[i].t -= dt / anims[i].duration;

                if (anims[i].t < 0.0f)
                    anims[i].t = 0.0f;
            }
        }
    }

    void Animation::GoForward() noexcept
    {
        anims[id].ending = false;
    }

    void Animation::GoBackward() noexcept
    {
        anims[id].ending = true;
    }

    void Animation::Invalidate() noexcept
    {
        anims[id].interp = nullptr;
        id = INVALID_ANIM_ID;
    }

    float Animation::GetVal() const noexcept
    {
        return anims[id].interp(anims[id].startVal, anims[id].endVal, anims[id].t);
    }

    bool Animation::ReachedEndVal() const noexcept
    {
        return anims[id].t == 1.0f;
    }

    bool Animation::IsFinished() const noexcept
    {
        return anims[id].ending && anims[id].t == 0.0f;
    }

    bool Animation::IsInvalid() const noexcept
    {
        return id == INVALID_ANIM_ID;
    }

}  // namespace IWXMVM::UI