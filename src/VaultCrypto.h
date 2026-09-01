#pragma once

#include <wx/wx.h>
#include <string>

// Password-based encryption for the connections vault file: PBKDF2-HMAC-
// SHA256 key derivation (a fresh random salt per encryption) feeding
// AES-256-GCM authenticated encryption (a fresh random IV per encryption).
// GCM's authentication tag is what actually tells a wrong password apart
// from a right one — Decrypt() fails whenever that tag doesn't verify,
// rather than relying on whatever garbage plaintext a wrong key happens to
// produce being *parseable* to detect the mismatch.
namespace VaultCrypto
{
    // Encrypts plaintext for the given password. The returned blob is
    // self-contained (magic, salt, IV, tag, ciphertext) — everything
    // Decrypt() needs except the password itself.
    std::string Encrypt(const std::string& plaintext, const wxString& password);

    // Decrypts a blob produced by Encrypt(). Returns false (leaving
    // outPlaintext untouched) if the password is wrong, or the blob is too
    // short/malformed to be one of ours — both are treated identically, so
    // this can't be used to distinguish "wrong password" from "corrupt
    // file" from the return value alone (deliberately: no point telling an
    // attacker which).
    bool Decrypt(const std::string& blob, const wxString& password, std::string& outPlaintext);
}
