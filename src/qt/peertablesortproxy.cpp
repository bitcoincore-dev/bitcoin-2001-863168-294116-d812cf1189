// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/peertablesortproxy.h>

#include <qt/peertablemodel.h>

#include <QModelIndex>
#include <QString>
#include <QVariant>

PeerTableSortProxy::PeerTableSortProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

bool PeerTableSortProxy::lessThan(const QModelIndex& left_index, const QModelIndex& right_index) const
{
    QVariant left = sourceModel()->data(left_index, Qt::UserRole);
    QVariant right = sourceModel()->data(right_index, Qt::UserRole);

    switch (left_index.column()) {
    case PeerTableModel::NetNodeId:
    case PeerTableModel::Ping:
        return left.toLongLong() < right.toLongLong();
    case PeerTableModel::Sent:
    case PeerTableModel::Received:
        return left.toULongLong() < right.toULongLong();
    }

    return left.toString() < right.toString();
}