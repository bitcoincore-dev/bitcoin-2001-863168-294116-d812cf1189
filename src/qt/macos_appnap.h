// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MACOS_APPNAP_H
#define BITCOIN_QT_MACOS_APPNAP_H

class CAppNapInhibitor
{
public:
    CAppNapInhibitor(const char* strReason);
    ~CAppNapInhibitor();
private:
    class Private;
    Private *d;
};

#endif // BITCOIN_QT_MACOS_APPNAP_H
