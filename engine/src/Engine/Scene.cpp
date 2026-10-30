#include "Imago/Engine/Scene.hpp"

namespace Imago::Engine 
{
    Scene::Scene(const SceneContext& gameCtx)
        : _ctx(gameCtx), _broadcaster(_ctx), _scheduler(_ctx)
        {
            _ctx.nexus = &_nexus;
            _ctx.broadcaster = &_broadcaster; 
        }

    void Scene::load() 
    {
        on_load(); 
        _nexus.flush(); 
    }

    void Scene::unload() 
    {   
        //TODO: get rid of persist behaviour when the scene is completely destroyed 
        on_unload();
        _nexus.flush(); 
    }

    void Scene::update(float dt) 
    {   
        // call all the systems
        _scheduler.tick(dt); 

        // at the end of the frame flush all the deferred commands
        _nexus.flush(); 

        //TODO figure out where rendering should be called 
    }
}