// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mempoolstats.h"

#include "clientmodel.h"
#include "guiutil.h"
#include "stats/stats.h"

#include <QCheckBox>
#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

static const char *LABEL_FONT = "Arial";
static int LABEL_TITLE_SIZE = 22;

static const int LABEL_LEFT_SIZE = 30;
static const int LABEL_RIGHT_SIZE = 30;
static const int GRAPH_PADDING_LEFT = 30+LABEL_LEFT_SIZE;
static const int GRAPH_PADDING_RIGHT = 30+LABEL_RIGHT_SIZE;
static const int GRAPH_PADDING_TOP = 10;
static const int GRAPH_PADDING_BOTTOM = 50;

namespace {

void setCenterPos(QGraphicsTextItem * const item, const QSizeF &pos) {
    const QRectF boundingRect = item->boundingRect();
    item->setPos(pos.width() - (boundingRect.width() / 2.0), pos.height() - (boundingRect.height() / 2.0));
}

}

MempoolStats::MempoolStats(QWidget *parent) :
QDialog(parent),
clientModel(0)
{
    this->setWindowTitle(tr("Memory pool statistics"));

    QVBoxLayout * const mainlayout = new QVBoxLayout(this);
    this->setLayout(mainlayout);

    QHBoxLayout * const toplayout = new QHBoxLayout(this);
    mainlayout->addLayout(toplayout);

    {
        QLabel * const secondTitle = new QLabel(this->windowTitle(), this);
        int secondTitleFontSize = secondTitle->font().pointSize() * 2;
        secondTitle->setStyleSheet(
            "QLabel {"
                "font-size: " + QString::number(secondTitleFontSize) + "pt;"
            "}"
        );
        toplayout->addWidget(secondTitle, 0, Qt::AlignBottom);
    }

    opts_grid = new QGridLayout(this);
    toplayout->addLayout(opts_grid);

    cbShowMemUsage = new QCheckBox(tr("Dynamic &memory usage"), this);
    cbShowMemUsage->setChecked(true);
    lblMemUsage = new QLabel(this);
    setupValueUI(cbShowMemUsage, lblMemUsage, "blue");

    cbShowNumTxns = new QCheckBox(tr("Number of &transactions"), this);
    cbShowNumTxns->setChecked(true);
    lblNumTxns = new QLabel(this);
    setupValueUI(cbShowNumTxns, lblNumTxns, "red");

    cbShowMinFeerate = new QCheckBox(tr("Minimum &fee-rate per kB"), this);
    cbShowMinFeerate->setChecked(false);
    lblMinFeerate = new QLabel(this);
    setupValueUI(cbShowMinFeerate, lblMinFeerate, "green");

    scene = new QGraphicsScene();
    gvChart = new QGraphicsView(scene, this);
    gvChart->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mainlayout->addWidget(gvChart);

    noDataItem = scene->addText(tr("No data available"));
    noDataItem->setDefaultTextColor(QColor(100,100,100, 200));

    QHBoxLayout * const bottomlayout = new QHBoxLayout(this);
    mainlayout->addLayout(bottomlayout);

    sliderScale = new QSlider(Qt::Horizontal, this);
    bottomlayout->addWidget(sliderScale);
    sliderScale->setMinimum(1);
    sliderScale->setMaximum(288);
    sliderScale->setPageStep(12);
    sliderScale->setValue(6);
    connect(sliderScale, SIGNAL(valueChanged(int)), this, SLOT(drawChart()));

    lblGraphRange = new QLabel(this);
    bottomlayout->addWidget(lblGraphRange);
    lblGraphRange->setFixedWidth(100);
    lblGraphRange->setAlignment(Qt::AlignCenter);

    if (parent) {
        parent->installEventFilter(this);
        raise();
    }

    if (clientModel)
        drawChart();
}

QSize MempoolStats::sizeHint() const
{
    QSize sz = QDialog::sizeHint();
    if (sz.width() < 640) {
        sz.setWidth(640);
    }
    sz.setHeight(sz.height() * 2);
    if (sz.height() < 400) {
        sz.setHeight(400);
    }
    return sz;
}

void MempoolStats::setupValueUI(QWidget * const checkbox, QWidget * const label, const QString &color)
{
    const int row = opts_grid->rowCount();

    connect(checkbox, SIGNAL(stateChanged(int)), this, SLOT(drawChart()));
    opts_grid->addWidget(checkbox, row, 0, Qt::AlignLeft);
    checkbox->setStyleSheet(
        "QCheckBox {"
            "border-bottom: 2px solid transparent;"
        "}"
        "QCheckBox::checked {"
            "border-bottom: 2px solid " + color + ";"
        "}"
    );

    label->setStyleSheet(
        "QLabel {"
            "font-weight: bold;"
        "}"
    );
//     label->setAlignment(Qt::AlignRight);
    opts_grid->addWidget(label, row, 1, Qt::AlignRight);
}

void MempoolStats::setClientModel(ClientModel *model)
{
    clientModel = model;

    if (model)
        connect(model, SIGNAL(mempoolStatsDidUpdate()), this, SLOT(drawChart()));
}

void MempoolStats::drawChart()
{
    if (!isVisible())
        return;

    const bool drawTxCount = cbShowNumTxns->isChecked();
    const bool drawMinFee = cbShowMinFeerate->isChecked();
    const bool drawDynMemUsage = cbShowMemUsage->isChecked();

    // remove the items which needs to be redrawn
    for (QGraphicsItem * item : redrawItems)
    {
        scene->removeItem(item);
        delete item;
    }
    redrawItems.clear();

    const int64_t timeFilter = sliderScale->value() * 5 * 60;
    lblGraphRange->setText(GUIUtil::formatDurationStr(timeFilter));

    // get the samples
    QDateTime toDateTime = QDateTime::currentDateTime();
    QDateTime fromDateTime = toDateTime.addSecs(-timeFilter); //-1h

    mempoolSamples_t vSamples = clientModel->getMempoolStatsInRange(fromDateTime, toDateTime);

    // set the values into the overview labels
    if (vSamples.size())
    {
        lblMemUsage->setText(GUIUtil::formatBytes(vSamples.back().dynMemUsage));
        lblNumTxns->setText(QString::number(vSamples.back().txCount));
        lblMinFeerate->setText(QString::number(vSamples.back().minFeePerK));
    }

    QSizeF chartsize = gvChart->sceneRect().size();

    // don't paint the grind/graph if there are no or only a signle sample
    if (vSamples.size() < 2)
    {
        QFont font = lblMinFeerate->font();
        font.setPointSize(font.pointSize() * 2);
        noDataItem->setFont(QFont(LABEL_FONT, LABEL_TITLE_SIZE, QFont::Light));
        setCenterPos(noDataItem, chartsize / 2);
        noDataItem->setVisible(true);
        return;
    }
    noDataItem->setVisible(false);

    int bottom = gvChart->size().height()-GRAPH_PADDING_BOTTOM;
    qreal maxwidth = gvChart->size().width()-GRAPH_PADDING_LEFT-GRAPH_PADDING_RIGHT;
    qreal maxheightG = gvChart->size().height()-GRAPH_PADDING_TOP-GRAPH_PADDING_BOTTOM;
    float paddingTopSizeFactor = 1.2;
    qreal step = maxwidth/(double)vSamples.size();

    // make sure we skip samples that would be drawn narrower then 1px
    // larger window can result in drawing more samples
    int samplesStep = 1;
    if (step < 1)
        samplesStep = ceil(1/samplesStep);

    // find maximum values
    int64_t maxDynMemUsage = 0;
    int64_t minDynMemUsage = std::numeric_limits<int64_t>::max();
    int64_t maxTxCount = 0;
    int64_t minTxCount = std::numeric_limits<int64_t>::max();
    int64_t maxMinFee = 0;
    uint32_t maxTimeDetla = vSamples.back().timeDelta-vSamples.front().timeDelta;
    for(const struct CStatsMempoolSample &sample : vSamples)
    {
        if (sample.dynMemUsage > maxDynMemUsage)
            maxDynMemUsage = sample.dynMemUsage;

        if (sample.dynMemUsage < minDynMemUsage)
            minDynMemUsage = sample.dynMemUsage;

        if (sample.txCount > maxTxCount)
            maxTxCount = sample.txCount;

        if (sample.txCount < minTxCount)
            minTxCount = sample.txCount;

        if (sample.minFeePerK > maxMinFee)
            maxMinFee = sample.minFeePerK;
    }

    int64_t dynMemUsagelog10Val = pow(10.0f, floor(log10(maxDynMemUsage*paddingTopSizeFactor-minDynMemUsage)));
    int64_t topDynMemUsage = ceil((double)maxDynMemUsage*paddingTopSizeFactor/dynMemUsagelog10Val)*dynMemUsagelog10Val;
    int64_t bottomDynMemUsage = floor((double)minDynMemUsage/dynMemUsagelog10Val)*dynMemUsagelog10Val;

    int64_t txCountLog10Val = pow(10.0f, floor(log10(maxTxCount*paddingTopSizeFactor-minTxCount)));
    int64_t topTxCount = ceil((double)maxTxCount*paddingTopSizeFactor/txCountLog10Val)*txCountLog10Val;
    int64_t bottomTxCount = floor((double)minTxCount/txCountLog10Val)*txCountLog10Val;
    
    qreal currentX = GRAPH_PADDING_LEFT;
    QPainterPath dynMemUsagePath(QPointF(currentX, bottom));
    QPainterPath txCountPath(QPointF(currentX, bottom));
    QPainterPath minFeePath(QPointF(currentX, bottom));

    // draw the three possible paths
    for (mempoolSamples_t::iterator it = vSamples.begin(); it != vSamples.end(); it+=samplesStep)
    {
        const struct CStatsMempoolSample &sample = (*it);
        qreal xPos = maxTimeDetla > 0 ? maxwidth/maxTimeDetla*(sample.timeDelta-vSamples.front().timeDelta) : maxwidth/(double)vSamples.size();
        if (sample.timeDelta == vSamples.front().timeDelta)
        {
            dynMemUsagePath.moveTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/(topDynMemUsage-bottomDynMemUsage)*(sample.dynMemUsage-bottomDynMemUsage));
            txCountPath.moveTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/(topTxCount-bottomTxCount)*(sample.txCount-bottomTxCount));
            minFeePath.moveTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/maxMinFee*sample.minFeePerK);
        }
        else
        {
            dynMemUsagePath.lineTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/(topDynMemUsage-bottomDynMemUsage)*(sample.dynMemUsage-bottomDynMemUsage));
            txCountPath.lineTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/(topTxCount-bottomTxCount)*(sample.txCount-bottomTxCount));
            minFeePath.lineTo(GRAPH_PADDING_LEFT+xPos, bottom-maxheightG/maxMinFee*sample.minFeePerK);
        }
    }

    // copy the path for the fill
    QPainterPath dynMemUsagePathFill(dynMemUsagePath);

    // close the path for the fill
    dynMemUsagePathFill.lineTo(GRAPH_PADDING_LEFT+maxwidth, bottom);
    dynMemUsagePathFill.lineTo(GRAPH_PADDING_LEFT, bottom);

    QPainterPath dynMemUsageGridPath(QPointF(currentX, bottom));

    // draw horizontal grid
    int amountOfLinesH = 5;
    QFont gridFont;
    gridFont.setPointSize(8);
    for (int i=0; i < amountOfLinesH; i++)
    {
        qreal lY = bottom-i*(maxheightG/(amountOfLinesH-1));
        dynMemUsageGridPath.moveTo(GRAPH_PADDING_LEFT, lY);
        dynMemUsageGridPath.lineTo(GRAPH_PADDING_LEFT+maxwidth, lY);

        size_t gridDynSize = (float)i*(topDynMemUsage-bottomDynMemUsage)/(amountOfLinesH-1) + bottomDynMemUsage;
        size_t gridTxCount = (float)i*(topTxCount-bottomTxCount)/(amountOfLinesH-1) + bottomTxCount;

        QGraphicsTextItem *itemDynSize = scene->addText(GUIUtil::formatBytes(gridDynSize), gridFont);
        QGraphicsTextItem *itemTxCount = scene->addText(QString::number(gridTxCount), gridFont);

        itemDynSize->setPos(GRAPH_PADDING_LEFT-itemDynSize->boundingRect().width(), lY-(itemDynSize->boundingRect().height()/2));
        itemTxCount->setPos(GRAPH_PADDING_LEFT+maxwidth, lY-(itemDynSize->boundingRect().height()/2));
        redrawItems.append(itemDynSize);
        redrawItems.append(itemTxCount);
    }

    // draw vertical grid
    int amountOfLinesV = 4;
    QDateTime drawTime(fromDateTime);
    std::string fromS = fromDateTime.toString().toStdString();
    std::string toS = toDateTime.toString().toStdString();
    qint64 secsTotal = fromDateTime.secsTo(toDateTime);
    for (int i=0; i <= amountOfLinesV; i++)
    {
        qreal lX = i*(maxwidth/(amountOfLinesV));
        dynMemUsageGridPath.moveTo(GRAPH_PADDING_LEFT+lX, bottom);
        dynMemUsageGridPath.lineTo(GRAPH_PADDING_LEFT+lX, bottom-maxheightG);

        QGraphicsTextItem *item = scene->addText(drawTime.toString("HH:mm"), gridFont);
        item->setPos(GRAPH_PADDING_LEFT+lX-(item->boundingRect().width()/2), bottom);
        redrawItems.append(item);
        qint64 step = secsTotal/amountOfLinesV;
        drawTime = drawTime.addSecs(step);
    }

    // materialize path
    QPen gridPen(QColor(100,100,100, 200), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    redrawItems.append(scene->addPath(dynMemUsageGridPath, gridPen));

    // draw semi-transparent gradient for the dynamic memory size fill
    QLinearGradient gradient(currentX, bottom, currentX, 0);
    gradient.setColorAt(1.0,	QColor(15,68,113, 250));
    gradient.setColorAt(0,QColor(255,255,255,0));
    QBrush graBru(gradient);

    QPen linePenBlue(QColor(15,68,113, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen linePenRed(QColor(188,49,62, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen linePenGreen(QColor(49,188,62, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    if (drawTxCount)
        redrawItems.append(scene->addPath(txCountPath, linePenRed));
    if (drawMinFee)
        redrawItems.append(scene->addPath(minFeePath, linePenGreen));
    if (drawDynMemUsage)
    {
        redrawItems.append(scene->addPath(dynMemUsagePath, linePenBlue));
        redrawItems.append(scene->addPath(dynMemUsagePathFill, QPen(Qt::NoPen), graBru));
    }
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void MempoolStats::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scene->setSceneRect(QRect(QPoint(0, 0), gvChart->contentsRect().size()));
    drawChart();
}

void MempoolStats::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (clientModel)
        drawChart();
}

MempoolStats::~MempoolStats()
{
    for (QGraphicsItem * item : redrawItems)
    {
        scene->removeItem(item);
        delete item;
    }
    redrawItems.clear();

    delete noDataItem;
    delete scene;
}
