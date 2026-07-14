// test file for learning and experimenting with nlohmann/json
// this includes serializing and deserializing a C++ struct to and from json
// and using the to_json and from_json to define the serilization behavior for a struct with
// several field types, and error handling for missing fields

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// alias, this allows us to use just json instead of nlohmann::json
using json = nlohmann::json;

// example struct for serilization
// to simulate actually coding in the engine the struct represents EntityData
struct EntityData {
  int                      id;
  std::string              tag;
  float                    posx, posy;
  bool                     visible;
  std::string              texturePath;
  std::vector<std::string> componentList;
};

// to_json and from_json mthods, for now these are just in the global namespace
// but for engine ready development they will juat have to exists in the same namespace
// as whatever struct they are defined for, they define seriliaztion and deserializtion
// behavior for a struct when using nlohmann/json

void to_json(json& j, const EntityData& e) {
    j = json{
        {"id", e.id},
        {"tag", e.tag},
        {"X Position", e.posx},
        {"Y Position", e.posy},
        {"isVisible", e.visible},
        {"Texture Path", e.texturePath},
        {"Component List", e.componentList} // std::vector seriliazes automatically 
    }; 
}

void from_json(const json& j, EntityData& e) {
    j.at("id").get_to(e.id); 
    j.at("tag").get_to(e.tag);
    j.at("X Position").get_to(e.posx);
    j.at("Y Position").get_to(e.posy);
    j.at("Texture Path").get_to(e.texturePath);
    j.at("Component List").get_to(e.componentList);

    // optional field gets a default value
    e.visible = j.value("isVisible", true); 
}

int main() {

    //--------------
    //Serialization
    //--------------

    // create an instance of the EntityData struct
    EntityData entity;
    entity.id = 1;
    entity.tag = "e";
    entity.posx = 1.4;
    entity.posy = 2.5; 
    entity.visible = true; 
    entity.texturePath = "test.png";
    entity.componentList.push_back("transform"); 
    entity.componentList.push_back("hurtbox"); 

    // serialize the struct to a JSON file, this calls to_json by default 
    json j = entity; 

    // wrap it in a level file so the file looks like a real level
    json levelFile; 
    levelFile["levelName"] = "TestLevel"; 
    levelFile["version"] = 1.0; 
    levelFile["entity"] = j; 

    // write to disk
    std::ofstream outFile("test_level.json");
    outFile << levelFile.dump(4); // 4 = pretty print with 4 space indent
    outFile.close(); 

    // print conformation to console
    std::cout << "LOG: Serialized to test_level.json" << std::endl;
    std::cout << "LOG:\n" << levelFile.dump(4) << std::endl; 

    //----------------
    //Deserialization
    //----------------

    // open the file 
    std::ifstream inFile("test_level.json"); 

    // pare it with json 
    json loaded = json::parse(inFile); 

    // extract the entity node and convert it back to struct (calls from_json)
    EntityData entityAgain = loaded.at("entity").get<EntityData>(); 

    // print the loaded values to confirm that the deserilaiztion workds
    std::cout << std::endl; 
    std::cout << "Deserialized Entity:" << std::endl; 
    std::cout << "id: " << entityAgain.id << std::endl; 
    std::cout << "tag: " << entityAgain.tag << std::endl; 
    std::cout << "Position: (" << entityAgain.posx << ", " << entityAgain.posy << ")" << std::endl; 
    std::cout << "texure: " << entityAgain.texturePath << std::endl; 
    std::cout << "visible: " << entityAgain.visible << std::endl; 
    for (std::string component : entityAgain.componentList) {
        std:: cout << "Component: " << component << std::endl; 
    }

    //----------------------
    //Error Handling Example
    //----------------------

    // parse error
    std::string badJson = R"({ "id": 1, "tag": "Player" )"; 

    try {
        json broken = json::parse(badJson);
    } catch (const json::parse_error& e) {
        std::cerr << "CAUGHT parse_error: " << e.what() << std::endl; 
    }

    return 0; 
}