#pragma once

#include <wx/wx.h>
#include <vector>

// Host key verification modes, shared between ConnectionDialog and
// ConnectionProfile::BuildArgv() so a persisted profile reconnects with
// exactly the same policy it was created with. Meaningless for Ftp (plain
// FTP has no host-key concept) but kept on the profile regardless — simpler
// than a protocol-specific field set, and BuildArgv() just ignores it there.
enum class SshHostKeyMode
{
    Ask = 0,
    Strict = 1,
    AcceptNew = 2,
};

enum class ConnectionProtocol
{
    Ssh = 0,
    Sftp = 1,
    Ftp = 2,
};

wxString ConnectionProtocolLabel(ConnectionProtocol protocol);

struct ConnectionProfile
{
    wxString name; // Tree label and tab title base; defaults to the target.
    ConnectionProtocol protocol = ConnectionProtocol::Ssh;
    wxString host;
    wxString username;
    int port = 22;
    wxString identityFile;
    SshHostKeyMode hostKeyChecking = SshHostKeyMode::Ask;
    wxString knownHostsFile;

    wxString BuildTarget() const;
    wxArrayString BuildArgv() const;
};

struct ConnectionGroup
{
    wxString name;

    // Invalid (default-constructed, !IsOk()) means "no custom color" — new
    // terminal tabs opened from this group's connections just use the
    // normal default tab appearance.
    wxColour color;

    std::vector<ConnectionProfile> connections;
};

// Persists connection groups and their SSH/SFTP/FTP connections, encrypted
// (see VaultCrypto) with a password set once per session via SetPassword(),
// to a config file under the user's home directory.
class ConnectionStore
{
public:
    std::vector<ConnectionGroup>& Groups() { return m_groups; }
    const std::vector<ConnectionGroup>& Groups() const { return m_groups; }

    // True if the vault file exists on disk already — used to decide
    // whether the startup prompt should ask to unlock it or to choose a
    // password for a new one.
    static bool VaultFileExists();

    void SetPassword(const wxString& password) { m_password = password; }

    // True once Load() has run: either the file didn't decrypt with the
    // current password, or it wasn't valid vault data. Groups() is left
    // empty in that case, and Save() refuses to do anything — a wrong
    // password must never overwrite whatever's actually in the file.
    bool IsLocked() const { return m_locked; }

    void Load();

    // No-op while IsLocked() — see above.
    void Save() const;

private:
    static wxString GetConfigPath();

    std::vector<ConnectionGroup> m_groups;
    wxString m_password;
    bool m_locked = false;
};
