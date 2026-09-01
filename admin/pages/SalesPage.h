#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

// 管理员端 · 销售统计页(待填充:QtCharts 销售图表,需先安装 Qt6Charts)
class SalesPage : public QWidget
{
    Q_OBJECT
public:
    explicit SalesPage(QWidget *parent = nullptr);
};

#endif // SALESPAGE_H
