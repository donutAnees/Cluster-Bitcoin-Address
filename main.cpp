#include <unordered_map>
#include <chrono>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/basic/document.hpp>

#include "api.h"
#include "entity.h"
#include "heuristics.h"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;


uint64_t lastEntityID = 0;


void iterate_documents(mongocxx::collection& addressCollection, mongocxx::collection& reuseCollection, std::unordered_map<std::string,uint64_t>& walletToEntity, std::unordered_map<uint64_t,Entity>& entities, std::unordered_map<std::string,int>& reuseFrequency) {
    // Execute a query with an empty filter to get all documents.
    mongocxx::cursor addresscursor = addressCollection.find({});
    mongocxx::cursor reusecursor = reuseCollection.find({});

    //The ID to Entity Map must be constructed from the previous data in the blockchain, we define a bool map which stores data on whether an entity is created not, if not then we create the entity and push the wallets corresponding to it, else we directly push to the entity in the entities structure
    std::unordered_map<uint64_t,bool> map;

    for (const bsoncxx::document::view& doc : addresscursor) {
        std::string wallet = doc["wallet"].get_string().value.to_string();
        uint64_t entityID = std::stoull(doc["entityID"].get_string().value.to_string());
        walletToEntity[wallet] = entityID;
        if(map[entityID]){
            entities[entityID].addWallet(wallet);
        }
        else{
            Entity entity(entityID);
            entity.addWallet(wallet);
            entities[entityID] = entity;
            map[entityID]=true; // adding the ID to the map
        }
        lastEntityID = std::max(entityID,lastEntityID);
    }
    // Setting the number of entities encountered thus far
    if(lastEntityID != 0)Entity::setEntitiesCount(lastEntityID);

    for (const bsoncxx::document::view& doc : reusecursor){
        std::string wallet = doc["wallet"].get_string().value.to_string();
        int frequency = doc["frequency"].get_int32().value;
        reuseFrequency[wallet] = frequency;
    }
}

int main()
{
    std::string username = "bitcoin";
    std::string password = "password";
    std::string address = "127.0.0.1";
    int port = 8332;

    
    /* Contains the wallet to Entity ID mapping used for adding related wallets to the same Entity */
    std::unordered_map<std::string,uint64_t> walletToEntity;
    /* Used for keeping track of the various entities*/
    std::unordered_map<uint64_t,Entity> entities;
    /* From which block to which block the heuristics must be run */
    uint32_t startBlockNumber, endBlockNumber;
    /* Stores the hashes of the block from the start block till the end block, which are later used to get the actual block data*/
    std::vector<std::string> blockhashes;
    /* Contains the transactions of each block*/
    std::vector<getrawtransaction_t> blockTransactions;
    /* Contains the number of times the wallet is reused for receiving*/
    std::unordered_map<std::string, int> reuseFrequency;

    std::chrono::_V2::system_clock::time_point start;

    try{
        // Create an instance.
        mongocxx::instance inst{};
        const auto uri = mongocxx::uri{"mongodb://localhost:27017"};

        // Set the version of the Stable API on the client.
        mongocxx::options::client client_options;
        const auto api = mongocxx::options::server_api{ mongocxx::options::server_api::version::k_version_1 };
        client_options.server_api_opts(api);

        // Setup the connection and get a handle on the "clustredaddresses" database.
        mongocxx::client conn{ uri, client_options };
        mongocxx::database db = conn["ClusteredAddresses"];
        mongocxx::collection collection = db["WalletToEntity"];
        mongocxx::collection reuseCollection = db["reuseFrequency"];

        /* This function gets the previous walletToEntity and reuseFrequency Stored in database*/
        iterate_documents(collection,reuseCollection,walletToEntity,entities,reuseFrequency);

        try
        {
            /* Constructor to connect to the bitcoin daemon */
            API btc(username, password, address, port,10000000);

            /* Getting start and end blocks index and retrieving them to store*/
            std::cout << "Enter start and End Block Index" << std::endl;
            std::cin >> startBlockNumber >> endBlockNumber;

            start = std::chrono::system_clock::now();
            
            Heuristics heuristic;
            int count = 0;

            for(int i = startBlockNumber; i <= endBlockNumber; i++){
                std::string blockhash = btc.getblockhash(i);
                blockinfo_t currentBlock = btc.getblock(blockhash);
                for(std::string& tx : currentBlock.tx){
                    getrawtransaction_t transaction = btc.getrawtransaction(tx);
                    blockTransactions.emplace_back(transaction);
                }
                heuristic.runHeuristics(entities,blockTransactions,walletToEntity,reuseFrequency);
                blockTransactions.clear();

                std::cout << "Done " << i << std::endl;

                /* Only for testing remove later */
                count++;
                if((count) % 50 == 0){
                    collection.drop();
                    reuseCollection.drop();
                    std::vector<bsoncxx::document::value> documents;
                    for(const auto& a : walletToEntity){
                        std::string entityID = std::to_string(a.second);
                        documents.emplace_back(make_document(kvp("wallet",a.first),kvp("entityID",entityID)));
                    }

                    collection.insert_many(documents);
                    documents.clear();

                    for(const auto& a : reuseFrequency){
                        documents.emplace_back(make_document(kvp("wallet",a.first),kvp("frequency",a.second)));
                    }
                    reuseCollection.insert_many(documents);
                }

            }

            /*Delete the previous entries in the collection and update with new mappings*/
            collection.drop();
            reuseCollection.drop();
            std::vector<bsoncxx::document::value> documents;
            for(const auto& a : walletToEntity){
                std::string entityID = std::to_string(a.second);
                documents.emplace_back(make_document(kvp("wallet",a.first),kvp("entityID",entityID)));
            }

            collection.insert_many(documents);
            documents.clear();

            for(const auto& a : reuseFrequency){
                documents.emplace_back(make_document(kvp("wallet",a.first),kvp("frequency",a.second)));
            }
            reuseCollection.insert_many(documents);

            /* To print the entities along with the wallets*/
            // for(auto &a : entities){
            //     a.second.listWallets();
            // }

            
            auto end = std::chrono::system_clock::now();

            const std::chrono::duration<double> time = end - start;

            std::cout << "Elapsed Time: " << time.count() << std::endl;

        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }

    }

    catch (const std::exception& e) 
    {
        std::cout<< "Exception: " << e.what() << std::endl;
    }


}