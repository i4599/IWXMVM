#pragma once

namespace IWXMVM::Types
{
    enum class GameState
    {
        MainMenu,
        InGame,
        InDemo,
        LoadingDemo
    };

    static inline std::string_view ToString(GameState state)
    {
        switch (state)
        {
            case GameState::MainMenu:
                return "Main Menu";
            case GameState::InDemo:
                return "Playing Demo";
            case GameState::InGame:
                return "In Game";
            case GameState::LoadingDemo:
                return "Loading Demo";
            default:
                return "Unknown Game";
        }
    }
}  // namespace IWXMVM::Types