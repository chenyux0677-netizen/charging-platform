#include "SalesPage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QPieLegendMarker>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

#include <QPainter>
#include <algorithm>

// Qt6 的 Qt Charts 类(QChart / QBarSeries / QPieSlice ...)在全局命名空间,
// 与 Qt5 的 QtCharts:: 前缀不同,所以这里直接写类名,不用命名空间限定。

namespace {

// 单站的聚合统计(销售统计图表的输入)
struct StationStat {
    QString name;
    int orderCount = 0;   // 已完成订单数
    double revenue = 0.0; // 营收(元)
    double energy = 0.0;  // 充电量(kWh)
};

// 把订单里存的开始时间解析成 QDateTime。库里是 localtime 字符串,
// 老版本可能用空格分隔,新版本 Qt ISODate 用 'T' 分隔,两种都兼容。
QDateTime parseOrderTime(const QVariant &v)
{
    const QString t = v.toString();
    QDateTime dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dt;
}

} // namespace

SalesPage::SalesPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("销售统计"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

    // ---- 顶部工具行:统计范围 + 手动刷新 ----
    auto *toolRow = new QHBoxLayout;
    toolRow->addWidget(new QLabel(QStringLiteral("统计范围:"), this));
    m_rangeFilter = new QComboBox(this);
    m_rangeFilter->addItem(QStringLiteral("全部"));
    m_rangeFilter->addItem(QStringLiteral("今日"));
    m_rangeFilter->addItem(QStringLiteral("近 7 天"));
    m_rangeFilter->addItem(QStringLiteral("近 30 天"));
    toolRow->addWidget(m_rangeFilter);
    toolRow->addStretch(1);

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    toolRow->addWidget(refreshBtn);

    // ---- 汇总卡片:订单数 / 营收 / 充电量 / 平均每单 ----
    auto *summaryRow = new QHBoxLayout;
    const auto makeCard = [this, summaryRow](const QString &caption) -> QLabel * {
        auto *card = new QWidget(this);
        card->setObjectName(QStringLiteral("summaryCard"));
        auto *cap = new QLabel(caption, card);
        cap->setObjectName(QStringLiteral("summaryCaptionLabel"));
        auto *value = new QLabel(QStringLiteral("-"), card);
        value->setObjectName(QStringLiteral("summaryValueLabel"));
        auto *box = new QVBoxLayout(card);
        box->setContentsMargins(12, 10, 12, 10);
        box->setSpacing(2);
        box->addWidget(cap);
        box->addWidget(value);
        summaryRow->addWidget(card, 1);
        return value;
    };
    m_orderCount = makeCard(QStringLiteral("已完成订单(单)"));
    m_revenueLabel = makeCard(QStringLiteral("总营收(元)"));
    m_energyLabel = makeCard(QStringLiteral("总充电量(kWh)"));
    m_avgLabel = makeCard(QStringLiteral("平均每单(元)"));

    // ---- 图表区:营收柱状图 + 订单占比饼图(并排) ----
    m_barView = new QChartView(this);
    m_barView->setRenderHint(QPainter::Antialiasing);
    m_pieView = new QChartView(this);
    m_pieView->setRenderHint(QPainter::Antialiasing);

    m_emptyLabel = new QLabel(QStringLiteral("该范围内暂无已完成订单\n先让用户完成一单充电,再来看看图表"), this);
    m_emptyLabel->setObjectName(QStringLiteral("placeholderLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();

    auto *chartRow = new QHBoxLayout;
    chartRow->addWidget(m_barView, 1);
    chartRow->addWidget(m_pieView, 1);
    chartRow->addWidget(m_emptyLabel, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(toolRow);
    layout->addLayout(summaryRow);
    layout->addLayout(chartRow, 1);

    // 换范围 / 手动刷新 / 订单与站桩变化(结算、用户端操作)都触发刷新
    connect(m_rangeFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SalesPage::refresh);
    connect(refreshBtn, &QPushButton::clicked, this, &SalesPage::refresh);
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("orders")
                || table == QStringLiteral("charging_piles")
                || table == QStringLiteral("charging_stations"))
                refresh();
        });
    }

    refresh();
}

void SalesPage::refresh()
{
    // 归一:把旧图表交给 deleteLater 销毁,再换上全新图表(避免重建时残留旧序列)
    const auto replaceChart = [](QChartView *view, QChart *chart) {
        if (QChart *old = view->chart())
            old->deleteLater();
        view->setChart(chart); // setChart 接管新图的所有权
    };

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds) {
        m_emptyLabel->show();
        m_barView->hide();
        m_pieView->hide();
        return;
    }

    // 统计范围起点:全部 = 不限制;今日 / 近 7 天 / 近 30 天从对应天数的 0 点算起
    const int rangeIdx = m_rangeFilter->currentIndex();
    QDateTime rangeStart;
    if (rangeIdx > 0) {
        QDateTime todayStart = QDateTime::currentDateTime();
        todayStart.setTime(QTime(0, 0, 0));
        if (rangeIdx == 1)      // 今日
            rangeStart = todayStart;
        else if (rangeIdx == 2) // 近 7 天(含今天 → 往前 6 天)
            rangeStart = todayStart.addDays(-6);
        else                    // 近 30 天(往前 29 天)
            rangeStart = todayStart.addDays(-29);
    }

    // 取全量已完成订单 + 站 / 桩映射,在页面侧做聚合(演示数据量小,够用)
    const QueryResult orders = ds->query(QStringLiteral("orders"), {},
                                         QStringLiteral("status = '已完成'"));
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));
    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));

    // pile_id -> 站名(桩或站被删过就归到"未知站",保证图表不因脏数据崩)
    QHash<qlonglong, QString> stationName;
    for (const DataRow &st : stations)
        stationName.insert(st.value(QStringLiteral("id")).toLongLong(),
                           st.value(QStringLiteral("name")).toString());
    QHash<qlonglong, QString> pileStation;
    for (const DataRow &p : piles)
        pileStation.insert(p.value(QStringLiteral("id")).toLongLong(),
                           stationName.value(p.value(QStringLiteral("station_id")).toLongLong(),
                                             QStringLiteral("未知站")));

    // 按站聚合;summedOrders / totalRevenue / totalEnergy 是对全部订单求和,
    // 即"范围内已完成订单"的口径(不含充电中、不含被范围跳过的)。
    QHash<QString, StationStat> agg;
    QVector<QString> stationOrder; // 首见顺序,最后按营收重排
    int summedOrders = 0;
    double totalRevenue = 0.0;
    double totalEnergy = 0.0;
    for (const DataRow &o : orders) {
        if (rangeStart.isValid()) {
            const QDateTime start = parseOrderTime(o.value(QStringLiteral("start_time")));
            if (!start.isValid() || start < rangeStart)
                continue; // 超范围或时间读不出(仅在开启范围筛选时跳过)
        }
        const QString name = pileStation.value(
            o.value(QStringLiteral("pile_id")).toLongLong(), QStringLiteral("未知站"));
        if (!agg.contains(name))
            stationOrder << name;
        StationStat &s = agg[name];
        s.name = name; // 之前只拿它当 key,漏了把站名写回结构体 → 图表里站名全空
        ++s.orderCount;
        const double amount = o.value(QStringLiteral("amount")).toDouble();
        s.revenue += amount;
        s.energy += o.value(QStringLiteral("energy_kwh")).toDouble();
        ++summedOrders;
        totalRevenue += amount;
        totalEnergy += o.value(QStringLiteral("energy_kwh")).toDouble();
    }

    // 汇总卡
    m_orderCount->setText(QString::number(summedOrders));
    m_revenueLabel->setText(QString::number(totalRevenue, 'f', 2));
    m_energyLabel->setText(QString::number(totalEnergy, 'f', 1));
    m_avgLabel->setText(QString::number(
        summedOrders > 0 ? totalRevenue / summedOrders : 0.0, 'f', 2));

    // 图表输入:按营收降序,便于一眼看到最赚钱的站
    QVector<StationStat> stats;
    stats.reserve(stationOrder.size());
    for (const QString &name : stationOrder)
        stats << agg.value(name);
    std::sort(stats.begin(), stats.end(), [](const StationStat &a, const StationStat &b) {
        return a.revenue > b.revenue;
    });

    if (stats.isEmpty()) {
        m_emptyLabel->show();
        m_barView->hide();
        m_pieView->hide();
        return;
    }
    m_emptyLabel->hide();
    m_barView->show();
    m_pieView->show();

    // ---- 柱状图:各充电站营收对比(元) ----
    {
        auto *chart = new QChart;
        chart->setTitle(QStringLiteral("各充电站营收对比(元)"));
        chart->setAnimationOptions(QChart::SeriesAnimations);

        auto *barSeries = new QBarSeries;
        auto *set = new QBarSet(QStringLiteral("营收"));
        for (const StationStat &s : stats)
            set->append(s.revenue);
        barSeries->append(set);
        barSeries->setLabelsVisible(true);
        // Qt6 Charts 的柱上标签只认 @value 占位符,不支持 C printf 的 "%.1f"
        // ——那种写法会被当作普通文字原样画在每个柱子里。
        // 这里不设 labelsFormat(走默认:显示该柱数值),用 labelsPrecision
        // 限制有效数字,并让数值显示在柱顶外侧。
        barSeries->setLabelsPrecision(4);
        barSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
        chart->addSeries(barSeries);

        auto *axisX = new QBarCategoryAxis;
        QStringList cats;
        for (const StationStat &s : stats)
            cats << s.name;
        axisX->append(cats);
        chart->addAxis(axisX, Qt::AlignBottom);
        barSeries->attachAxis(axisX);

        auto *axisY = new QValueAxis;
        axisY->setTitleText(QStringLiteral("元"));
        axisY->setLabelFormat(QStringLiteral("%.1f"));
        chart->addAxis(axisY, Qt::AlignLeft);
        barSeries->attachAxis(axisY);
        // 柱顶标签需要余量:Y 上限抬高到最高营收的 1.2 倍,避免最高的柱顶标签贴边被裁
        double maxRevenue = 0.0;
        for (const StationStat &s : stats)
            maxRevenue = qMax(maxRevenue, s.revenue);
        if (maxRevenue > 0.0)
            axisY->setRange(0.0, maxRevenue * 1.2);
        chart->legend()->setVisible(false);

        replaceChart(m_barView, chart);
    }

    // ---- 饼图:各充电站已完成订单数占比 ----
    {
        auto *chart = new QChart;
        chart->setTitle(QStringLiteral("各充电站订单量占比"));
        chart->setAnimationOptions(QChart::SeriesAnimations);

        auto *pieSeries = new QPieSeries;
        for (const StationStat &s : stats) {
            QPieSlice *slice = pieSeries->append(s.name, s.orderCount);
            slice->setProperty("stationName", s.name); // 属性名必须是 const char*
            // 饼上文字 = 每站订单笔数
            slice->setLabel(QStringLiteral("%1 单").arg(s.orderCount));
        }
        pieSeries->setLabelsVisible(true);
        pieSeries->setLabelsPosition(QPieSlice::LabelOutside);

        chart->addSeries(pieSeries);

        // 图例 = 色块释义,应显示"站名"。图例文本默认镜像扇区 label("X 单"),
        // 但 QLegendMarker 自带 label 可单独覆盖,这里逐条目改成对应站名。
        // (markers 在 addSeries 后即已生成,同步设置即可)
        const QList<QLegendMarker *> legendMarks = chart->legend()->markers(pieSeries);
        for (QLegendMarker *base : legendMarks) {
            if (auto *m = qobject_cast<QPieLegendMarker *>(base)) {
                if (QPieSlice *sl = m->slice())
                    m->setLabel(sl->property("stationName").toString());
            }
        }
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignRight);

        replaceChart(m_pieView, chart);
    }
}
