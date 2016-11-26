// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MEMPOOLSTATS_H
#define BITCOIN_QT_MEMPOOLSTATS_H

#include <QWidget>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>

#include <QCheckBox>
#include <QDialog>
#include <QGraphicsProxyWidget>
#include <QGraphicsView>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

#include <QEvent>

class ClientModel;


class MempoolStats : public QDialog
{
    Q_OBJECT

public:
    MempoolStats(QWidget *parent = 0);
    ~MempoolStats();

    void setClientModel(ClientModel *model);

    virtual QSize sizeHint() const;

public Q_SLOTS:
    void drawChart();

private:
    ClientModel *clientModel;

    void setupValueUI(QWidget *checkbox, QWidget *label, const QString&);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void showEvent(QShowEvent *event);

    QGridLayout *opts_grid;

    QCheckBox *cbShowMemUsage;
    QLabel *lblMemUsage;

    QCheckBox *cbShowNumTxns;
    QLabel *lblNumTxns;

    QCheckBox *cbShowMinFeerate;
    QLabel *lblMinFeerate;

    QGraphicsView *gvChart;

    QGraphicsTextItem *noDataItem;

    QSlider *sliderScale;
    QLabel *lblGraphRange;

    QGraphicsScene *scene;
    QVector<QGraphicsItem*> redrawItems;
};

#endif // BITCOIN_QT_MEMPOOLSTATS_H
