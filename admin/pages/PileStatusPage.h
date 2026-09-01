#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QHash>
#include <QTableWidget>
#include <QWidget>

// 管理员端 · 充电桩状态页:全站电桩实时状态一览(只读),可按状态筛选
class QComboBox;

class PileStatusPage : public QWidget
{
    Q_OBJECT
public:
    explicit PileStatusPage(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QComboBox *m_statusFilter = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // PILESTATUSPAGE_H
