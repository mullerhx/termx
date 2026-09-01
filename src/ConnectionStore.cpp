#include "ConnectionStore.h"
#include "VaultCrypto.h"

#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/mstream.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <sys/stat.h>

wxString ConnectionProtocolLabel(ConnectionProtocol protocol)
{
    switch (protocol)
    {
        case ConnectionProtocol::Sftp: return "SFTP";
        case ConnectionProtocol::Ftp: return "FTP";
        default: return "SSH";
    }
}

wxString ConnectionProfile::BuildTarget() const
{
    wxString trimmedHost = host;
    trimmedHost.Trim().Trim(false);
    wxString trimmedUser = username;
    trimmedUser.Trim().Trim(false);
    return trimmedUser.IsEmpty() ? trimmedHost : (trimmedUser + "@" + trimmedHost);
}

wxArrayString ConnectionProfile::BuildArgv() const
{
    wxArrayString argv;

    if (protocol == ConnectionProtocol::Ftp)
    {
        // Plain FTP predates key-based auth and per-user @host targets in
        // most clients' argument parsing, so it's kept to what every common
        // ftp client actually accepts: a bare host, with the login prompted
        // for interactively once connected (or supplied via .netrc).
        argv.Add("ftp");
        wxString trimmedHost = host;
        trimmedHost.Trim().Trim(false);
        argv.Add(trimmedHost);
        if (port != 21)
            argv.Add(wxString::Format("%d", port));
        return argv;
    }

    // Ssh and Sftp share ssh's option set — sftp is literally built on the
    // ssh client — they just spell the port flag differently.
    argv.Add(protocol == ConnectionProtocol::Sftp ? "sftp" : "ssh");

    const int defaultPort = 22;
    if (port != defaultPort)
    {
        argv.Add(protocol == ConnectionProtocol::Sftp ? "-P" : "-p");
        argv.Add(wxString::Format("%d", port));
    }

    if (!identityFile.IsEmpty())
    {
        argv.Add("-i");
        argv.Add(identityFile);
    }

    // Explicit regardless of selection, rather than relying on whatever this
    // system's /etc/ssh/ssh_config or user config happens to default to —
    // the profile's saved choice should be exactly what happens, every time.
    const char* strictHostKeyChecking = "ask";
    switch (hostKeyChecking)
    {
        case SshHostKeyMode::Strict: strictHostKeyChecking = "yes"; break;
        case SshHostKeyMode::AcceptNew: strictHostKeyChecking = "accept-new"; break;
        default: break; // Ask
    }
    argv.Add("-o");
    argv.Add(wxString::Format("StrictHostKeyChecking=%s", strictHostKeyChecking));

    if (!knownHostsFile.IsEmpty())
    {
        argv.Add("-o");
        argv.Add("UserKnownHostsFile=" + knownHostsFile);
    }

    argv.Add(BuildTarget());
    return argv;
}

wxString ConnectionStore::GetConfigPath()
{
    // wxStandardPaths::GetUserConfigDir() returns the bare home directory on
    // this system's wx build, not ~/.config — resolve XDG_CONFIG_HOME (or its
    // ~/.config default) ourselves instead.
    wxString configHome;
    if (!wxGetEnv("XDG_CONFIG_HOME", &configHome) || configHome.IsEmpty())
        configHome = wxFileName::GetHomeDir() + "/.config";

    wxFileName path(configHome, "connections.conf");
    path.AppendDir("termx");
    return path.GetFullPath();
}

bool ConnectionStore::VaultFileExists()
{
    return wxFileName::FileExists(GetConfigPath());
}

void ConnectionStore::Load()
{
    m_groups.clear();
    m_locked = false;

    const wxString path = GetConfigPath();
    if (!wxFileName::FileExists(path))
        return; // No vault yet — m_password will become its password on first Save().

    wxFile file(path, wxFile::read);
    if (!file.IsOpened())
    {
        m_locked = true;
        return;
    }

    std::string blob(static_cast<size_t>(file.Length()), '\0');
    if (!blob.empty())
        file.Read(&blob[0], blob.size());
    file.Close();

    std::string plaintext;
    if (!VaultCrypto::Decrypt(blob, m_password, plaintext))
    {
        // Wrong password (or a corrupt/tampered file) — leave the group
        // list empty and the file untouched, and make sure nothing in this
        // process can write over it: a Save() with the wrong password's
        // (wrong) key would permanently destroy whatever's actually in
        // there. Real filesystem permissions, not just an in-memory flag,
        // so this holds even if some other code path forgets to check
        // IsLocked().
        m_locked = true;
        chmod(path.ToUTF8().data(), S_IRUSR);
        return;
    }

    // Correct password: make sure the file isn't stuck read-only from a
    // previous session that guessed wrong.
    chmod(path.ToUTF8().data(), S_IRUSR | S_IWUSR);

    wxMemoryInputStream stream(plaintext.data(), plaintext.size());
    wxFileConfig cfg(stream);

    long groupCount = 0;
    cfg.Read("/Groups/Count", &groupCount, 0);

    for (long i = 0; i < groupCount; ++i)
    {
        const wxString base = wxString::Format("/Groups/Group%ld", i);

        ConnectionGroup group;
        cfg.Read(base + "/Name", &group.name, wxString::Format("Group %ld", i + 1));

        long colorRgb = -1;
        cfg.Read(base + "/Color", &colorRgb, -1);
        if (colorRgb >= 0)
            group.color.SetRGB(static_cast<wxUint32>(colorRgb));

        long connCount = 0;
        cfg.Read(base + "/ConnectionCount", &connCount, 0);

        for (long j = 0; j < connCount; ++j)
        {
            const wxString cbase = wxString::Format("%s/Connection%ld", base, j);

            ConnectionProfile profile;
            cfg.Read(cbase + "/Name", &profile.name, wxString());

            // Missing means a connection saved before Protocol existed —
            // those were all SSH.
            long protocol = 0;
            cfg.Read(cbase + "/Protocol", &protocol, 0);
            profile.protocol = static_cast<ConnectionProtocol>(protocol);

            cfg.Read(cbase + "/Host", &profile.host, wxString());
            cfg.Read(cbase + "/Username", &profile.username, wxString());

            long port = 22;
            cfg.Read(cbase + "/Port", &port, 22);
            profile.port = static_cast<int>(port);

            cfg.Read(cbase + "/IdentityFile", &profile.identityFile, wxString());

            long hostKeyMode = 0;
            cfg.Read(cbase + "/HostKeyChecking", &hostKeyMode, 0);
            profile.hostKeyChecking = static_cast<SshHostKeyMode>(hostKeyMode);

            cfg.Read(cbase + "/KnownHostsFile", &profile.knownHostsFile, wxString());

            if (profile.name.IsEmpty())
                profile.name = profile.BuildTarget();

            group.connections.push_back(profile);
        }

        m_groups.push_back(group);
    }
}

void ConnectionStore::Save() const
{
    // A wrong password must never get the chance to encrypt an empty (or
    // otherwise wrong) group list over the top of the real vault data.
    if (m_locked)
        return;

    // style=0 (neither USE_LOCAL_FILE nor USE_GLOBAL_FILE): a plain
    // wxFileConfig() would otherwise auto-discover and merge in whatever
    // real config file it finds for this app name — this needs to be a
    // clean in-memory buffer we fully control, since it's what gets
    // encrypted wholesale below.
    wxFileConfig cfg(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString, 0);

    cfg.Write("/Groups/Count", static_cast<long>(m_groups.size()));

    for (size_t i = 0; i < m_groups.size(); ++i)
    {
        const ConnectionGroup& group = m_groups[i];
        const wxString base = wxString::Format("/Groups/Group%zu", i);

        cfg.Write(base + "/Name", group.name);
        cfg.Write(base + "/Color",
                  group.color.IsOk() ? static_cast<long>(group.color.GetRGB()) : -1L);
        cfg.Write(base + "/ConnectionCount", static_cast<long>(group.connections.size()));

        for (size_t j = 0; j < group.connections.size(); ++j)
        {
            const ConnectionProfile& profile = group.connections[j];
            const wxString cbase = wxString::Format("%s/Connection%zu", base, j);

            cfg.Write(cbase + "/Name", profile.name);
            cfg.Write(cbase + "/Protocol", static_cast<long>(profile.protocol));
            cfg.Write(cbase + "/Host", profile.host);
            cfg.Write(cbase + "/Username", profile.username);
            cfg.Write(cbase + "/Port", static_cast<long>(profile.port));
            cfg.Write(cbase + "/IdentityFile", profile.identityFile);
            cfg.Write(cbase + "/HostKeyChecking", static_cast<long>(profile.hostKeyChecking));
            cfg.Write(cbase + "/KnownHostsFile", profile.knownHostsFile);
        }
    }

    wxMemoryOutputStream plaintextStream;
    cfg.Save(plaintextStream);

    const wxStreamBuffer* buf = plaintextStream.GetOutputStreamBuffer();
    const std::string plaintext(static_cast<const char*>(buf->GetBufferStart()),
                                static_cast<const char*>(buf->GetBufferEnd()));
    const std::string blob = VaultCrypto::Encrypt(plaintext, m_password);

    const wxString path = GetConfigPath();
    wxFileName::Mkdir(wxFileName(path).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    // Written 0600 from the start (not just after the fact): the plaintext
    // connection details this replaced were previously left group/world
    // readable, and the encrypted blob deserves better regardless.
    wxFile file;
    file.Create(path, true /* overwrite */, wxS_IRUSR | wxS_IWUSR);
    file.Write(blob.data(), blob.size());
    file.Close();
    chmod(path.ToUTF8().data(), S_IRUSR | S_IWUSR);
}
