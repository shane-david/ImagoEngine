#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <random>
#include <vector>

#include "Imago/ECS/Nexus.hpp"

using namespace Imago::ECS;

namespace
{
    struct Position { int x; };
    struct Velocity { int dx; };
    struct Health   { int hp; };
    struct Armor    { int value; };

    using Entities = std::vector<Entity>;

    // gathers every Entity a handle yields, sorted so tests never depend on the group's internal order
    template <typename Handle>
    Entities collect(Handle handle)
    {
        Entities result;
        for (Entity e : handle) {
            result.push_back(e);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    Entities sorted(Entities entities)
    {
        std::sort(entities.begin(), entities.end());
        return entities;
    }
}

TEST_CASE("A Bond created over existing components starts with the right group", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();
    Entity e3 = nexus.create();
    Entity e4 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Position>(e2, Position{ 2 });
    nexus.bind<Position>(e3, Position{ 3 });
    nexus.bind<Velocity>(e2, Velocity{ 20 });
    nexus.bind<Velocity>(e3, Velocity{ 30 });
    nexus.bind<Velocity>(e4, Velocity{ 40 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e2, e3 }));
}

TEST_CASE("An Entity joins the group once it has every bonded component", "[Bond]")
{
    Nexus nexus;
    Entity e = nexus.create();

    // create the Bond before any components exist, like a scene would at setup
    REQUIRE(collect(nexus.bond<Position, Velocity>()).empty());

    SECTION("Position first, then Velocity")
    {
        nexus.bind<Position>(e, Position{ 1 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()).empty());

        nexus.bind<Velocity>(e, Velocity{ 2 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e });
    }

    SECTION("Velocity first, then Position")
    {
        nexus.bind<Velocity>(e, Velocity{ 2 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()).empty());

        nexus.bind<Position>(e, Position{ 1 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e });
    }

    SECTION("both bound in the same flush")
    {
        nexus.bind<Position>(e, Position{ 1 });
        nexus.bind<Velocity>(e, Velocity{ 2 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e });
    }
}

TEST_CASE("Unbinding a bonded component removes the Entity from the group", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();
    Entity e3 = nexus.create();

    int i = 1;
    for (Entity e : { e1, e2, e3 }) {
        nexus.bind<Position>(e, Position{ i });
        nexus.bind<Velocity>(e, Velocity{ i * 10 });
        ++i;
    }
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2, e3 }));

    SECTION("unbinding from the first Entity")
    {
        nexus.unbind<Position>(e1);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e2, e3 }));
        REQUIRE(nexus.has<Velocity>(e1));
        REQUIRE(nexus.get<Position>(e2).x == 2);
        REQUIRE(nexus.get<Velocity>(e3).dx == 30);
    }

    SECTION("unbinding from the middle Entity")
    {
        nexus.unbind<Velocity>(e2);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e3 }));
        REQUIRE(nexus.has<Position>(e2));
        REQUIRE(nexus.get<Position>(e1).x == 1);
        REQUIRE(nexus.get<Velocity>(e3).dx == 30);
    }

    SECTION("unbinding from the last Entity")
    {
        nexus.unbind<Position>(e3);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2 }));
        REQUIRE(nexus.get<Position>(e1).x == 1);
        REQUIRE(nexus.get<Velocity>(e2).dx == 20);
    }

    SECTION("unbinding from every Entity empties the group")
    {
        nexus.unbind<Position>(e1);
        nexus.unbind<Position>(e2);
        nexus.unbind<Position>(e3);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()).empty());
    }

    SECTION("rebinding returns the Entity to the group")
    {
        nexus.unbind<Position>(e2);
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e3 }));

        nexus.bind<Position>(e2, Position{ 99 });
        nexus.flush();
        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2, e3 }));
        REQUIRE(nexus.get<Position>(e2).x == 99);
    }
}

TEST_CASE("Destroying an Entity removes it from the group", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();
    Entity e3 = nexus.create();
    Entity partial = nexus.create();

    int i = 1;
    for (Entity e : { e1, e2, e3 }) {
        nexus.bind<Position>(e, Position{ i });
        nexus.bind<Velocity>(e, Velocity{ i * 10 });
        ++i;
    }
    nexus.bind<Position>(partial, Position{ 4 }); // has only one of the two components
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2, e3 }));

    SECTION("destroying a grouped Entity")
    {
        nexus.destroy(e1);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e2, e3 }));
        REQUIRE(nexus.get<Position>(e2).x == 2);
        REQUIRE(nexus.get<Velocity>(e3).dx == 30);
        REQUIRE(nexus.get<Position>(partial).x == 4);
    }

    SECTION("destroying an Entity that is not in the group")
    {
        nexus.destroy(partial);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2, e3 }));
        REQUIRE(nexus.get<Position>(e1).x == 1);
    }

    SECTION("destroying every Entity")
    {
        nexus.destroy(e1);
        nexus.destroy(e2);
        nexus.destroy(e3);
        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity>()).empty());
        REQUIRE(nexus.get<Position>(partial).x == 4);
    }
}

TEST_CASE("Component data stays with the right Entity while the group reorders", "[Bond]")
{
    Nexus nexus;
    Entities entities;

    for (int i = 0; i < 10; ++i) {
        Entity e = nexus.create();
        entities.push_back(e);
        nexus.bind<Position>(e, Position{ i });
        if (i % 2 == 0) {
            nexus.bind<Velocity>(e, Velocity{ i * 10 });
        }
    }
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()).size() == 5);

    // complete the odd Entities one at a time so the group grows and reorders repeatedly
    for (int i = 1; i < 10; i += 2) {
        nexus.bind<Velocity>(entities[i], Velocity{ i * 10 });
        nexus.flush();
    }

    REQUIRE(collect(nexus.bond<Position, Velocity>()).size() == 10);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(nexus.get<Position>(entities[i]).x == i);
        REQUIRE(nexus.get<Velocity>(entities[i]).dx == i * 10);
    }
}

TEST_CASE("Requesting the same Bond again, in any order, returns the same group", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Velocity>(e1, Velocity{ 10 });
    nexus.bind<Position>(e2, Position{ 2 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e1 });
    REQUIRE(collect(nexus.bond<Velocity, Position>()) == Entities{ e1 });
    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e1 }); // repeated, like calling it every frame

    nexus.bind<Velocity>(e2, Velocity{ 20 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Velocity, Position>()) == sorted({ e1, e2 }));
}

TEST_CASE("Surveys over bonded component types still work", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();
    Entity e3 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Velocity>(e1, Velocity{ 10 });
    nexus.bind<Position>(e2, Position{ 2 });
    nexus.bind<Position>(e3, Position{ 3 });
    nexus.bind<Velocity>(e3, Velocity{ 30 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e3 }));

    REQUIRE(collect(nexus.survey<Position, Velocity>()) == sorted({ e1, e3 }));
    REQUIRE(collect(nexus.survey<Position>()) == sorted({ e1, e2, e3 }));
    REQUIRE(collect(nexus.survey<Velocity>()) == sorted({ e1, e3 }));

    // the Bond reorders pools as components change, and surveys still agree afterwards
    nexus.unbind<Velocity>(e1);
    nexus.flush();

    REQUIRE(collect(nexus.survey<Position, Velocity>()) == Entities{ e3 });
    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e3 });
}

TEST_CASE("Components outside a Bond do not affect its group", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Velocity>(e1, Velocity{ 10 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e1 });

    nexus.bind<Health>(e1, Health{ 5 });
    nexus.bind<Health>(e2, Health{ 6 });
    nexus.flush();
    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e1 });

    nexus.unbind<Health>(e1);
    nexus.flush();
    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e1 });
}

TEST_CASE("Bonds on disjoint component types are independent", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();
    Entity e3 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Velocity>(e1, Velocity{ 10 });
    nexus.bind<Health>(e1, Health{ 100 });
    nexus.bind<Armor>(e1, Armor{ 7 });

    nexus.bind<Position>(e2, Position{ 2 });
    nexus.bind<Velocity>(e2, Velocity{ 20 });

    nexus.bind<Health>(e3, Health{ 300 });
    nexus.bind<Armor>(e3, Armor{ 9 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({ e1, e2 }));
    REQUIRE(collect(nexus.bond<Health, Armor>()) == sorted({ e1, e3 }));

    nexus.unbind<Position>(e1);
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e2 });
    REQUIRE(collect(nexus.bond<Health, Armor>()) == sorted({ e1, e3 }));

    nexus.destroy(e3);
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity>()) == Entities{ e2 });
    REQUIRE(collect(nexus.bond<Health, Armor>()) == Entities{ e1 });
}

TEST_CASE("A Bond of three component types works", "[Bond]")
{
    Nexus nexus;
    Entity e1 = nexus.create();
    Entity e2 = nexus.create();

    nexus.bind<Position>(e1, Position{ 1 });
    nexus.bind<Velocity>(e1, Velocity{ 10 });
    nexus.bind<Position>(e2, Position{ 2 });
    nexus.bind<Velocity>(e2, Velocity{ 20 });
    nexus.flush();

    REQUIRE(collect(nexus.bond<Position, Velocity, Health>()).empty());

    nexus.bind<Health>(e2, Health{ 5 });
    nexus.flush();
    REQUIRE(collect(nexus.bond<Position, Velocity, Health>()) == Entities{ e2 });

    nexus.bind<Health>(e1, Health{ 6 });
    nexus.flush();
    REQUIRE(collect(nexus.bond<Position, Velocity, Health>()) == sorted({ e1, e2 }));
}

TEST_CASE("Randomized binds, unbinds and destroys keep the group correct", "[Bond][stress]")
{
    Nexus nexus;
    std::mt19937 rng(12345);

    struct State { bool position = false; bool velocity = false; bool health = false; bool armor = false; };
    std::map<Entity, State> model; // what each live Entity should currently have (Armor is outside the Bond)

    // create the Bond up front, like a real scene would
    nexus.bond<Position, Velocity, Health>();

    auto expected_group = [&model]() {
        Entities result;
        for (const auto& [e, s] : model) {
            if (s.position && s.velocity && s.health) {
                result.push_back(e);
            }
        }
        return result; // std::map iterates in ascending order, so this is already sorted
    };

    auto check_everything = [&]() {
        Entities group = collect(nexus.bond<Position, Velocity, Health>());
        REQUIRE(group == expected_group());

        // component data must still belong to the right Entity, wherever it moved to
        for (const auto& [e, s] : model) {
            REQUIRE(nexus.has<Position>(e) == s.position);
            REQUIRE(nexus.has<Velocity>(e) == s.velocity);
            REQUIRE(nexus.has<Health>(e) == s.health);
            REQUIRE(nexus.has<Armor>(e) == s.armor);

            if (s.position) REQUIRE(nexus.get<Position>(e).x == static_cast<int>(e));
            if (s.velocity) REQUIRE(nexus.get<Velocity>(e).dx == static_cast<int>(e) * 2);
            if (s.health)   REQUIRE(nexus.get<Health>(e).hp == static_cast<int>(e) * 3);
            if (s.armor)    REQUIRE(nexus.get<Armor>(e).value == static_cast<int>(e) * 4);
        }
    };

    for (int step = 0; step < 3000; ++step) {

        // keep a pool of Entities to work with
        if (model.size() < 4) {
            Entity created = nexus.create();
            model[created] = State{};
            continue;
        }

        auto it = model.begin();
        std::advance(it, rng() % model.size());
        Entity e = it->first;
        int value = static_cast<int>(e);

        switch (rng() % 10) {
        case 0:
            if (model.size() < 60) {
                Entity created = nexus.create();
                model[created] = State{};
            }
            break;

        case 1:
            nexus.destroy(e);
            model.erase(it);
            break;

        default: {
            State& s = it->second;
            switch (rng() % 4) {
            case 0:
                if (s.position) nexus.unbind<Position>(e); else nexus.bind<Position>(e, Position{ value });
                s.position = !s.position;
                break;
            case 1:
                if (s.velocity) nexus.unbind<Velocity>(e); else nexus.bind<Velocity>(e, Velocity{ value * 2 });
                s.velocity = !s.velocity;
                break;
            case 2:
                if (s.health) nexus.unbind<Health>(e); else nexus.bind<Health>(e, Health{ value * 3 });
                s.health = !s.health;
                break;
            default:
                if (s.armor) nexus.unbind<Armor>(e); else nexus.bind<Armor>(e, Armor{ value * 4 });
                s.armor = !s.armor;
                break;
            }
            break;
        }
        }

        nexus.flush();

        REQUIRE(collect(nexus.bond<Position, Velocity, Health>()) == expected_group());

        if (step % 100 == 0) {
            check_everything();
        }
    }

    check_everything();
}