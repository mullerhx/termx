#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/aui/aui.h>
#include <wx/propgrid/propgrid.h>

#include "ConnectionStore.h"
#include "DockBarButton.h"
#include "GroupColorTabArt.h"
#include "TerminalPanel.h"
#include "TerminalSettings.h"

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title, const wxString& vaultPassword);
    ~MainFrame() override;

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);
    void OnNewDropdown(wxCommandEvent& event);
    void OnNewLocalShell(wxCommandEvent& event);
    void OnNewSshSession(wxCommandEvent& event);
    void OnNewSftpSession(wxCommandEvent& event);
    void OnNewFtpSession(wxCommandEvent& event);
    void OnNewReverseShell(wxCommandEvent& event);
    void OnUpgradeTty(wxCommandEvent& event);
    void OnUpdateUiUpgradeTty(wxUpdateUIEvent& event);
    void OnTreeSelectionChanged(wxTreeEvent& event);
    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeContextMenu(wxContextMenuEvent& event);
    void OnPropertyGridChanged(wxPropertyGridEvent& event);
    void OnTreeAddGroup(wxCommandEvent& event);
    void OnTreeRenameGroup(wxCommandEvent& event);
    void OnTreeDeleteGroup(wxCommandEvent& event);
    void OnTreeAddConnection(wxCommandEvent& event);
    void OnTreeEditConnection(wxCommandEvent& event);
    void OnTreeDeleteConnection(wxCommandEvent& event);
    void OnTreeConnect(wxCommandEvent& event);
    void OnToggleTree(wxCommandEvent& event);
    void OnToggleProperties(wxCommandEvent& event);
    void OnPaneClose(wxAuiManagerEvent& event);
    void OnNotebookPageClose(wxAuiNotebookEvent& event);

    TerminalPanel* CurrentTerminal() const;
    void AddTerminalTab(const wxString& title, const wxArrayString& argv = wxArrayString(),
                         TerminalKind kind = TerminalKind::LocalShell,
                         const wxColour& tabColor = wxColour());
    void ConnectProfile(const ConnectionProfile& profile, const wxColour& groupColor = wxColour());
    void RebuildTree();
    void PopulatePropertyGridForConnection(int groupIndex, int connectionIndex);
    void ClearPropertyGrid();
    void BuildToolBar();

    wxToolBar* m_toolBar = nullptr;

    wxAuiManager m_auiManager;

    // Permanent vertical strip of toggle buttons docked left of the tree and
    // properties panes — a "dock bar" for collapsing/expanding them, since
    // wxAuiPaneInfo::MinimizeButton() has no effect on this platform's wxAUI
    // build (its minimize/restore handling isn't compiled in).
    wxPanel* m_dockBarPanel = nullptr;
    DockBarButton* m_treeToggle = nullptr;
    DockBarButton* m_propertiesToggle = nullptr;

    wxTreeCtrl* m_tree = nullptr;
    wxTreeItemId m_treeRoot;
    wxPropertyGrid* m_propGrid = nullptr;
    int m_propGridGroupIndex = -1;
    int m_propGridConnectionIndex = -1;
    wxAuiNotebook* m_notebook = nullptr;
    GroupColorTabArt* m_tabArt = nullptr;

    ConnectionStore m_connectionStore;
    TerminalSettings m_terminalSettings;
};
