#include "MainFrame.h"
#include "AboutIcon.h"
#include "AppIcon.h"
#include "ConnectionDialog.h"
#include "DockBarButton.h"
#include "ExitIcon.h"
#include "GearIcon.h"
#include "GroupDialog.h"
#include "NewTerminalIcon.h"
#include "PreferencesDialog.h"
#include "ReverseShellDialog.h"
#include "TerminalIcon.h"
#include "TerminalPanel.h"

#include <wx/aboutdlg.h>
#include <wx/accel.h>
#include <wx/artprov.h>
#include <wx/mstream.h>
#include <wx/tglbtn.h>
#include <wx/propgrid/advprops.h>
#include <wx/splitter.h>

#include <gtk/gtk.h>

#include <vector>

enum
{
    ID_New = wxID_HIGHEST + 1,
    ID_NewLocalShell,
    ID_NewSshSession,
    ID_NewSftpSession,
    ID_NewFtpSession,
    ID_NewReverseShell,
    ID_UpgradeTty,
    ID_TreeAddGroup,
    ID_TreeRenameGroup,
    ID_TreeDeleteGroup,
    ID_TreeAddConnection,
    ID_TreeEditConnection,
    ID_TreeDeleteConnection,
    ID_TreeConnect,
    ID_ToggleTree,
    ID_ToggleProperties,
    ID_NextTab,
    ID_PreviousTab,
    ID_About = wxID_ABOUT,
    ID_Exit = wxID_EXIT,
    ID_Preferences = wxID_PREFERENCES,
};

namespace
{
    // Tags each tree item with which group/connection it represents.
    // connectionIndex < 0 means the item is a group node itself.
    class ConnectionTreeItemData : public wxTreeItemData
    {
    public:
        ConnectionTreeItemData(int groupIndex, int connectionIndex = -1)
            : m_groupIndex(groupIndex), m_connectionIndex(connectionIndex)
        {
        }

        int GroupIndex() const { return m_groupIndex; }
        int ConnectionIndex() const { return m_connectionIndex; }
        bool IsGroup() const { return m_connectionIndex < 0; }

    private:
        int m_groupIndex;
        int m_connectionIndex;
    };

    // A notebook page is either a lone TerminalPanel, or a (possibly nested)
    // wxSplitterWindow tree of them after one or more splits.
    void CollectTerminals(wxWindow* window, std::vector<TerminalPanel*>& out)
    {
        if (auto* terminal = dynamic_cast<TerminalPanel*>(window))
        {
            out.push_back(terminal);
            return;
        }

        if (auto* splitter = dynamic_cast<wxSplitterWindow*>(window))
        {
            if (splitter->GetWindow1())
                CollectTerminals(splitter->GetWindow1(), out);
            if (splitter->GetWindow2())
                CollectTerminals(splitter->GetWindow2(), out);
        }
    }

    wxBitmap LoadEmbeddedBitmap(const unsigned char* data, unsigned int len, const wxSize& size)
    {
        wxMemoryInputStream stream(data, len);
        wxImage image(stream, wxBITMAP_TYPE_PNG);
        if (image.IsOk())
            image = image.Scale(size.GetWidth(), size.GetHeight(), wxIMAGE_QUALITY_HIGH);
        return wxBitmap(image);
    }
}

MainFrame::MainFrame(const wxString& title, const wxString& vaultPassword)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1000, 700))
{
    // Opacity in TerminalPanel::ApplySettings relies on the background color's
    // alpha channel actually being composited, which needs an RGBA-capable
    // visual on the top-level window requested before it's shown.
    auto* topLevel = reinterpret_cast<GtkWidget*>(GetHandle());
    GdkScreen* screen = gtk_widget_get_screen(topLevel);
    GdkVisual* rgbaVisual = gdk_screen_get_rgba_visual(screen);
    if (rgbaVisual)
        gtk_widget_set_visual(topLevel, rgbaVisual);

    {
        wxMemoryInputStream iconStream(kAppIconPng, kAppIconPngLen);
        wxImage iconImage(iconStream, wxBITMAP_TYPE_PNG);
        if (iconImage.IsOk())
        {
            // A window manager picks whichever of these best matches each of
            // its own UI surfaces (title bar, taskbar, alt-tab, ...) instead
            // of scaling a single size — and critically, none of them comes
            // close to the ~240px-square ceiling where a single request to
            // set _NET_WM_ICON starts silently failing (see AppIcon.h).
            wxIconBundle bundle;
            for (int size : {16, 24, 32, 48, 64, 128})
            {
                wxImage resized = iconImage.Scale(size, size, wxIMAGE_QUALITY_HIGH);
                wxIcon icon;
                icon.CopyFromBitmap(wxBitmap(resized));
                bundle.AddIcon(icon);
            }
            SetIcons(bundle);
        }
    }

    m_terminalSettings.Load();
    BuildToolBar();

    CreateStatusBar();
    SetStatusText("termx");

    m_auiManager.SetManagedWindow(this);

    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);

    m_propGrid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition,
                                     wxDefaultSize, wxPG_SPLITTER_AUTO_CENTER);

    m_connectionStore.SetPassword(vaultPassword);
    m_connectionStore.Load();
    RebuildTree();

    if (m_connectionStore.IsLocked())
    {
        CallAfter([this]() {
            wxMessageBox(
                "The connection vault could not be unlocked with that password.\n\n"
                "No saved connections are shown, and the vault file has been made "
                "read-only for this session so it can't be accidentally overwritten. "
                "Restart termx and enter the correct password to see your connections again.",
                "termx — Connection Vault", wxOK | wxICON_WARNING, this);
        });
    }

    m_notebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxAUI_NB_TOP | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS |
                                   wxAUI_NB_CLOSE_ON_ALL_TABS);
    m_tabArt = new GroupColorTabArt();
    m_notebook->SetArtProvider(m_tabArt);
    AddTerminalTab("Local Shell");

    m_auiManager.AddPane(m_tree, wxAuiPaneInfo()
        .Name("tree")
        .Caption("Tree View")
        .Left()
        .Layer(1)
        .Row(0)
        .Position(0)
        .BestSize(220, 300)
        .MinSize(120, 80)
        .CloseButton(true)
        .MaximizeButton(true)
        .PinButton(true)
        .Dockable(true)
        .Floatable(true));

    m_auiManager.AddPane(m_propGrid, wxAuiPaneInfo()
        .Name("properties")
        .Caption("Properties")
        .Left()
        .Layer(1)
        .Row(0)
        .Position(1)
        .BestSize(220, 300)
        .MinSize(120, 80)
        .CloseButton(true)
        .MaximizeButton(true)
        .PinButton(true)
        .Dockable(true)
        .Floatable(true));

    m_auiManager.AddPane(m_notebook, wxAuiPaneInfo()
        .Name("terminals")
        .CenterPane());

    // A fixed, always-visible vertical strip of toggle buttons docked
    // outside (Layer 2, further left than) the tree/properties panes —
    // this is what actually collapses/expands them: wxAUI's own pane
    // minimize/restore isn't compiled into this platform's wxAUI build.
    m_dockBarPanel = new wxPanel(this, wxID_ANY);
    m_treeToggle = new DockBarButton(m_dockBarPanel, ID_ToggleTree,
                                     wxArtProvider::GetBitmap(wxART_FOLDER_OPEN, wxART_TOOLBAR, wxSize(16, 16)),
                                     "Tree View");
    m_treeToggle->SetToggled(true);
    m_propertiesToggle = new DockBarButton(m_dockBarPanel, ID_ToggleProperties,
                                           wxArtProvider::GetBitmap(wxART_LIST_VIEW, wxART_TOOLBAR, wxSize(16, 16)),
                                           "Properties");
    m_propertiesToggle->SetToggled(true);

    auto* dockBarSizer = new wxBoxSizer(wxVERTICAL);
    dockBarSizer->Add(m_treeToggle, 0, wxEXPAND);
    dockBarSizer->Add(m_propertiesToggle, 0, wxEXPAND);
    m_dockBarPanel->SetSizerAndFit(dockBarSizer);

    m_auiManager.AddPane(m_dockBarPanel, wxAuiPaneInfo()
        .Name("dockbar")
        .Left()
        .Layer(2)
        .Row(0)
        .Position(0)
        .CaptionVisible(false)
        .CloseButton(false)
        .MaximizeButton(false)
        .PinButton(false)
        .Gripper(false)
        .PaneBorder(false)
        .Dockable(false)
        .Floatable(false)
        .Movable(false)
        .Resizable(false)
        .DockFixed(true));

    m_auiManager.Update();

    Bind(wxEVT_TOOL, &MainFrame::OnExit, this, ID_Exit);
    Bind(wxEVT_TOOL, &MainFrame::OnAbout, this, ID_About);
    Bind(wxEVT_TOOL, &MainFrame::OnPreferences, this, ID_Preferences);
    Bind(wxEVT_TOOL, &MainFrame::OnNewDropdown, this, ID_New);
    Bind(wxEVT_MENU, &MainFrame::OnNewLocalShell, this, ID_NewLocalShell);
    Bind(wxEVT_MENU, &MainFrame::OnNewSshSession, this, ID_NewSshSession);
    Bind(wxEVT_MENU, &MainFrame::OnNewSftpSession, this, ID_NewSftpSession);
    Bind(wxEVT_MENU, &MainFrame::OnNewFtpSession, this, ID_NewFtpSession);
    Bind(wxEVT_MENU, &MainFrame::OnNewReverseShell, this, ID_NewReverseShell);
    Bind(wxEVT_TOOL, &MainFrame::OnUpgradeTty, this, ID_UpgradeTty);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateUiUpgradeTty, this, ID_UpgradeTty);
    Bind(wxEVT_TREE_SEL_CHANGED, &MainFrame::OnTreeSelectionChanged, this, m_tree->GetId());
    Bind(wxEVT_TREE_ITEM_ACTIVATED, &MainFrame::OnTreeItemActivated, this, m_tree->GetId());
    m_tree->Bind(wxEVT_CONTEXT_MENU, &MainFrame::OnTreeContextMenu, this);
    Bind(wxEVT_MENU, &MainFrame::OnTreeAddGroup, this, ID_TreeAddGroup);
    Bind(wxEVT_MENU, &MainFrame::OnTreeRenameGroup, this, ID_TreeRenameGroup);
    Bind(wxEVT_MENU, &MainFrame::OnTreeDeleteGroup, this, ID_TreeDeleteGroup);
    Bind(wxEVT_MENU, &MainFrame::OnTreeAddConnection, this, ID_TreeAddConnection);
    Bind(wxEVT_MENU, &MainFrame::OnTreeEditConnection, this, ID_TreeEditConnection);
    Bind(wxEVT_MENU, &MainFrame::OnTreeDeleteConnection, this, ID_TreeDeleteConnection);
    Bind(wxEVT_MENU, &MainFrame::OnTreeConnect, this, ID_TreeConnect);
    m_propGrid->Bind(wxEVT_PG_CHANGED, &MainFrame::OnPropertyGridChanged, this);
    Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnToggleTree, this, ID_ToggleTree);
    Bind(wxEVT_TOGGLEBUTTON, &MainFrame::OnToggleProperties, this, ID_ToggleProperties);
    m_auiManager.Bind(wxEVT_AUI_PANE_CLOSE, &MainFrame::OnPaneClose, this);
    m_notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &MainFrame::OnNotebookPageClose, this);
    Bind(wxEVT_MENU, &MainFrame::OnNextTab, this, ID_NextTab);
    Bind(wxEVT_MENU, &MainFrame::OnPreviousTab, this, ID_PreviousTab);
    ApplyShortcuts();
}

MainFrame::~MainFrame()
{
    m_auiManager.UnInit();
}

void MainFrame::OnExit(wxCommandEvent&)
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent&)
{
    wxAboutDialogInfo info;
    info.SetName("termx");
    wxIcon aboutIcon;
    aboutIcon.CopyFromBitmap(LoadEmbeddedBitmap(kAppIconPng, kAppIconPngLen, wxSize(64, 64)));
    info.SetIcon(aboutIcon);
    info.SetDescription(
        "A dockable, multi-pane terminal emulator for Linux, built on "
        "wxWidgets and VTE.\n\n"
        "Includes a color-coded SSH/SFTP/FTP connection manager backed by "
        "an encrypted, password-protected vault (AES-256-GCM with "
        "PBKDF2-HMAC-SHA256 key derivation), split panes, and a "
        "customizable toolbar.");
    info.SetLicense("MIT License");
    info.AddDeveloper("Claude AI");
    info.AddDeveloper("an unknown developer");
    wxAboutBox(info, this);
}

void MainFrame::OnPreferences(wxCommandEvent&)
{
    PreferencesDialog dialog(this, m_terminalSettings);

    if (dialog.ShowModal() == wxID_OK)
    {
        m_terminalSettings = dialog.GetSettings();
        m_terminalSettings.Save();

        for (size_t i = 0; i < m_notebook->GetPageCount(); ++i)
        {
            std::vector<TerminalPanel*> terminals;
            CollectTerminals(m_notebook->GetPage(i), terminals);
            for (TerminalPanel* terminal : terminals)
                terminal->ApplySettings(m_terminalSettings);
        }

        BuildToolBar();
        ApplyShortcuts();
    }
}

void MainFrame::OnNextTab(wxCommandEvent&)
{
    if (!m_notebook || m_notebook->GetPageCount() < 2)
        return;

    const size_t count = m_notebook->GetPageCount();
    const size_t next = (static_cast<size_t>(m_notebook->GetSelection()) + 1) % count;
    m_notebook->SetSelection(next);
}

void MainFrame::OnPreviousTab(wxCommandEvent&)
{
    if (!m_notebook || m_notebook->GetPageCount() < 2)
        return;

    const size_t count = m_notebook->GetPageCount();
    const size_t selection = static_cast<size_t>(m_notebook->GetSelection());
    const size_t previous = (selection == 0) ? count - 1 : selection - 1;
    m_notebook->SetSelection(previous);
}

void MainFrame::ApplyShortcuts()
{
    wxAcceleratorEntry entries[2];
    entries[0].Set(m_terminalSettings.nextTabAccelFlags, m_terminalSettings.nextTabAccelKeyCode,
                   ID_NextTab);
    entries[1].Set(m_terminalSettings.prevTabAccelFlags, m_terminalSettings.prevTabAccelKeyCode,
                   ID_PreviousTab);
    SetAcceleratorTable(wxAcceleratorTable(2, entries));
}

void MainFrame::BuildToolBar()
{
    if (m_toolBar)
    {
        SetToolBar(nullptr);
        m_toolBar->Destroy();
    }

    long style = wxTB_HORIZONTAL | wxTB_FLAT;
    if (m_terminalSettings.toolbarShowLabels)
        style |= wxTB_TEXT;

    m_toolBar = CreateToolBar(style);

    const wxSize iconSize = m_terminalSettings.toolbarLargeIcons ? wxSize(32, 32) : wxSize(16, 16);
    m_toolBar->SetToolBitmapSize(iconSize);

    // Terminal/shell actions first, then app-level actions (preferences,
    // about), then exit isolated at the end behind its own separator.
    m_toolBar->AddTool(ID_New, "New",
                       LoadEmbeddedBitmap(kNewTerminalIconPng, kNewTerminalIconPngLen, iconSize),
                       "New terminal (local shell, SSH session, or reverse shell listener)");
    m_toolBar->AddTool(ID_UpgradeTty, "Upgrade to Full TTY",
                       LoadEmbeddedBitmap(kTerminalIconPng, kTerminalIconPngLen, iconSize),
                       "Upgrade the current reverse shell to a full interactive TTY");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_Preferences, "Preferences",
                       LoadEmbeddedBitmap(kGearIconPng, kGearIconPngLen, iconSize),
                       "Terminal preferences");
    m_toolBar->AddTool(ID_About, "About",
                       LoadEmbeddedBitmap(kAboutIconPng, kAboutIconPngLen, iconSize),
                       "Show about dialog");
    m_toolBar->AddSeparator();
    m_toolBar->AddTool(ID_Exit, "Exit",
                       LoadEmbeddedBitmap(kExitIconPng, kExitIconPngLen, iconSize),
                       "Quit termx");
    m_toolBar->Realize();
}

void MainFrame::OnNewDropdown(wxCommandEvent&)
{
    wxMenu menu;
    menu.Append(ID_NewLocalShell, "Local Shell");
    menu.Append(ID_NewSshSession, "SSH Session...");
    menu.Append(ID_NewSftpSession, "SFTP Session...");
    menu.Append(ID_NewFtpSession, "FTP Session...");
    menu.Append(ID_NewReverseShell, "Reverse Shell...");

    PopupMenu(&menu);
}

void MainFrame::OnNewLocalShell(wxCommandEvent&)
{
    AddTerminalTab("Local Shell");
}

void MainFrame::OnNewSshSession(wxCommandEvent&)
{
    ConnectionDialog dialog(this, nullptr, ConnectionProtocol::Ssh);
    if (dialog.ShowModal() == wxID_OK)
        AddTerminalTab(dialog.GetTabTitle(), dialog.GetArgv(), TerminalKind::Ssh);
}

void MainFrame::OnNewSftpSession(wxCommandEvent&)
{
    ConnectionDialog dialog(this, nullptr, ConnectionProtocol::Sftp);
    if (dialog.ShowModal() == wxID_OK)
        AddTerminalTab(dialog.GetTabTitle(), dialog.GetArgv(), TerminalKind::Sftp);
}

void MainFrame::OnNewFtpSession(wxCommandEvent&)
{
    ConnectionDialog dialog(this, nullptr, ConnectionProtocol::Ftp);
    if (dialog.ShowModal() == wxID_OK)
        AddTerminalTab(dialog.GetTabTitle(), dialog.GetArgv(), TerminalKind::Ftp);
}

void MainFrame::OnNewReverseShell(wxCommandEvent&)
{
    ReverseShellDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK)
        AddTerminalTab(dialog.GetTabTitle(), dialog.GetArgv(), TerminalKind::ReverseShell);
}

void MainFrame::OnUpgradeTty(wxCommandEvent&)
{
    if (auto* terminal = CurrentTerminal())
        terminal->UpgradeToFullTty();
}

void MainFrame::OnUpdateUiUpgradeTty(wxUpdateUIEvent& event)
{
    auto* terminal = CurrentTerminal();
    event.Enable(terminal != nullptr && terminal->GetKind() == TerminalKind::ReverseShell);
}

TerminalPanel* MainFrame::CurrentTerminal() const
{
    const int selection = m_notebook->GetSelection();
    if (selection == wxNOT_FOUND)
        return nullptr;

    wxWindow* page = m_notebook->GetPage(static_cast<size_t>(selection));

    std::vector<TerminalPanel*> terminals;
    CollectTerminals(page, terminals);
    if (terminals.empty())
        return nullptr;

    // Prefer whichever pane currently has keyboard focus, so split panes
    // each act on the one the user is actually looking at/typing into.
    for (wxWindow* focus = wxWindow::FindFocus(); focus; focus = focus->GetParent())
    {
        if (auto* terminal = dynamic_cast<TerminalPanel*>(focus))
            return terminal;
    }

    return terminals.front();
}

void MainFrame::AddTerminalTab(const wxString& title, const wxArrayString& argv, TerminalKind kind,
                               const wxColour& tabColor)
{
    auto* terminal = new TerminalPanel(m_notebook, argv, kind);
    terminal->ApplySettings(m_terminalSettings);

    // Tag the colour, and make sure it's live, before the page is added —
    // both matter. wxAuiNotebook internally clones whatever art provider is
    // current the first time a page is added, and from then on our own
    // m_tabArt is just an orphaned copy: mutating it directly has no effect
    // on what's actually drawn (confirmed empirically — DrawTab always saw
    // an empty colour map despite SetPageColour having "succeeded"), so a
    // fresh SetArtProvider(Clone()) has to be handed back in to actually
    // reach the live instance. And it has to happen before AddPage(), since
    // wxAuiNotebook measures/lays out each tab once, at add time — coloring
    // an already-added tab this same way leaves its geometry stuck at
    // whatever it was originally measured as, producing a squashed tab.
    if (tabColor.IsOk())
    {
        m_tabArt->SetPageColour(terminal, tabColor);
        m_notebook->SetArtProvider(m_tabArt->Clone());
        m_tabArt = static_cast<GroupColorTabArt*>(m_notebook->GetArtProvider());
    }

    m_notebook->AddPage(terminal, title, true /* select */);
}

void MainFrame::OnNotebookPageClose(wxAuiNotebookEvent& event)
{
    // Stop tracking this page's colour once it closes — otherwise the map
    // grows unbounded over a long session, and worse, a future TerminalPanel
    // could be allocated at the same freed address and inherit a stale
    // colour that was never actually set for it.
    const int selection = event.GetSelection();
    if (selection != wxNOT_FOUND)
        m_tabArt->ClearPageColour(m_notebook->GetPage(static_cast<size_t>(selection)));

    event.Skip();
}

void MainFrame::OnTreeSelectionChanged(wxTreeEvent& event)
{
    wxTreeItemId item = event.GetItem();
    wxString label = item.IsOk() ? m_tree->GetItemText(item) : wxString();
    SetStatusText("Selected: " + label);

    auto* data = item.IsOk() ? static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item)) : nullptr;
    if (data && !data->IsGroup())
        PopulatePropertyGridForConnection(data->GroupIndex(), data->ConnectionIndex());
    else
        ClearPropertyGrid();
}

void MainFrame::OnPropertyGridChanged(wxPropertyGridEvent&)
{
    if (m_propGridGroupIndex < 0 || m_propGridConnectionIndex < 0)
        return;

    auto& groups = m_connectionStore.Groups();
    if (static_cast<size_t>(m_propGridGroupIndex) >= groups.size())
        return;

    auto& connections = groups[m_propGridGroupIndex].connections;
    if (static_cast<size_t>(m_propGridConnectionIndex) >= connections.size())
        return;

    ConnectionProfile& profile = connections[m_propGridConnectionIndex];

    profile.protocol = static_cast<ConnectionProtocol>(
        m_propGrid->GetPropertyByName("Protocol")->GetValue().GetInteger());
    profile.name = m_propGrid->GetPropertyByName("Name")->GetValueAsString();
    profile.host = m_propGrid->GetPropertyByName("Host")->GetValueAsString();
    profile.username = m_propGrid->GetPropertyByName("Username")->GetValueAsString();
    profile.port = m_propGrid->GetPropertyByName("Port")->GetValue().GetInteger();
    profile.identityFile = m_propGrid->GetPropertyByName("IdentityFile")->GetValueAsString();
    profile.hostKeyChecking = static_cast<SshHostKeyMode>(
        m_propGrid->GetPropertyByName("HostKeyChecking")->GetValue().GetInteger());
    profile.knownHostsFile = m_propGrid->GetPropertyByName("KnownHostsFile")->GetValueAsString();

    m_connectionStore.Save();

    // Keep the tree label in sync with whatever the user just edited (name
    // and/or protocol both feed into the displayed label — see RebuildTree).
    wxTreeItemId item = m_tree->GetSelection();
    if (item.IsOk())
    {
        wxString label = profile.name;
        if (profile.protocol != ConnectionProtocol::Ssh)
            label += wxString::Format(" (%s)", ConnectionProtocolLabel(profile.protocol));
        m_tree->SetItemText(item, label);
    }
}

void MainFrame::PopulatePropertyGridForConnection(int groupIndex, int connectionIndex)
{
    const ConnectionProfile& profile =
        m_connectionStore.Groups()[groupIndex].connections[connectionIndex];

    m_propGrid->Clear();

    wxArrayString protocols;
    protocols.Add("SSH");
    protocols.Add("SFTP");
    protocols.Add("FTP");
    m_propGrid->Append(new wxEnumProperty("Protocol", "Protocol", protocols, wxArrayInt(),
                                           static_cast<int>(profile.protocol)));

    m_propGrid->Append(new wxStringProperty("Name", "Name", profile.name));
    m_propGrid->Append(new wxStringProperty("Host", "Host", profile.host));
    m_propGrid->Append(new wxStringProperty("Username", "Username", profile.username));
    m_propGrid->Append(new wxIntProperty("Port", "Port", profile.port));
    m_propGrid->Append(new wxFileProperty("Identity File", "IdentityFile", profile.identityFile));

    wxArrayString hostKeyModes;
    hostKeyModes.Add("Ask (default)");
    hostKeyModes.Add("Strict — reject unknown or changed keys");
    hostKeyModes.Add("Accept new keys — still reject changed keys");
    m_propGrid->Append(new wxEnumProperty("Host Key Verification", "HostKeyChecking", hostKeyModes,
                                           wxArrayInt(), static_cast<int>(profile.hostKeyChecking)));

    m_propGrid->Append(new wxFileProperty("Known Hosts File", "KnownHostsFile", profile.knownHostsFile));

    m_propGridGroupIndex = groupIndex;
    m_propGridConnectionIndex = connectionIndex;
}

void MainFrame::ClearPropertyGrid()
{
    m_propGrid->Clear();
    m_propGridGroupIndex = -1;
    m_propGridConnectionIndex = -1;
}

void MainFrame::OnTreeItemActivated(wxTreeEvent& event)
{
    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(event.GetItem()));
    if (data && !data->IsGroup())
    {
        const ConnectionGroup& group = m_connectionStore.Groups()[data->GroupIndex()];
        ConnectProfile(group.connections[data->ConnectionIndex()], group.color);
    }
}

void MainFrame::OnTreeContextMenu(wxContextMenuEvent& event)
{
    wxPoint clientPt = event.GetPosition();
    if (clientPt == wxDefaultPosition)
        clientPt = m_tree->ScreenToClient(wxGetMousePosition());
    else
        clientPt = m_tree->ScreenToClient(clientPt);

    int flags = 0;
    wxTreeItemId item = m_tree->HitTest(clientPt, flags);

    wxMenu menu;
    if (item.IsOk() && (flags & wxTREE_HITTEST_ONITEM))
    {
        m_tree->SelectItem(item);
        auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
        if (data->IsGroup())
        {
            menu.Append(ID_TreeAddConnection, "New Connection...");
            menu.AppendSeparator();
            menu.Append(ID_TreeRenameGroup, "Edit Group...");
            menu.Append(ID_TreeDeleteGroup, "Delete Group");
        }
        else
        {
            menu.Append(ID_TreeConnect, "Connect");
            menu.AppendSeparator();
            menu.Append(ID_TreeEditConnection, "Edit Connection...");
            menu.Append(ID_TreeDeleteConnection, "Delete Connection");
        }
    }
    else
    {
        menu.Append(ID_TreeAddGroup, "New Connection Group...");
    }

    m_tree->PopupMenu(&menu, clientPt);
}

void MainFrame::OnTreeAddGroup(wxCommandEvent&)
{
    GroupDialog dlg(this, "New Connection Group", wxString(), wxColour());
    if (dlg.ShowModal() != wxID_OK)
        return;

    ConnectionGroup group;
    group.name = dlg.GetGroupName();
    group.color = dlg.GetGroupColor();
    m_connectionStore.Groups().push_back(group);
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeRenameGroup(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    ConnectionGroup& group = m_connectionStore.Groups()[data->GroupIndex()];

    GroupDialog dlg(this, "Edit Connection Group", group.name, group.color);
    if (dlg.ShowModal() != wxID_OK)
        return;

    group.name = dlg.GetGroupName();
    group.color = dlg.GetGroupColor();
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeDeleteGroup(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    ConnectionGroup& group = m_connectionStore.Groups()[data->GroupIndex()];

    const int confirm = wxMessageBox(
        wxString::Format("Delete group \"%s\" and all its connections?", group.name),
        "Delete Group", wxYES_NO | wxICON_WARNING, this);
    if (confirm != wxYES)
        return;

    m_connectionStore.Groups().erase(m_connectionStore.Groups().begin() + data->GroupIndex());
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeAddConnection(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    const int groupIndex = data->GroupIndex();

    ConnectionDialog dialog(this);
    if (dialog.ShowModal() != wxID_OK)
        return;

    m_connectionStore.Groups()[groupIndex].connections.push_back(dialog.GetProfile());
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeEditConnection(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    ConnectionProfile& profile =
        m_connectionStore.Groups()[data->GroupIndex()].connections[data->ConnectionIndex()];

    ConnectionDialog dialog(this, &profile);
    if (dialog.ShowModal() != wxID_OK)
        return;

    profile = dialog.GetProfile();
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeDeleteConnection(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    auto& connections = m_connectionStore.Groups()[data->GroupIndex()].connections;
    connections.erase(connections.begin() + data->ConnectionIndex());
    m_connectionStore.Save();
    RebuildTree();
}

void MainFrame::OnTreeConnect(wxCommandEvent&)
{
    wxTreeItemId item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    auto* data = static_cast<ConnectionTreeItemData*>(m_tree->GetItemData(item));
    if (!data->IsGroup())
    {
        const ConnectionGroup& group = m_connectionStore.Groups()[data->GroupIndex()];
        ConnectProfile(group.connections[data->ConnectionIndex()], group.color);
    }
}

void MainFrame::ConnectProfile(const ConnectionProfile& profile, const wxColour& groupColor)
{
    TerminalKind kind = TerminalKind::Ssh;
    switch (profile.protocol)
    {
        case ConnectionProtocol::Sftp: kind = TerminalKind::Sftp; break;
        case ConnectionProtocol::Ftp: kind = TerminalKind::Ftp; break;
        default: break; // Ssh
    }

    AddTerminalTab(profile.name, profile.BuildArgv(), kind, groupColor);
}

void MainFrame::OnToggleTree(wxCommandEvent&)
{
    m_auiManager.GetPane(m_tree).Show(m_treeToggle->IsToggled());
    m_auiManager.Update();
}

void MainFrame::OnToggleProperties(wxCommandEvent&)
{
    m_auiManager.GetPane(m_propGrid).Show(m_propertiesToggle->IsToggled());
    m_auiManager.Update();
}

void MainFrame::OnPaneClose(wxAuiManagerEvent& event)
{
    // Keep the dock bar's toggle state in sync when a pane is closed via its
    // own caption's close button, not just via the dock bar itself.
    wxAuiPaneInfo* pane = event.GetPane();
    if (pane)
    {
        if (pane->window == m_tree)
            m_treeToggle->SetToggled(false);
        else if (pane->window == m_propGrid)
            m_propertiesToggle->SetToggled(false);
    }

    event.Skip();
}

void MainFrame::RebuildTree()
{
    ClearPropertyGrid();
    m_tree->DeleteAllItems();
    m_treeRoot = m_tree->AddRoot("Root");

    const auto& groups = m_connectionStore.Groups();
    for (size_t g = 0; g < groups.size(); ++g)
    {
        wxTreeItemId groupItem = m_tree->AppendItem(m_treeRoot, groups[g].name);
        m_tree->SetItemData(groupItem, new ConnectionTreeItemData(static_cast<int>(g)));

        const auto& connections = groups[g].connections;
        for (size_t c = 0; c < connections.size(); ++c)
        {
            const ConnectionProfile& profile = connections[c];
            wxString label = profile.name;
            if (profile.protocol != ConnectionProtocol::Ssh)
                label += wxString::Format(" (%s)", ConnectionProtocolLabel(profile.protocol));

            wxTreeItemId connectionItem = m_tree->AppendItem(groupItem, label);
            m_tree->SetItemData(connectionItem,
                                 new ConnectionTreeItemData(static_cast<int>(g), static_cast<int>(c)));
        }

        m_tree->Expand(groupItem);
    }
}
