#ifndef PASSWORDHASHER_H
#define PASSWORDHASHER_H

#include <QString>

namespace PasswordHasher {

QString generateSalt();
QString derive(const QString &password, const QString &saltHex);
bool verify(const QString &password, const QString &saltHex, const QString &expectedHex);

} // namespace PasswordHasher

#endif // PASSWORDHASHER_H
