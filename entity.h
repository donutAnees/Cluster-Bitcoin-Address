#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <unordered_set>
#include <iostream>
#include <queue>
 
class Entity{
    private:
    static uint64_t entitiescount;
    static std::queue<uint64_t> freeID;
    uint64_t id;
    public:
    std::unordered_set<std::string>wallets;
    Entity();
    Entity(uint64_t id);
    void addWallet(std::string wallet);
    void listWallets();
    uint64_t getId();
    static void setEntitiesCount(uint64_t);
    static void pushToFreeID(uint64_t id);
};

#endif