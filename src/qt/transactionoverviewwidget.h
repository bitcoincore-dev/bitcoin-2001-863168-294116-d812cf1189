// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONOVERVIEWWIDGET_H
#define BITCOIN_QT_TRANSACTIONOVERVIEWWIDGET_H

#include <qt/transactiontablemodel.h>

#include <QListView>
#include <QWidget>

class TransactionOverviewWidget : public QListView
{
    Q_OBJECT

public:
    explicit TransactionOverviewWidget(QWidget* parent = nullptr) : QListView(parent) {}

    QSize sizeHint() const override
    {
        return {sizeHintForColumn(TransactionTableModel::ToAddress), QListView::sizeHint().height()};
    }
};

#endif // BITCOIN_QT_TRANSACTIONOVERVIEWWIDGET_H
