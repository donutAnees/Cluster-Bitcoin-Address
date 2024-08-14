#ifndef HEURISTICS_H
#define HEURISTICS_H

#include <unordered_map>
#include "api.h"
#include "entity.h"

class Heuristics{
    public:
    void runHeuristics(std::unordered_map<uint64_t,Entity>& entities, std::vector<getrawtransaction_t>& blockTransactions, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency);
    private:
    void commonInputOwnershipHeuritics(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity);
    void changeAddressHeuristics(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency);
    void scriptChainMergeHeuristics(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency);
    void coinbaseOutput(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity);
};

#endif