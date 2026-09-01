#pragma once

#include <wx/wx.h>

#include "TerminalSettings.h"

typedef struct _GtkWidget GtkWidget;

enum class TerminalKind
{
    LocalShell,
    Ssh,
    Sftp,
    Ftp,
    ReverseShell,
};

// Hosts a local shell in a real, embedded VTE terminal widget (the same
// emulator used by GNOME Terminal/Terminator/Tilix), so escape sequences,
// colors, cursor addressing and the alternate screen buffer are all handled
// correctly instead of being reimplemented here. GTK-only: relies on wxGTK
// exposing its container as a native GtkWidget via GetHandle().
class TerminalPanel : public wxPanel
{
public:
    // If argv is empty, spawns the user's default local shell. Otherwise
    // argv[0] is the command to run (searched on PATH) and the rest are its
    // arguments — e.g. {"ssh", "-p", "2222", "user@host"} for an SSH session.
    explicit TerminalPanel(wxWindow* parent, const wxArrayString& argv = wxArrayString(),
                           TerminalKind kind = TerminalKind::LocalShell);
    ~TerminalPanel() override;

    void ApplySettings(const TerminalSettings& settings);
    const TerminalSettings& GetSettings() const { return m_settings; }

    TerminalKind GetKind() const { return m_kind; }

    // For a reverse-shell tab: upgrades the raw, non-interactive remote shell
    // to a full PTY-backed one (the standard `python -c 'import pty; ...'`
    // technique), matches its TERM/window size to this terminal, and puts
    // the local pty into raw mode so the remote pty's own echo isn't doubled
    // locally. Only meaningful for TerminalKind::ReverseShell.
    void UpgradeToFullTty();

private:
    void OnSetFocus(wxFocusEvent& event);
    void AllocateVteToFill(int width, int height);
    void ShowContextMenu(const wxPoint& pos);
    void SplitPane(bool vertical);
    void ClosePane();
    void CopySelection();
    void PasteFromClipboard();

    GtkWidget* m_vte = nullptr;
    GtkWidget* m_scrolledWindow = nullptr;
    GtkWidget* m_container = nullptr;
    TerminalSettings m_settings;
    TerminalKind m_kind = TerminalKind::LocalShell;
    wxArrayString m_argv;

    // Debounces AllocateVteToFill during a live sash drag: applying every
    // single intermediate size-allocate resizes the pty (SIGWINCH) that many
    // times, and a prompt that redraws itself on resize (starship,
    // powerlevel10k, ...) can't keep up and leaves overlapping partial
    // redraws behind. See the .cpp for how these are used.
    unsigned int m_resizeSourceId = 0;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;
};
