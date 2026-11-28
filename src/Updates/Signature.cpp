#include "Signature.hpp"
#include "Manifest.hpp"
#include <QCryptographicHash>
#include <openssl/evp.h>
#include <memory>

namespace Acheron::Updates {
QList<QByteArray> productionKeys()
{
    // Reviewed public keys only. Network responses and preferences cannot add pins.
    return {};
}

bool verifySignature(const QByteArray &manifest, const QByteArray &envelope, const QList<QByteArray> &pins)
{
    if (manifest.isEmpty() || manifest.size() > ManifestLimit || envelope.size() != 129 || envelope.at(64) != '\n')
        return false;
    for (const auto &raw : pins) {
        if (raw.size() != 32 || QCryptographicHash::hash(raw,QCryptographicHash::Sha256).toHex() != envelope.left(64))
            continue;
        using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
        using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
        Key key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                reinterpret_cast<const unsigned char *>(raw.constData()), 32), EVP_PKEY_free);
        Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        return key && ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1
            && EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char *>(envelope.constData()+65), 64,
                                reinterpret_cast<const unsigned char *>(manifest.constData()), manifest.size()) == 1;
    }
    return false;
}
}
