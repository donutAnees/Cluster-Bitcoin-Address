#ifndef DEFINITION_H
#define DEFINITION_H

#include <vector>
#include <string>

    struct blockinfo_t{
		std::string hash;
		std::vector<std::string> tx;
	};

	struct scriptSig_t{
		std::string assembly;
		std::string hex;
		std::string address;
	};

	struct scriptPubKey_t{
		std::string assembly;
		std::string hex;
		std::string type;
		std::vector<std::string> addresses;
	};

	struct vin_t{
        std::string txid;
		bool isCoinbase;
		double value;
		scriptSig_t scriptSig;
	};

	struct vout_t{
		double value;
		scriptPubKey_t scriptPubKey;
	};

	struct getrawtransaction_t{
        std::string txid;
		std::vector<vin_t> vin;
		std::vector<vout_t> vout;
		std::string hex;
		std::string blockhash;
	};

#endif