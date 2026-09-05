#ifndef PILEMANAGEPAGE_H
#define PILEMANAGEPAGE_H

#include <QHash>
#include <QTableWidget>
#include <QWidget>

// 管理员端 · 充电桩管理页:充电桩增删改查,可按电站筛选
class QComboBox;

class PileManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit PileManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onAdd();
    void onEdit();
    void onRestart();
    void onRemove();

private:
    // 弹出"新增/编辑充电桩"对话框;取消或校验不过返回空 Map
    QHash<QString, QVariant> editPileDialog(const QHash<QString, QVariant> &initial);
    QHash<qlonglong, QString> stationNames() const; // id -> 名称

    QComboBox *m_stationFilter = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // PILEMANAGEPAGE_H
