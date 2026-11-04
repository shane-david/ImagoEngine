/**
 * @file GameConfig.hpp
 * @author Shane David
 * @brief Defines the settings Game is constructed with.
 */

#pragma once

#include <string> // for title setting

namespace Imago::Engine 
{
    /// @brief How the game limits its frame rate. Excactly one mode is ever active. 
    enum class FramePacing { VSync, Limited, Uncapped }; 

    /// @brief Everything the Game needs to start. Default values for now but will later be filled by a YAML config file. 
    struct GameConfig {
        std::string title = "ImagoEngine"; ///< window title
        unsigned int width = 1280; ///< width of the window
        unsigned int height = 720; ///< height of the window
        FramePacing pacing = FramePacing::VSync; ///< how the frame rate is limited. VSync by default. 
        unsigned int targetFps = 60; ///< only read when pacing is limited. 
        std::string firstScene; ///< name of the first Scene to load and run. 
    }; 
}