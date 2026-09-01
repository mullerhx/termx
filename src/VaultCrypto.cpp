#include "VaultCrypto.h"

#include <nettle/aes.h>
#include <nettle/gcm.h>
#include <nettle/pbkdf2.h>

#include <sys/random.h>

#include <cstring>

namespace
{
    constexpr char kMagic[4] = {'T', 'X', 'V', '1'};
    constexpr size_t kSaltSize = 16;
    constexpr size_t kIvSize = GCM_IV_SIZE;    // 12
    constexpr size_t kTagSize = GCM_DIGEST_SIZE; // 16
    constexpr size_t kKeySize = 32;            // AES-256
    constexpr unsigned kPbkdf2Iterations = 200000;
    constexpr size_t kHeaderSize = sizeof(kMagic) + kSaltSize + kIvSize + kTagSize;

    void FillRandom(uint8_t* buffer, size_t length)
    {
        // getrandom() can return fewer bytes than requested if interrupted
        // by a signal; loop until the buffer is actually full rather than
        // silently using a partially-random (and thus weaker) salt/IV.
        size_t filled = 0;
        while (filled < length)
        {
            const ssize_t n = getrandom(buffer + filled, length - filled, 0);
            if (n <= 0)
                continue;
            filled += static_cast<size_t>(n);
        }
    }

    void DeriveKey(const wxString& password, const uint8_t* salt, uint8_t* outKey)
    {
        const wxScopedCharBuffer utf8 = password.ToUTF8();
        pbkdf2_hmac_sha256(utf8.length(), reinterpret_cast<const uint8_t*>(utf8.data()),
                           kPbkdf2Iterations, kSaltSize, salt, kKeySize, outKey);
    }

    // nettle's gcm_*_update() requires every call's length to be a multiple
    // of GCM_BLOCK_SIZE except the very last one — feeding it the header in
    // three separate short calls (4, 16, 12 bytes) trips that assertion, so
    // it has to go in as a single call instead.
    void UpdateHeader(struct gcm_aes256_ctx* ctx, const uint8_t* salt, const uint8_t* iv)
    {
        uint8_t header[sizeof(kMagic) + kSaltSize + kIvSize];
        memcpy(header, kMagic, sizeof(kMagic));
        memcpy(header + sizeof(kMagic), salt, kSaltSize);
        memcpy(header + sizeof(kMagic) + kSaltSize, iv, kIvSize);
        gcm_aes256_update(ctx, sizeof(header), header);
    }
}

std::string VaultCrypto::Encrypt(const std::string& plaintext, const wxString& password)
{
    uint8_t salt[kSaltSize];
    uint8_t iv[kIvSize];
    FillRandom(salt, kSaltSize);
    FillRandom(iv, kIvSize);

    uint8_t key[kKeySize];
    DeriveKey(password, salt, key);

    struct gcm_aes256_ctx ctx;
    gcm_aes256_set_key(&ctx, key);
    gcm_aes256_set_iv(&ctx, kIvSize, iv);
    // Bind the header (magic + salt + IV) into the authentication tag too,
    // so tampering with any of it — not just the ciphertext — is detected.
    UpdateHeader(&ctx, salt, iv);

    std::string ciphertext(plaintext.size(), '\0');
    gcm_aes256_encrypt(&ctx, plaintext.size(), reinterpret_cast<uint8_t*>(&ciphertext[0]),
                       reinterpret_cast<const uint8_t*>(plaintext.data()));

    uint8_t tag[kTagSize];
    gcm_aes256_digest(&ctx, kTagSize, tag);

    std::string blob;
    blob.reserve(kHeaderSize + ciphertext.size());
    blob.append(kMagic, sizeof(kMagic));
    blob.append(reinterpret_cast<const char*>(salt), kSaltSize);
    blob.append(reinterpret_cast<const char*>(iv), kIvSize);
    blob.append(reinterpret_cast<const char*>(tag), kTagSize);
    blob.append(ciphertext);

    memset(key, 0, sizeof(key));
    return blob;
}

bool VaultCrypto::Decrypt(const std::string& blob, const wxString& password,
                          std::string& outPlaintext)
{
    if (blob.size() < kHeaderSize)
        return false;

    const auto* data = reinterpret_cast<const uint8_t*>(blob.data());
    if (memcmp(data, kMagic, sizeof(kMagic)) != 0)
        return false;

    const uint8_t* salt = data + sizeof(kMagic);
    const uint8_t* iv = salt + kSaltSize;
    const uint8_t* tag = iv + kIvSize;
    const uint8_t* ciphertext = tag + kTagSize;
    const size_t ciphertextLen = blob.size() - kHeaderSize;

    uint8_t key[kKeySize];
    DeriveKey(password, salt, key);

    struct gcm_aes256_ctx ctx;
    gcm_aes256_set_key(&ctx, key);
    gcm_aes256_set_iv(&ctx, kIvSize, iv);
    UpdateHeader(&ctx, salt, iv);

    std::string plaintext(ciphertextLen, '\0');
    if (ciphertextLen > 0)
    {
        gcm_aes256_decrypt(&ctx, ciphertextLen, reinterpret_cast<uint8_t*>(&plaintext[0]),
                           ciphertext);
    }

    uint8_t computedTag[kTagSize];
    gcm_aes256_digest(&ctx, kTagSize, computedTag);
    memset(key, 0, sizeof(key));

    // Constant-time-ish comparison: not that it matters much for a local
    // desktop app with no network-observable timing channel, but costs
    // nothing to do properly.
    uint8_t diff = 0;
    for (size_t i = 0; i < kTagSize; ++i)
        diff |= computedTag[i] ^ tag[i];
    if (diff != 0)
        return false; // wrong password, or the file was tampered with/corrupt

    outPlaintext = std::move(plaintext);
    return true;
}
