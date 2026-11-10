#include "CredentialStore.h"

#include <QHash>

#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#include <string>
#endif

namespace tg {
namespace {
constexpr auto kService = "tr.ticketgoteknoloji.izleme";

#if defined(Q_OS_MACOS)
CFStringRef cf(const QString& text)
{
    return CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(text.utf16()), text.size());
}

struct MacQuery {
    CFMutableDictionaryRef query = nullptr;
    SecKeychainRef keychain = nullptr;
    ~MacQuery()
    {
        if (query) CFRelease(query);
        if (keychain) CFRelease(keychain);
    }
};

QString keychainStatusText(OSStatus status, const QString& action)
{
    if (status == errSecMissingEntitlement) return action + QStringLiteral(" Uygulamanın veri koruma anahtar zinciri yetkisi yok.");
    if (status == errSecNoSuchKeychain) return action + QStringLiteral(" Oturum açma anahtar zinciri bulunamadı.");
    return action + QStringLiteral(" (") + QString::number(int(status)) + QStringLiteral(").");
}

bool beginLoginQuery(MacQuery& out, const QString& account, QString* error)
{
    OSStatus copied = errSecNoSuchKeychain;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    copied = SecKeychainCopyDefault(&out.keychain);
#pragma clang diagnostic pop
    if (copied != errSecSuccess || !out.keychain) {
        if (error) *error = keychainStatusText(copied, QStringLiteral("Oturum açma anahtar zinciri açılamadı."));
        return false;
    }
    out.query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(out.query, kSecClass, kSecClassGenericPassword);
    CFStringRef service = cf(QString::fromLatin1(kService));
    CFStringRef accountRef = cf(account);
    CFDictionarySetValue(out.query, kSecAttrService, service);
    CFDictionarySetValue(out.query, kSecAttrAccount, accountRef);
    CFDictionarySetValue(out.query, kSecUseKeychain, out.keychain);
    CFDictionarySetValue(out.query, kSecUseDataProtectionKeychain, kCFBooleanFalse);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CFDictionarySetValue(out.query, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
#pragma clang diagnostic pop
    CFRelease(service);
    CFRelease(accountRef);
    return true;
}
#endif
} // namespace

bool MemoryCredentialStore::setSecret(const QString& account, const QString& secret, QString* error)
{
    if (account.trimmed().isEmpty() || secret.isEmpty()) {
        if (error) *error = QStringLiteral("Kimlik bilgisi boş.");
        return false;
    }
    values_.insert(account, secret);
    return true;
}

QString MemoryCredentialStore::secret(const QString& account, bool* ok, QString* error) const
{
    if (!values_.contains(account)) {
        if (ok) *ok = false;
        if (error) *error = QStringLiteral("Kimlik bilgisi yok.");
        return {};
    }
    if (ok) *ok = true;
    return values_.value(account);
}

bool MemoryCredentialStore::removeSecret(const QString& account, QString*)
{
    return values_.remove(account) > 0;
}

bool MemoryCredentialStore::contains(const QString& account) const
{
    return values_.contains(account);
}

QString MemoryCredentialStore::backendName() const { return QStringLiteral("memory"); }

bool KeychainCredentialStore::setSecret(const QString& account, const QString& secret, QString* error)
{
    if (secret.isEmpty()) {
        if (error) *error = QStringLiteral("Boş giz kaydedilmez.");
        return false;
    }
#if defined(Q_OS_MACOS)
    removeSecret(account, nullptr);
    MacQuery query;
    if (!beginLoginQuery(query, account, error)) return false;
    const QByteArray bytes = secret.toUtf8();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(bytes.constData()), bytes.size());
    CFDictionarySetValue(query.query, kSecValueData, data);
    const OSStatus status = SecItemAdd(query.query, nullptr);
    CFRelease(data);
    if (status != errSecSuccess) {
        if (error) *error = keychainStatusText(status, QStringLiteral("Anahtar zinciri kaydı başarısız."));
        return false;
    }
    return true;
#elif defined(Q_OS_WIN)
    CREDENTIALW cred{};
    const std::wstring target = (QString::fromLatin1(kService) + QLatin1Char('/') + account).toStdWString();
    const QByteArray blob = secret.toUtf8();
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target.c_str());
    cred.CredentialBlobSize = DWORD(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(blob.constData()));
    cred.Persist = CRED_PERSIST_ENTERPRISE;
    cred.UserName = const_cast<LPWSTR>(L"ticketgo-izleme");
    if (!CredWriteW(&cred, 0)) {
        if (error) *error = QStringLiteral("Windows kimlik yöneticisi kaydı başarısız.");
        return false;
    }
    return true;
#else
    if (error) *error = QStringLiteral("Bu işletim sisteminde anahtar zinciri yok. Düz metin yedek kullanılmadı.");
    return false;
#endif
}

QString KeychainCredentialStore::secret(const QString& account, bool* ok, QString* error) const
{
#if defined(Q_OS_MACOS)
    MacQuery query;
    if (!beginLoginQuery(query, account, error)) {
        if (ok) *ok = false;
        return {};
    }
    CFDictionarySetValue(query.query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query.query, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query.query, &result);
    if (status != errSecSuccess || !result) {
        if (ok) *ok = false;
        if (error) *error = status == errSecItemNotFound ? QStringLiteral("Anahtar zincirinde kayıt yok.") : keychainStatusText(status, QStringLiteral("Anahtar zinciri okunamadı."));
        return {};
    }
    const auto* data = static_cast<CFDataRef>(result);
    QString value = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), int(CFDataGetLength(data)));
    CFRelease(result);
    if (ok) *ok = true;
    return value;
#elif defined(Q_OS_WIN)
    const std::wstring target = (QString::fromLatin1(kService) + QLatin1Char('/') + account).toStdWString();
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred) || !cred) {
        if (ok) *ok = false;
        if (error) *error = QStringLiteral("Windows kimlik kaydı yok.");
        return {};
    }
    const QString value = QString::fromUtf8(reinterpret_cast<const char*>(cred->CredentialBlob), int(cred->CredentialBlobSize));
    CredFree(cred);
    if (ok) *ok = true;
    return value;
#else
    Q_UNUSED(account);
    if (ok) *ok = false;
    if (error) *error = QStringLiteral("Anahtar zinciri yok.");
    return {};
#endif
}

bool KeychainCredentialStore::removeSecret(const QString& account, QString* error)
{
#if defined(Q_OS_MACOS)
    MacQuery query;
    if (!beginLoginQuery(query, account, error)) return false;
    const OSStatus status = SecItemDelete(query.query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
        if (error) *error = keychainStatusText(status, QStringLiteral("Anahtar zinciri silinemedi."));
        return false;
    }
    return true;
#elif defined(Q_OS_WIN)
    const std::wstring target = (QString::fromLatin1(kService) + QLatin1Char('/') + account).toStdWString();
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        if (GetLastError() == ERROR_NOT_FOUND) return true;
        if (error) *error = QStringLiteral("Windows kimlik kaydı silinemedi.");
        return false;
    }
    return true;
#else
    Q_UNUSED(account);
    if (error) *error = QStringLiteral("Anahtar zinciri yok.");
    return false;
#endif
}

bool KeychainCredentialStore::contains(const QString& account) const
{
    bool ok = false;
    secret(account, &ok, nullptr);
    return ok;
}

QString KeychainCredentialStore::backendName() const
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("macos-keychain");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows-credential-manager");
#else
    return QStringLiteral("unavailable");
#endif
}

} // namespace tg
