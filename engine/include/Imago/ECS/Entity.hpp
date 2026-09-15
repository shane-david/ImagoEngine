# pragma once 

#include <cstdint> // for uint32_t 
#include <cassert> // for error notification in MakeEntity

// Defines the Entity alias and the free functions used to pack/unpack it along with the bit-layout constraints that will all use. 
// Scoped to Imago::ECS
namespace Imago::ECS 
{

    // Entity alias - Entities are just a packed uint32_t containing index and generation bits
    using Entity = uint32_t;

    // NOTE: for these variables, constexpr means that the variable is locked to its set value at compile time 
    // and that value is baked directly into the generated code everywhere it is used, meaning these values cannot be reassigned at any point 

    constexpr uint32_t ENTITY_INDEX_BITS      = 20; // low 20 bits represent the Entity's index
    constexpr uint32_t ENTITY_GENERATION_BITS = 12; // top 12 bits represent the Entity's generation (how many times it has been destroyed and recreated)

    constexpr uint32_t ENTITY_INDEX_MASK      = (1u << ENTITY_INDEX_BITS) - 1;     // index mask to isolate low 20 bits 
    constexpr uint32_t ENTITY_GENERATION_MASK = (1u << ENTITY_GENERATION_BITS) -1; // generation mask to isolate low 12 bits (must be preceded by a right shift)

    constexpr Entity NULL_ENTITY = 0xFFFFFFFFu;   // bits used to represent a null entitty 

    // NOTE: for these free functions, constexpr means that if every argument is known at compile time the compiler can simply 
    // run this function during compilation and bake in the result, meaning the value cannot be reassigned, and if every argument
    // is not known at compile time it simply treats it as a runtime function call

    // takes an entity and zeros out the generation bits to just return the Entity's index
    constexpr uint32_t GetEntityIndex(Entity entity) 
    {
        return entity & ENTITY_INDEX_MASK; 
    }

    // takes an entity and shifts the generation bits right then zeros out the remaining bits to just return the Entity's generation
    constexpr uint32_t GetEntityGeneration(Entity entity) 
    {
        return (entity >> ENTITY_INDEX_BITS) & ENTITY_GENERATION_MASK; 
    }

    // takes an index and it's generation and packs it into a single Entity
    constexpr Entity MakeEntity(uint32_t index, uint32_t generation) 
    {
        // ensure that the Entity index and generation do not exceed their respective bit ranges
        assert(index <= ENTITY_INDEX_MASK && "[ENTITY] Entity index exceeds 20-bit range");
        assert(generation <= ENTITY_GENERATION_MASK && "[ENTITY] Entity generation exceeds 12-bit range");
        
        return ((generation & ENTITY_GENERATION_MASK) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK); 
    }

}