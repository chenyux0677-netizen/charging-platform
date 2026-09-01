#include "ChargingPage.h"

#include <QLabel>
#include <QVBoxLayout>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *placeholder = new QLabel(QStringLiteral("充电页(待填充)"), this);
    placeholder->setObjectName(QStringLiteral("placeholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(placeholder);
}
