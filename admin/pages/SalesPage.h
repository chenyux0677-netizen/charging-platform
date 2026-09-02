#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

class QChartView; // Qt6 Charts 的类在全局命名空间(与 Qt5 的 QtCharts:: 不同)
class QComboBox;
class QLabel;

// 管理员端 · 销售统计页:
// 统计"已完成"订单,顶部汇总卡 + 各充电站营收柱状图 + 各充电站订单占比饼图。
// 可切换统计范围(全部 / 今日 / 近 7 天 / 近 30 天),订单变化时自动刷新。
// 需要 Qt6Charts(libqt6charts6-dev)。
class SalesPage : public QWidget
{
    Q_OBJECT
public:
    explicit SalesPage(QWidget *parent = nullptr);

private:
    void refresh();

    QComboBox *m_rangeFilter = nullptr;
    QLabel *m_orderCount = nullptr;   // 已完成订单数
    QLabel *m_revenueLabel = nullptr; // 营收合计(元)
    QLabel *m_energyLabel = nullptr;  // 充电量合计(kWh)
    QLabel *m_avgLabel = nullptr;     // 平均每单金额(元)

    QChartView *m_barView = nullptr; // 各站营收柱状图
    QChartView *m_pieView = nullptr; // 各站订单占比饼图
    QLabel *m_emptyLabel = nullptr;            // 无数据占位
};

#endif // SALESPAGE_H
