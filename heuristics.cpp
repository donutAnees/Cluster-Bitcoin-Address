#include <algorithm>
#include <thread>
#include <mutex>

#include "heuristics.h"


std::mutex key;


void Heuristics::runHeuristics(std::unordered_map<uint64_t,Entity>& entities, std::vector<getrawtransaction_t>& blockTransactions, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency){
    for(getrawtransaction_t& transaction : blockTransactions){
        /* Iterates through each transaction output and stores the output reused frequency, once this is computed it is used for the heuristic */
        for(vout_t& out : transaction.vout){
            /* Even though this is a vector it contains only one address, used vector for convention, therefore accessing just addresses[0] */
            reuseFrequency[out.scriptPubKey.addresses[0]]++;
        }
        /*HEURISTICS 5*/
        /* If the transaction is a coinbase transaction we merge the outputs, since the output is managed by a single miner */
        if(transaction.vin[0].isCoinbase == true){
            coinbaseOutput(transaction, entities, walletToEntity);
            continue;
        }
        /*HEURISTICS 1*/
        commonInputOwnershipHeuritics(transaction, entities, walletToEntity);
        /* Using threads for the following two functions*/
        /*HEURISTICS 2 and 4*/
        std::thread changeAddressThread(&Heuristics::changeAddressHeuristics, this, std::ref(transaction), std::ref(entities), std::ref(walletToEntity), std::ref(reuseFrequency));
        /*HEURISTICS 3*/
        std::thread scriptChainThread(&Heuristics::scriptChainMergeHeuristics, this, std::ref(transaction), std::ref(entities), std::ref(walletToEntity), std::ref(reuseFrequency));

        changeAddressThread.join();
        scriptChainThread.join();
}
}

void Heuristics::commonInputOwnershipHeuritics(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity){
    std::vector<uint64_t> entitiesToMerge;
    std::vector<std::string> walletaddresses;
    /* Get all the input addresses and check if any of them is part of any other entity, if so then we should merge the current addresses with the already existing entity where it belongs, else we can create a new entity for the inputs*/
    for(vin_t& in : transaction.vin){
        std::string walletaddress = in.scriptSig.address;
        auto it = walletToEntity.find(walletaddress);
        if (it != walletToEntity.end()) {
            entitiesToMerge.push_back(it->second);
        }
        walletaddresses.emplace_back(walletaddress); 
    }
    uint64_t entityIndex; // the entity index, walletToEntity mapping are assigned for the current input wallets
    /* Updating the mapping in walletToEntity and changing the id in entity, if there are entities to merge */
    if(entitiesToMerge.size() != 0){
        /* Getting the first minimum entity id and merging all the addresses into it, the reason we do this is because, the current input addresses can match with multiple various entities and all of these entities must be made into one, we choose the entity with the minimum id as the candidate */
        std::sort(entitiesToMerge.begin(),entitiesToMerge.end());
        entityIndex = entitiesToMerge[0];
        uint64_t entitiesToMergeSize = entitiesToMerge.size();
        /* add the wallets from the other entities to which the current inputs match with to the minimum entity id and delete the entity */
        for(uint64_t i = 1; i < entitiesToMergeSize; i++){
            for(auto wallet: entities[entitiesToMerge[i]].wallets){
                walletaddresses.emplace_back(wallet);
            }
            /* Push the deleted id's to the freeID vector which is used during creation of newer entity*/
            Entity::pushToFreeID(entitiesToMerge[i]);
            entities.erase(entitiesToMerge[i]);
        }
        /* No new entity is created, added to the existing entity*/
        for(std::string& s : walletaddresses) {
            entities[entityIndex].addWallet(s);
            walletToEntity[s] = entityIndex;
        }
    }
    else{/* If the input wallets do not belong to any other entity then we create a new entity */
        Entity entity;
        entityIndex = entity.getId();
        for(std::string& s : walletaddresses) {
            entity.addWallet(s);
            walletToEntity[s] = entityIndex;
        }
        entities.insert({entityIndex,entity});
    }
}

void Heuristics::changeAddressHeuristics(getrawtransaction_t& transaction,std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency){
    /*HEURISTICS 4*/
    /* If there is only one output then we can just merge this output with the inputs, since its likely that sender is depositing all the funds from his wallets to a new wallet */
    if(transaction.vout.size() == 1){
        uint64_t entityId = walletToEntity[transaction.vin[0].scriptSig.address];
        if(key.try_lock()){
        entities[entityId].addWallet(transaction.vout[0].scriptPubKey.addresses[0]);
        walletToEntity[transaction.vout[0].scriptPubKey.addresses[0]] = entityId;
        key.unlock();
        }
        return;
    }
    if(transaction.vout.size() != 2) return;
    /*HEURISTICS 2*/
    std::string address1 = transaction.vout[0].scriptPubKey.addresses[0];
    std::string address2 = transaction.vout[1].scriptPubKey.addresses[0];
    /* Check if one of the two wallets is not reused and the other is reused, if reuseFrequency is 1 then it means this is the only transaction where the wallet is used and therefore not used*/
    if((reuseFrequency[address1] != 1 && reuseFrequency[address2] < 1) || (reuseFrequency[address1] < 1 && reuseFrequency[address2] != 1)) return;
    /* If this is the case then merge the input and the change wallet*/
    std::string inputaddress;
    inputaddress = transaction.vin[0].scriptSig.address;
    /* Check if input address is in any of the output addressses, if that is the case then break out since the input address is the change address and our heuristics will likely consider the payment address, if its not reused, as change address leading to false positives*/
    if(inputaddress == address1 || inputaddress == address2) return;
    std::string address = reuseFrequency[address1] == 1 ? address1 : address2;
    uint64_t entityId = walletToEntity[inputaddress];
    if(key.try_lock()){
    entities[entityId].addWallet(address);
    walletToEntity[address] = entityId;
    key.unlock();
    }
}

void Heuristics::scriptChainMergeHeuristics(getrawtransaction_t& transaction, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<std::string, int> &reuseFrequency){
    if(transaction.vin.size() < 2 || transaction.vout.size() != 2) return;
    /* Process the inputs, that is get the inputs */
    std::vector<std::string> inputaddresses;
    for(vin_t& in : transaction.vin){
        inputaddresses.emplace_back(in.scriptSig.address);
    }
    /* Process the output, that is find which one is the change and which one is the payment, minimum is the change */
    std::pair<std::string,std::string> paymentWalletChangeWalletPair;
    if (transaction.vout[0].value > transaction.vout[1].value) {
        paymentWalletChangeWalletPair = {transaction.vout[0].scriptPubKey.addresses[0], transaction.vout[1].scriptPubKey.addresses[0]};
    } 
    else {
        paymentWalletChangeWalletPair = {transaction.vout[1].scriptPubKey.addresses[0], transaction.vout[0].scriptPubKey.addresses[0]};
    }
    /* Now we check if ΣVin- min(Vin) < Vmax, this condition suggests that the user tried to minimize the total value, that is used his combined funds from change wallets to make the transaction*/
    uint64_t inMin = transaction.vin[0].value , outMax = transaction.vout[0].value;
    uint64_t inSize = transaction.vin.size(), outSize = transaction.vout.size();
    long long sum = transaction.vin[0].value;
    for(uint32_t i = 1; i < inSize; i++)  if(transaction.vin[i].value < inMin) inMin = transaction.vin[i].value, sum += transaction.vin[i].value;
    for(uint32_t i = 1; i < outSize; i++) if(transaction.vout[i].value > outMax) outMax = transaction.vout[i].value;
    if(sum - inMin > outMax) return;
    /* Get the id of any of the input wallets, we consider any because by common input heuristics all the inputs belong to one user */
    int entityId = walletToEntity[inputaddresses[0]];
    if(key.try_lock()){
    entities[entityId].addWallet(paymentWalletChangeWalletPair.second);
    walletToEntity[paymentWalletChangeWalletPair.second] = entityId;
    key.unlock();
    }
}

void Heuristics::coinbaseOutput(getrawtransaction_t &transaction, std::unordered_map<uint64_t, Entity> &entities, std::unordered_map<std::string, uint64_t> &walletToEntity)
{
    /* Merge the outputs and create one entity*/
    Entity entity;
    uint64_t entityIndex = entity.getId();
    for(auto& output : transaction.vout){
        entity.addWallet(output.scriptPubKey.addresses[0]);
        walletToEntity[output.scriptPubKey.addresses[0]] = entityIndex;
    }
    entities.insert({entityIndex,entity});
}
