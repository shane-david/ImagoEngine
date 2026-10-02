/**
 * @file Broadcaster.hpp
 * @author Shane David
 * @brief Defines the Broadcaster to emit events for subscribed systems
 */

#include <cstdint> // for uint32_t 
#include <functional> // for std::function 
#include <unordered_map> // for std::unordered_map

#pragma once

namespace Imago::Engine 
{

    //NOTE: this will be defined later and for now is just a forward declaration so that it can be used in the function signatures
    struct SceneContext; 

    /// @brief Alias for event ids in event storage 
    using EventTypeId = uint32_t; 

    //NOTE: this is again the meyers singleton, see Nexus.hpp for more details about how it works

    namespace detail 
    {   
        /**
         * @brief Returns the next sequential EventTypeId, starting at 0 
         * 
         * Backed by a single static counter shared across every call, regardless of caller. 
         * This should not be called directly. get_event_type_id<T>()  should be used instead to ensure
         * that each event type only ever consumes one sequential id. 
         * 
         * @return EventTypeId 
         */
        inline EventTypeId next_event_type_id() 
        {
            static EventTypeId id = 0;
            return id++; 
        }
    }

    /**
     * @brief Returns the EventTypeId for the event of type T
     * 
     * The id is assigned the first time this is called for a given event of type T. Sice the id 
     * is static the id is the same for every following call with that component type.
     * 
     * @tparam The event type
     * @return The id for that event type. 
     */
    template <typename T>
    EventTypeId get_event_type_id() 
    {
        static const EventTypeId id = detail::next_event_type_id(); 
        return id; 
    }

    /**
     * @brief 
     * 
     */
    class Broadcaster {
    private:
    
        /**
         * @brief Alias for the common function pointer type (returns nothing recieves nothing) that every system is cast to for type erasure.
         * It is only used to store and compare the address of different functions in unsubscribe, nothing is ever called by it. 
         */
        using ErasedFn = void(*)(); 
        
        /**
         * @brief Wrapper of a systems address and std::function storage built in subscribe<T> to keep track of subscribed systems
         */
        struct Subscriber {
            ErasedFn systemAddress;
            std::function<void(SceneContext&, const void*)> invoke; ///> the wrapper built in subscribe<T> that casts the event back to const T* and calls the system
        }; 

        SceneContext& _ctx; ///> reference to the SceneContext that will be passed to all subscribed systems
        std::unordered_map<EventTypeId, std::vector<Subscriber>> _subscribers; 

        //NOTE: this variable exists so we can keep track of how many broadcasts emit has made 
        //and not subscribe or unsubscribe from any systems during an emit

        uint32_t _broadcastDepth; ///> counter for how many broadcasts emit has made
    
    public:
        
        /**
         * @brief Function pointer to a system that recieves SceneContext and an EventType and returns nothing
         * @tparam The EventType. 
         */
        template <typename T>
        using SystemFn = void (*)(SceneContext&, const T&); 

        /**
         * @brief Construct a new Broadcaster object with the current SceneContext
         * @param ctx The current SceneContext.
         */
        explicit Broadcaster(SceneContext& ctx); 

        // explicitly disallow copy behavior
        Broadcaster(const Broadcaster&) = delete;
        Broadcaster& operator=(const Broadcaster&) = delete; 

        /**
         * @brief Call every system subscribed to EventType T in subcription order. 
         * @tparam The Event Type data to broadcast
         * @param event The Event to emit. 
         */
        template <typename T>
        void emit(const T& event); 

        /**
         * @brief Adds the system function pointer to the list for EventType T. Called during scene initialization. 
         * @tparam The Event to subscribe to 
         * @param system The functin pointer to the system that is subscribing. 
         */
        template <typename T>
        void subscribe(SystemFn<T> system); 

        /**
         * @brief Removes the system functinp pointer from the list for EventType T.
         * 
         * This preserves the order of the remaining subscribers. It finds the specific system to unsubscribe from 
         * by comparing the ErasedFn address to the one's of those in the event list. 
         * 
         * @tparam The Event to unsubscribe from. 
         * @param system The function pointer to the system that is unsubscribing. 
         */
        template <typename T>
        void unsubscribe(SystemFn<T> system); 

    }; 
}