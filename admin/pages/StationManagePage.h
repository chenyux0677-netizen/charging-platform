#ifndef STATIONMANAGEPAGE_H
#define STATIONMANAGEPAGE_H

#include <QHash>
#include <QTableWidget>
#include <QWidget>

// 管理员端 · 充电站管理页:充电站增删改查
class StationManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit StationManagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onAdd();
    void onEdit();
    void onRemove();
    void onDetails();

private:
    // 弹出"新增/编辑充电站"对话框;取消或校验不过返回空 Map
    QHash<QString, QVariant> editStationDialog(const QHash<QString, QVariant> &initial);

    QTableWidget *m_table = nullptr;
};

#endif // STATIONMANAGEPAGE_H
