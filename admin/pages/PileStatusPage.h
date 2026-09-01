#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QWidget>

// 管理员端 · 充电桩状态页(待填充:各桩实时状态一览)
class PileStatusPage : public QWidget
{
    Q_OBJECT
public:
    explicit PileStatusPage(QWidget *parent = nullptr);
};

#endif // PILESTATUSPAGE_H
