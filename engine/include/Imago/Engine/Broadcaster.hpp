/**
 * @file Broadcaster.hpp
 * @author Shane David
 * @brief Defines the Broadcaster to emit events for subscribed systems
 */

#include <cstdint> // for uint32_t 
#include <functional> // for std::function 
#include <unordered_map> // for std::unordered_map
#include <algorithm> // for std::find 
#include <cassert> // for assert
#include <type_traits> // for checking if T is a const reference in subscribe and unsubscribe 
#include <spdlog/spdlog.h> // for error logging 


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

        uint32_t _broadcastDepth; ///> counter for how many broadcasts emit is making

        /**
         * @brief Private helper function to find a subscriber given a vector of subscribers.
         * 
         * This is used to determine in subscribe if a subscriber already exists and in unsubscribe to determine if the subscription never existed in the first place. 
         * 
         * @param subscribers The vector of Subscribers to check
         * @param system The system to look for 
         * @return std::vector<Subscriber>::iterator pointing to the subscriber if it is found 
         */
        static std::vector<Subscriber>::iterator find_subscriber(std::vector<Subscriber>& subscribers, ErasedFn system); 
    
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
         * 
         * emit<T>() simply does nothing if there are no systems subscribed to a particular event. 
         * It also increments _broadcast depth for every emit currently happening. 
         * 
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

        /**
         * @brief Drops every subscriptoin for every event type. 
         */
        void clear(); 

    }; 

    //-----------
    //definitions
    //-----------

    template <typename T>
    void Broadcaster::emit(const T& event) 
    {
        // get the id at this event type
        EventTypeId id = get_event_type_id<T>(); 

        // if the event vector does not exist return 
        auto it = _subscribers.find(id); 
        if (it == _subscribers.end()) return; 

        // if we are here it was found so get a reference
        std::vector<Subscriber>& subscribers = it->second; 

        // iterate though it calling the lambgas and keeping track of broadcast depth accordingly 
        _broadcastDepth++; 
        for (const Subscriber& subscriber : subscribers) {
            subscriber.invoke(_ctx, &event);
        }
        _broadcastDepth--; 
    }

    template <typename T>
    void Broadcaster::subscribe(SystemFn<T> system) 
    {
        // make sure T is not a const reference so that ids do not get messaed up 
        static_assert(!std::is_const_v<T> && !std::is_reference_v<T>, "[Broadcaster] Event type T must not be const or a reference");

        //TODO: eventually have a system for this that just waits on the subscribe instead of crashing 
        // ensure ther are no broadcasts currently occuring 
        assert(_broadcastDepth == 0 && "[Broadcaster] you are trying to subscribe to an event while the Broadcaster is emitting!"); 

        //TODO: this should crash the game in a build come up for a solution with this with the logger
        // ensure the system is not null 
        assert(system != nullptr && "[Broadcaster] you are trying to subscribe a null system to an event!"); 

        // get the id of the event being subscribe dto
        EventTypeId id = get_event_type_id<T>(); 

        // create or get the vector the matches with id 
        std::vector<Subscriber>& subscribers = _subscribers[id]; 

        //NOTE: in c++ reinterpret_cast<T>() tells the compiler to treat the same bits as a different type with no conversion and no checking 
        // in this case, since we are casting function pointers the address (which is what we want) will stay the same and the type the compiler associates
        // with it will change so we can store all system addresses in the same vector. 

        // convert the system to ErasedFn to store its address and make sure that system is not alreayd in the vector 
        ErasedFn erasedSystem = reinterpret_cast<ErasedFn>(system);
        if (find_subscriber(subscribers, erasedSystem) != subscribers.end()) return; 

        // buld the Subscriber wrapper and add it the _subscribers
        Subscriber subscriber = {
            erasedSystem, 
            [system](SceneContext& ctx, const void* event) {
                system(ctx, *static_cast<const T*>(event)); 
            }
        };
        subscribers.push_back(subscriber); 
    }

    template <typename T>
    void Broadcaster::unsubscribe(SystemFn<T> system) 
    {
        // make sure T is not a const reference so that ids do not get messaed up 
        static_assert(!std::is_const_v<T> && !std::is_reference_v<T>, "[Broadcaster] Event type T must not be const or a reference");

        //TODO: eventually have a system for this that just waits on the subscribe instead of crashing 
        // ensure ther are no broadcasts currently occuring 
        assert(_broadcastDepth == 0 && "[Broadcaster] you are trying to unsubscribe from an event while the Broadcaster is emitting!"); 

        //TODO: intergrate spdlog into editor logger once it exists
        // get the id of the event and make sure it existt
        EventTypeId id = get_event_type_id<T>(); 
        auto it = _subscribers.find(id); 
        if (it == _subscribers.end()) {
            spdlog::warn("[Broadcaster] You are trying to unsubscribe from an event that the system is already not subscribed to"); 
            return; 
        }

        // get reference to the subscribers vector
        std::vector<Subscriber>& subscribers = it->second; 

        // convert to erasedFn so we can check the address
        ErasedFn erasedSystem = reinterpret_cast<ErasedFn>(system);
        
        //NOTE: remove if shifts the elements that do not match the condition in the lambda toward the front and return an iterator to where the end
        // would be if the ones that did match were gone. 

        // erase the subscriber from its vector if addresss match, storing whether or not something was removed
        auto newEnd = std::remove_if(subscribers.begin(), subscribers.end(), [erasedSystem](const Subscriber& s) { return s.systemAddress == erasedSystem ; }); 

        //TODO: intergrate spdlog into editor logger once it exists
        // if nothing changed, report and return 
        if (newEnd == subscribers.end()) {
            spdlog::warn("[Broadcaster] You are trying to unsubscribe from an event that the system is already not subscribed to"); 
            return; 
        }

        // if something changed, remove it 
        subscribers.erase(newEnd, subscribers.end()); 
    }
}