// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinamountlabel.h>

#include <qt/bitcoinunits.h>

#include <QChar>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cassert>

namespace {
const QChar space{' '};
const QChar point{'.'};
const QChar mask{'#'};

QString cloak(QString value_and_unit)
{
    const QString unit = space + value_and_unit.split(space).last();
    const QStringList value_parts = value_and_unit.remove(unit).split(point);
    assert(value_parts.size() <= 2);
    QString cloaked = QString(value_parts.first().size(), mask);
    if (value_parts.size() == 2) {
        cloaked += point + QString(value_parts.last().size(), mask);
    }
    return cloaked + unit;
}
} // namespace

BitcoinAmountLabel::BitcoinAmountLabel(QWidget* parent)
    : QLabel(parent)
{
}

void BitcoinAmountLabel::setValue(int unit, const CAmount& amount)
{
    QString value_and_unit = BitcoinUnits::format(unit, amount, false, BitcoinUnits::separatorAlways, true) + space + BitcoinUnits::shortName(unit);
    if (m_hidden_value.isEmpty()) {
        QLabel::setText(value_and_unit);
    } else {
        m_hidden_value = value_and_unit;
        QLabel::setText(cloak(m_hidden_value));
    }
}

void BitcoinAmountLabel::setPrivacyMode(bool privacy)
{
    if (privacy && m_hidden_value.isEmpty()) {
        m_hidden_value = text();
        QLabel::setText(cloak(m_hidden_value));
    } else if (!privacy && !m_hidden_value.isEmpty()) {
        QLabel::setText(m_hidden_value);
        m_hidden_value.clear();
    }
}
