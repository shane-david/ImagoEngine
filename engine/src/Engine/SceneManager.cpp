#include "Imago/Engine/SceneManager.hpp"

#include <cassert> // to assert for
#include <spdlog/spdlog.h> // for temporary warning logging
#include <utility> // for std::move 

namespace Imago::Engine 
{
    // constructor
    SceneManager::SceneManager(const SceneContext& gameCtx)
        : _gameCtx(gameCtx) { }

    // destructor
    SceneManager::~SceneManager() 
    {
        // clean up the active scene if it exists when the SceneManager is destroy
        if (_active != nullptr) {
            _active->unload(); 
        }
    }

    // methods

    void SceneManager::update(float dt) 
    {
        // do nothing if there is not active scene
        if (_active == nullptr) return; 

        // otherwise update the active scene
        _active->update(dt); 
    }

    void SceneManager::add_factory(const std::string& name, SceneFactoryFn factory)
    {   
        //TODO: integrate the asserts with the logger once it exists

        // make sure a not null function pointer is being passed in 
        assert(factory != nullptr && "[SceneManager] you cannot add a facotry with a null factory function pointer!"); 

        // make sure that the factory does not already exist 
        auto it = _factories.find(name);
        assert(it == _factories.end() && "[SceneManager] the factory you are trying to add already exists"); 

        // store it in the factories map
        _factories[name] = factory; 
    }

    void SceneManager::request_switch(const std::string& name, bool persist) 
    {
        // TODO integrate warning and assert into the logger once it exists
        
        // try to find the name in factories
        auto it = _factories.find(name); 

        // if the name does not exist warn 
        assert(it != _factories.end() && "[SceneManager] you are trying to switch to a Scene that has not been registered!"); 
        if (it == _factories.end()) {
            spdlog::warn("[SceneManager] you are trying to switch to a Scene that has not been registered!"); 
            return; 
        }

        // set class-wide switch data
        _switchPending = true; 
        _pendingName = name; 
        _pendingPersist = persist; 
    }

    void SceneManager::apply_pending_switch()
    {
        // do not apply if there is not a switch pending
        if (!_switchPending) return; 

        // copy class-wide switch data in locals so it does not ruin the switch if it is changed during this method's execution
        std::string target = _pendingName;
        bool persist = _pendingPersist; 
        _switchPending = false; 

        // if the target is the current active scene or the scene should not persist, unload and remove the scene from the live map
        if (_active != nullptr && (target == _activeName || !persist)) {
            _active->unload();
            _scenes.erase(_activeName); 
        }

        _active = nullptr; 
        _activeName.clear(); 

        // if the target already exists in scenes simply reactivate it 
        auto it = _scenes.find(target);
        if (it != _scenes.end()) {
            _active = it->second.get(); 
            _activeName = target; 
            return; 
        }

        // if it does not already exist try to find the factory
        auto factoryIt = _factories.find(target);

        //TODO: integrate assert into logger 
        assert(factoryIt != _factories.end() && "[SceneManage] You are tyring to switch to a Scene that has not been registered!");

        // build the scene from the stored function pointer
        std::unique_ptr<Scene> scene = factoryIt->second(_gameCtx); 

        // save tha raw pointer into active scene (happens before move because a moved-from unique_ptr is null)
        _active = scene.get(); 
        _activeName = target; 

        // move ownership to the map
        _scenes[target] = std::move(scene); 

        // load the scene
        _active->load(); 
    }
}

