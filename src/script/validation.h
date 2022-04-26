// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_VALIDATION_H
#define BITCOIN_SCRIPT_VALIDATION_H

#include <script/bitcoinconsensus.h>

int bitcoinconsensus_verify_script_impl(const unsigned char* scriptPubKey, unsigned int scriptPubKeyLen,
                                        const unsigned char* txTo, unsigned int txToLen,
                                        unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err);

int bitcoinconsensus_verify_script_with_amount_impl(const unsigned char* scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
                                                    const unsigned char* txTo, unsigned int txToLen,
                                                    unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err);

unsigned int bitcoinconsensus_version_impl();

#endif // BITCOIN_SCRIPT_VALIDATION_H
