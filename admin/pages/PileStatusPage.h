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
    // 全量重建表格(桩/站状态真变、筛选、手动刷新)
    void refresh();
    // 充电中每拍进度上报:只原地更新"使用中"行的 4 个进度格,避免整表重建的每秒闪烁
    void refreshActiveProgress();

private:
    QComboBox *m_statusFilter = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // PILESTATUSPAGE_H
