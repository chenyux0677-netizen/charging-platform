#ifndef PILESTATUSPAGE_H
#define PILESTATUSPAGE_H

#include <QHash>
#include <QTableWidget>
#include <QWidget>

// 管理员端 · 充电桩状态页:状态数量/占比汇总 + 实时明细(只读)
class QComboBox;
class QLabel;

class PileStatusPage : public QWidget
{
    Q_OBJECT
public:
    explicit PileStatusPage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void refreshActiveProgress();

private:
    QComboBox *m_statusFilter = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_freeValue = nullptr;
    QLabel *m_busyValue = nullptr;
    QLabel *m_faultValue = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // PILESTATUSPAGE_H
