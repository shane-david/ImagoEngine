/**
 * @file SceneManager.hpp
 * @author Shane David
 * @brief Defines the SceneManager, which is responsible for creating, switching between, and updating Scenes
 */

#pragma once

#include <memory> // for std::unique_ptr to store the Scenes
#include <string> // for scene names
#include <unordered_map> // to store the live scenes and scene factories
#include <type_traits> // for std::is... methods in register static asserts

#include "Imago/Engine/Scene.hpp"

namespace Imago::Engine 
{

    /**
     * @brief The manager and the only object that creates, switches, and updates Scenes.
     * 
     * Scenes are registerd by name with register_scene and built on first use. Only one Scene can 
     * be active at a time. Switching is defferred to the end of the current frame. 
     */
    class SceneManager {
    public:
        
        // NOTE: SceneFactoryFn is a function pointer to an instanc of the static create_scene() method that exists for each 
        // registered Scene Type. Creating a Scene requires knowing its concrete type at compile time but not only is the Scene
        // to build chosen at Runtime but the SceneManager cannot even know what user-defined Scenes even exist at compile time. 
        // To solve this we use factories. register_scene<T> is the one place that the Scene type is known at copile time so it 
        // instantiates create_scene<T> and stores a pointer ot it. The SceneManager can then just call the stored function 
        // without even having to know what type the Scene it is instantiating is. 

        /// @brief A function pointer to a function that builds one specific Scene type. One is instantiated per registered type. 
        using SceneFactoryFn = std::unique_ptr<Scene>(*)(const SceneContext&); 

    private:
        
        /**
         * @brief The factory that register_scene stores for T. Builds a T and returns it as a Scene. 
         * @tparam T The type of the Scene to build. 
         * @param gameCtx SceneContext filled with the global engine objects Game owns. 
         * @return std::unique_ptr<Scene> The Scene that was built.
         */
        template <typename T>
        static std::unique_ptr<Scene> create_scene(const SceneContext& gameCtx); 

        /**
         * @brief Stores a factory under a name. Asserts the name is not already registered.
         * @param name The name of the factory. 
         * @param factory the function pointer to the create_scene<T> factory. 
         */
        void add_factory(const std::string& name, SceneFactoryFn factory); 

        const SceneContext& _gameCtx; ///< The Game Context filled with the global engine objects Game owns. Copied to every Scene's individual context.

        // scene storage

        std::unordered_map<std::string, SceneFactoryFn> _factories; ///< function pointers to instances of create_scene 
        std::unordered_map<std::string, std::unique_ptr<Scene>> _scenes; ///< every live scene, whether active or suspended 
        Scene* _active = nullptr; ///< the current active Scene, raw pointer because it is non-owning since it lives in _scenes
        std::string _activeName; ///< the name of the current active scene, empty if none. 

        // switch data

        bool _switchPending; ///< whether or not a switch has been requested
        std::string _pendingName; ///< the name of the scene to switch to
        bool _pendingPersist = false; ///< whether or not to suspend or destroy the scene after a witch

    public:

        /**
         * @brief Construct a new SceneManager object, instantiating gameCtx so it can be copied into every Scene it builds.
         * @param gameCtx SceneContext filled with the global engine objecst that the Game owns. 
         */
        explicit SceneManager(const SceneContext& gameCtx); 

        /**
         * @brief Unloads the active scene then destroys every live scene in _scenes. 
         */
        ~SceneManager(); 

        // explicitly dissallow copy behavior 
        SceneManager(const SceneManager&) = delete; 
        SceneManager& operator=(const SceneManager&) = delete; 

        /**
         * @brief Registers a Scene type under a name. The scene is not build on registration but instead a factory to a method that builds the Scene 
         * is stored and then called when that Scene is switched to for the first time. 
         * @tparam T Type of Scene to register. 
         * @param name Name of the Scene. 
         */
        template <typename T>
        void register_scene(const std::string& name); 

        /**
         * @brief Records a request to switch scenes. The switch is not actually applied until the end of the next frame.
         * If several switch requests occur during the same frame, the last one wins. 
         * @param name The name of the registered scene to switch to. 
         * @param persist Whether or not the scene should be suspended (true) or completely destroyed (false).
         */
        void request_switch(const std::string& name, bool persist); 

        /**
         * @brief Actually switch the pending Scene. Called by Game at the end of each frame. 
         */
        void apply_pending_switch(); 

        /**
         * @brief Updates the active scene. Does nothing if there is no active Scene. 
         * @param dt Real time since the last frame in seconds. 
         */
        void update(float dt); 
    }; 

    //-----------
    //definitions
    //-----------

    template <typename T>
    std::unique_ptr<Scene> SceneManager::create_scene(const SceneContext& gameCtx) 
    {
        return std::make_unique<T>(gameCtx); 
    }

    template <typename T>
    void SceneManager::register_scene(const std::string& name) 
    {
        // ensure at compile time that the T passed in is a Scene
        static_assert(std::is_base_of_v<Scene, T>, "[SceneManager] You cannot register an object that does not extend Scene!"); 

        // ensure at compile time that the T passed in has overridden on_load
        static_assert(!std::is_abstract_v<T>, "[SceneManager] You cannot register a Scene that has not overridden on_load()!");

        // ensure at compile time that the T passed in can be built from a SceneContext
        static_assert(std::is_constructible_v<T, const SceneContext&>, "[SceneManager] The Scene you are trying to register is not constructible. Use 'using Scene::Scene'"); 

        // register the scene
        add_factory(name, &create_scene<T>); 
    }
}