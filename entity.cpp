#include "entity.h"

Entity::Entity(){
    /* IF there are no deleted ID's ready to be reused then we increment the entitiescount and assign it to the newly created entity*/
    if(freeID.size() == 0) this->id = entitiescount++;
    /* Else one of the free ID is assigned*/
    else{
        this->id = freeID.front();
        freeID.pop();
    }
}

Entity::Entity(uint64_t id){
    this->id = id;
}

void Entity::addWallet(std::string wallet){
    this->wallets.insert(wallet);
}

void Entity::listWallets(){
    std::cout << "Entity: " << this->id << std::endl;
    for(auto wallet : this->wallets){
        std::cout << wallet << std::endl;
    }
}

uint64_t Entity::getId(){
    return id;

}

/* This is used to init during program startup, if the database had the last entity ID has 10, then the restarted program should begin with 11, this is what this function is does and is called in the main program*/
void Entity::setEntitiesCount(uint64_t id)
{
    entitiescount = id + 1;
}

void Entity::pushToFreeID(uint64_t id){
    freeID.push(id);
}

uint64_t Entity::entitiescount = 1;
std::queue<uint64_t> Entity::freeID;