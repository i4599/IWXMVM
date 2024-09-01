#pragma once
#include "StdInclude.hpp"

namespace IWXMVM::UI
{
    class Animation
    {
       public:
        static Animation Create(float duration, float startVal, float endVal,
                                float (*interp)(float a, float b, float t));
        static void UpdateAll(float dt);

        Animation() = default;

        void GoForward() noexcept;
        void GoBackward() noexcept;
        void Invalidate() noexcept;
        float GetVal() const noexcept;
        bool ReachedEndVal() const noexcept;
        bool IsFinished() const noexcept;
        bool IsInvalid() const noexcept;

       private:
        Animation(std::size_t id) : id(id)
        {
        }

        std::size_t id = 0;
    };
}  // namespace IWXMVM::UI