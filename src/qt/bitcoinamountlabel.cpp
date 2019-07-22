// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinamountlabel.h>

#include <qt/bitcoinunits.h>

#include <QLabel>
#include <QString>
#include <QWidget>

BitcoinAmountLabel::BitcoinAmountLabel(QWidget* parent)
    : QLabel(parent)
{
}

void BitcoinAmountLabel::setValue(const CAmount& amount, int unit, bool privacy)
{
    QString value = BitcoinUnits::format(unit, privacy ? 0 : amount, false, BitcoinUnits::separatorAlways, true);
    if (privacy) {
        value.replace('0', '#');
    }
    QLabel::setText(value + QString(" ") + BitcoinUnits::shortName(unit));
}
