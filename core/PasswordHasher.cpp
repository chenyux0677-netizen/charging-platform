#include "PasswordHasher.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>

namespace {
constexpr int kSaltBytes = 16;
constexpr int kIterations = 100000;
constexpr int kDerivedKeyBytes = 32;
}

QString PasswordHasher::generateSalt()
{
    QByteArray salt;
    salt.reserve(kSaltBytes);
    for (int i = 0; i < kSaltBytes; i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        salt.append(static_cast<char>(value));
        salt.append(static_cast<char>(value >> 8));
        salt.append(static_cast<char>(value >> 16));
        salt.append(static_cast<char>(value >> 24));
    }
    return QString::fromLatin1(salt.toHex());
}

QString PasswordHasher::derive(const QString &password, const QString &saltHex)
{
    const QByteArray salt = QByteArray::fromHex(saltHex.toLatin1());
    if (password.isEmpty() || salt.size() != kSaltBytes)
        return QString();
    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), salt,
        kIterations, kDerivedKeyBytes);
    return QString::fromLatin1(key.toHex());
}

bool PasswordHasher::verify(const QString &password, const QString &saltHex,
                            const QString &expectedHex)
{
    const QByteArray actual = derive(password, saltHex).toLatin1();
    const QByteArray expected = expectedHex.toLatin1();
    if (actual.size() != expected.size() || actual.isEmpty())
        return false;

    uchar difference = 0;
    for (qsizetype i = 0; i < actual.size(); ++i)
        difference |= static_cast<uchar>(actual.at(i) ^ expected.at(i));
    return difference == 0;
}
