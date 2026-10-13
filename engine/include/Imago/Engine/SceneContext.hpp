/**
 * @file SceneContext.hpp
 * @author Shane David
 * @brief Defines the SceneContext, how systems reach persistent engine objects. 
 */

#pragma once

#include <string> // for storing action names
#include <unordered_map> // for storing inputs (action names -> boolean)

// NOTE: since SceneContext only stores pointers we just need to do forward declarations instead of full types 
// to avoid circular includes and make the .hpp file cheaper 
namespace sf { class RenderWindow; }
namespace Imago::ECS { class Nexus; }

namespace Imago::Engine 
{
    // internal forward declarations 
    class AssetManager; 
    class Broadcaster; 

    /**
     * @brief Gives systems access to the persisten engine objects they could need. 
     * 
     * This is created by the Game which will fill in what it owns (window, assets, input), and leave the pointers 
     * it does not own as nullptr. It will then pass it down through the SceneManager to the Scene which will fill in
     * the Nexus and the Broadcaster. 
     * Every pointer is a raw pointer because it is non-owning. 
     */
    struct SceneContext {
        ECS::Nexus* nexus = nullptr; ///< the active Scene's nexus
        Broadcaster* broadcaster = nullptr; ///< the active Scene's broadcaster
        AssetManager* assets = nullptr; ///< the loaded assets, owned by the Game
        sf::RenderWindow* window = nullptr; ///< the render window, owned by the Game 
        std::unordered_map<std::string, bool>* input = nullptr; ///< action name -> currently active input map 
    }; 
}