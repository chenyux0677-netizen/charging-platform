#include "OrderPage.h"

#include <QLabel>
#include <QVBoxLayout>

OrderPage::OrderPage(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(QStringLiteral("我的订单页(待填充)"), this);
    placeholder->setObjectName(QStringLiteral("placeholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}
