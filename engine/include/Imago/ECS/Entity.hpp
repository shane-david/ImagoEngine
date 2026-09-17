/**
 * @file Entity.hpp
 * @author Shane David
 * @brief Defines the Entity alias and the free functions used to pack and unpack it
 * 
 * An Entity is a lightweight identifier, a single packed uint32_t containing both an
 * index (which slot this entity occupies) and a generation (how many times that slot
 * has been destroyed and recreated). Entities carry no data and no behavior; they are
 * pure IDs used to look up components in the ECS's component pools.
 */
# pragma once 

#include <cstdint> // for uint32_t 
#include <cassert> // for error notification in MakeEntity

namespace Imago::ECS 
{

    /// @brief  A packed identifier: 20 bits of index, 12 bits of generation
    using Entity = uint32_t;

    // NOTE: for these variables, constexpr means that the variable is locked to its set value at compile time 
    // and that value is baked directly into the generated code everywhere it is used, meaning these values cannot be reassigned at any point 

    /// @brief Number of bits allocated to an Entity's index
    constexpr uint32_t ENTITY_INDEX_BITS      = 20; // low 20 bits represent the Entity's index
    /// @brief Number of bits allocated to an Entity's generation
    constexpr uint32_t ENTITY_GENERATION_BITS = 12; // top 12 bits represent the Entity's generation (how many times it has been destroyed and recreated)

    /// @brief Mask isolating the low 20 index bits of a packed Entity
    constexpr uint32_t ENTITY_INDEX_MASK      = (1u << ENTITY_INDEX_BITS) - 1;   
    /// @brief Mask isolating the low 12 bits of a right shifted generation value
    constexpr uint32_t ENTITY_GENERATION_MASK = (1u << ENTITY_GENERATION_BITS) -1;

    /**
     * @brief Sentinel value representing "no entity"
     * 
     * Decodes to index == ENTITY_INDEX_MASK and generation == ENTITY_GENERATION_MASK,
     * a bit pattern EntityManager never hands out as a real, live Entity. 
     */
    constexpr Entity NULL_ENTITY = 0xFFFFFFFFu;   // bits used to represent a null entitty 

    // NOTE: for these free functions, constexpr means that if every argument is known at compile time the compiler can simply 
    // run this function during compilation and bake in the result, meaning the value cannot be reassigned, and if every argument
    // is not known at compile time it simply treats it as a runtime function call

    /**
     * @brief Extracts the index bits from a packed Entity.
     * @param entity The packed Entity to decode.
     * @return The entity's index.
     */
    constexpr uint32_t GetEntityIndex(Entity entity) 
    {
        return entity & ENTITY_INDEX_MASK; 
    }


    /**
     * @brief Extracts the generation bits from a packed Entity.
     * @param entity The packed Entity to decode.
     * @return The entity's generation.
     */
    constexpr uint32_t GetEntityGeneration(Entity entity) 
    {
        return (entity >> ENTITY_INDEX_BITS) & ENTITY_GENERATION_MASK; 
    }

    /**
     * @brief Packs an index and generation into a single Entity.
     * @param index Index value.
     * @param generation Generation value.
     * @pre index <= ENTITY_INDEX_MASK
     * @pre generation <= ENTITY_GENERATION_MASK
     * @return The packed Entity.
     * @note Debug builds assert on out-of-range input; release builds silently mask it.
     */
    constexpr Entity MakeEntity(uint32_t index, uint32_t generation) 
    {
        // ensure that the Entity index and generation do not exceed their respective bit ranges
        assert(index <= ENTITY_INDEX_MASK && "[ENTITY] Entity index exceeds 20-bit range");
        assert(generation <= ENTITY_GENERATION_MASK && "[ENTITY] Entity generation exceeds 12-bit range");
        
        return ((generation & ENTITY_GENERATION_MASK) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK); 
    }

}