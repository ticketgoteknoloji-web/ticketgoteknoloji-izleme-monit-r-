#pragma once

#include <QHash>
#include <QString>

namespace tg {

class CredentialStore {
public:
    virtual ~CredentialStore() = default;
    virtual bool setSecret(const QString& account, const QString& secret, QString* error) = 0;
    virtual QString secret(const QString& account, bool* ok, QString* error) const = 0;
    virtual bool removeSecret(const QString& account, QString* error) = 0;
    virtual bool contains(const QString& account) const = 0;
    virtual QString backendName() const = 0;
};

class MemoryCredentialStore final : public CredentialStore {
public:
    bool setSecret(const QString& account, const QString& secret, QString* error) override;
    QString secret(const QString& account, bool* ok, QString* error) const override;
    bool removeSecret(const QString& account, QString* error) override;
    bool contains(const QString& account) const override;
    QString backendName() const override;

private:
    QHash<QString, QString> values_;
};

class KeychainCredentialStore final : public CredentialStore {
public:
    bool setSecret(const QString& account, const QString& secret, QString* error) override;
    QString secret(const QString& account, bool* ok, QString* error) const override;
    bool removeSecret(const QString& account, QString* error) override;
    bool contains(const QString& account) const override;
    QString backendName() const override;
};

} // namespace tg
