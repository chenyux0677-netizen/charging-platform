#include "UserManagePage.h"

#include <QLabel>
#include <QVBoxLayout>

UserManagePage::UserManagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(QStringLiteral("用户管理页(待填充)"), this);
    placeholder->setObjectName(QStringLiteral("placeholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}
