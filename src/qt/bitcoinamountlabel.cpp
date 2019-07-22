// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinamountlabel.h>

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <cassert>

static const QString mask{'#'};

static QString cloak(QString& bitcoin_value_string)
{
    QStringList parts = bitcoin_value_string.split('.');
    assert(parts.size() <= 2);
    QString integer_part = parts.first();
    integer_part = QString(integer_part.size() - 1, ' ') + mask;
    if (parts.size() == 1) {
        // Unit is Satoshi (sat).
        return integer_part;
    }
    QString fractional_part = parts.last();
    fractional_part = fractional_part.replace(QRegularExpression("\\d"), mask);
    return integer_part + '.' + fractional_part;
}

BitcoinAmountLabel::BitcoinAmountLabel(QWidget* parent)
    : QLabel(parent)
{
}

void BitcoinAmountLabel::setText(const QString& newText)
{
    if (cache.isEmpty()) {
        QLabel::setText(newText);
    } else {
        cache = newText;
        QLabel::setText(cloak(cache));
    }
}

void BitcoinAmountLabel::changePrivacyMode(bool privacy)
{
    if (privacy && cache.isEmpty()) {
        cache = text();
        QLabel::setText(cloak(cache));
    } else if (!privacy && !cache.isEmpty()) {
        QLabel::setText(cache);
        cache.clear();
    }
}

void BitcoinAmountLabel::mousePressEvent(QMouseEvent* ev)
{
    Q_EMIT aboutToTogglePrivacy();
    QLabel::mousePressEvent(ev);
}
