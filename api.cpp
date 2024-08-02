#include "api.h"
#include <string>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>

/* THe following functions are for converting the public Key into the BTC address format*/

const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string encodeBase58(const unsigned char *pbegin, const unsigned char *pend)
{
    int zeroes = 0;
    int length = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    int size = (pend - pbegin) * 138 / 100 + 1; 
    std::vector<unsigned char> b58(size);
    while (pbegin != pend) {
        int carry = *pbegin;
        int i = 0;
        for (auto it = b58.rbegin(); (carry != 0 || i < length) && (it != b58.rend()); it++, i++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        length = i;
        pbegin++;
    }
    auto it = b58.begin() + (size - length);
    while (it != b58.end() && *it == 0)
        it++;
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}

std::string encodeBase58(const std::vector<unsigned char> &vch)
{
    return encodeBase58(&vch[0], &vch[0] + vch.size());
}

std::string getsha256(std::string& address){
    long len = 0;
    unsigned char *bin = OPENSSL_hexstr2buf(address.c_str(), &len);
    const EVP_MD *md_algo = EVP_sha256();
    unsigned int md_len = EVP_MD_size(md_algo);
    std::vector<unsigned char> md( md_len );
    EVP_Digest(bin, len, md.data(), &md_len, md_algo, nullptr);
    OPENSSL_free(bin);
    std::string sha256;
    static const char hex[] = "0123456789abcdef";
    for(char c : md){
        sha256 += hex[(c >> 4) & 0xF];
        sha256 += hex[c & 0xF];
    }
    return sha256;
}

std::string getripemd(std::string& address){
    long len = 0;
    unsigned char *bin = OPENSSL_hexstr2buf(address.c_str(), &len);
    const EVP_MD *md_algo = EVP_ripemd160();
    unsigned int md_len = EVP_MD_size(md_algo);
    std::vector<unsigned char> md( md_len );
    EVP_Digest(bin, len, md.data(), &md_len, md_algo, nullptr);
    OPENSSL_free(bin);
    std::string ripemd;
    static const char hex[] = "0123456789abcdef";
    for(char c : md){
        ripemd += hex[(c >> 4) & 0xF];
        ripemd += hex[c & 0xF];
    }
    return ripemd; 
}

std::string decodeaddress(std::string& address){
    std::string sha = getsha256(address);
    std::string r = getripemd(sha);
    std::string extendedPublicKey = "00" + r;
    std::string sha1 = getsha256(extendedPublicKey);
    std::string sha2 = getsha256(sha1);
    std::string checksum = sha2.substr(0,8);
    std::string address_in_hex = extendedPublicKey + checksum;
    std::vector<unsigned char> address_in_hex_v;
    for (size_t i = 0; i < address_in_hex.length(); i += 2) {
        std::string byteString = address_in_hex.substr(i, 2);
        unsigned char byte = (unsigned char) strtol(byteString.c_str(), nullptr, 16);
        address_in_hex_v.push_back(byte);
    }
    std::string wallet = encodeBase58(address_in_hex_v);
    return wallet;
}

API::API(std::string& user, std::string& password, std::string& host, int port, int timeout)
: httpClient(new HttpClient("http://" + user + ":" + password + "@" + host + ":" + std::to_string(port))),
  client(new Client(*httpClient, JSONRPC_CLIENT_V1))
{
    httpClient->SetTimeout(timeout);
}

API::~API()
{
    delete client;
    delete httpClient;
}

Json::Value API::request(std::string &command, Json::Value &params)
{
    Value result;
	result = client->CallMethod(command, params);
	return result;
}

/* This gets the transaction in raw hex format and decodes it*/

getrawtransaction_t API::getrawtransaction(std::string& txid) {
	std::string command = "getrawtransaction";
	Value params, result;
	getrawtransaction_t res;
	params.append(txid);
	params.append(2);
	result = request(command, params);
	res.txid = result["txid"].asString();
    for (ValueIterator it = result["vin"].begin(); it != result["vin"].end();
            it++) {
        Value val = (*it);
        vin_t input;
        if(val["coinbase"]) input.isCoinbase = true;
        else input.isCoinbase = false;
        input.txid = val["txid"].asString();
        input.scriptSig.assembly = val["scriptSig"]["asm"].asString();
        input.scriptSig.hex = val["scriptSig"]["hex"].asString();
        input.value = val["prevout"]["value"].asDouble();
        std::string address = val["prevout"]["scriptPubKey"]["type"].asString();
        /* Sometimes the transaction may not have the standard BTC Address but rather the public key, this has to be decoded*/
        if(address == "pubkey"){
                std::string pubkey;
                for(char c :address) pubkey += c;
                input.scriptSig.address = decodeaddress(pubkey);
        }
        else input.scriptSig.address = val["prevout"]["scriptPubKey"]["address"].asString();
        res.vin.push_back(input);
    }

    for (ValueIterator it = result["vout"].begin(); it != result["vout"].end();
            it++) {
        Value val = (*it);
        vout_t output;

        output.value = val["value"].asDouble();
        output.scriptPubKey.assembly = val["scriptPubKey"]["asm"].asString();
        output.scriptPubKey.hex = val["scriptPubKey"]["hex"].asString();
        output.scriptPubKey.type = val["scriptPubKey"]["type"].asString();
        if(output.scriptPubKey.type == "pubkey"){
            std::string pubkey;
            for(char c : val["scriptPubKey"]["address"].asString()) pubkey += c;
            output.scriptPubKey.addresses.push_back(decodeaddress(pubkey));
        }
        else output.scriptPubKey.addresses.push_back(val["scriptPubKey"]["address"].asString());

        res.vout.push_back(output);
    }

	return res;
}

/* Get the block data and decodes it */

blockinfo_t API::getblock(std::string& blockhash) {
	std::string command = "getblock";
	Value params, result;
	blockinfo_t ret;
	params.append(blockhash);
	result = request(command, params);
	ret.hash = result["hash"].asString();
	for(ValueIterator it = result["tx"].begin(); it != result["tx"].end(); it++){
		ret.tx.push_back((*it).asString());
	}
	return ret;
}


std::string API::getblockhash(int blocknumber) {
	std::string command = "getblockhash";
	Value params, result;
	params.append(blocknumber);
	result = request(command, params);
	return result.asString();
}