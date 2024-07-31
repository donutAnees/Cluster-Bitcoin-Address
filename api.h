#ifndef API_H
#define API_H

#include <jsoncpp/json/json.h>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

#include "definition.h"

using jsonrpc::Client;
using jsonrpc::JSONRPC_CLIENT_V1;
using jsonrpc::HttpClient;
using jsonrpc::JsonRpcException;
using Json::Value;
using Json::ValueIterator;

class API
{

private:
    jsonrpc::HttpClient * httpClient;
    jsonrpc::Client * client;

public:
    API(std::string& user, std::string& password, std::string& host, int port, int timeout);
    ~API();
    Json::Value request(std::string& command, Json::Value& params);
    getrawtransaction_t getrawtransaction(std::string& txid);
    std::string getblockhash(int blocknumber);
    blockinfo_t getblock(std::string& blockhash);
};

#endif