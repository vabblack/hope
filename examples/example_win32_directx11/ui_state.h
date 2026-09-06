#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <shlobj.h>

// Legacy focus state kept for binary & subsystem compatibility (e.g. Agent)
enum class FocusState { None, Username, Password, Chat, BrowserUrl, BrowserPage, BrowserType };

extern FocusState g_currentFocus;
extern std::string g_chatBuffer;

// ============================================================================
// STEP 2: NORMALIZED APPLICATION STATE MODEL
// ============================================================================

// High-level interaction state: Is the user currently engaging Hope?
enum class InteractionState {
    Inactive,  // Hope is passive/idle; host applications retain input priority
    Active     // Hope is actively engaged by the user (via deliberate hotkey or focus)
};

// Input modality: How is the user interacting with Hope?
enum class InputMode {
    Mouse,     // Interacting via mouse clicks/hover
    Keyboard   // Navigating via keyboard (Tab/Arrows/Enter/Esc)
};

// Window role in multi-region / dual-mode setups
enum class WindowRole {
    Primary,   // Main Hope window (standard mode)
    Secondary, // Dual HUD overlay / collapsed pill
    Host       // External target window (Window A, e.g. browser)
};

// Region / panel under interaction
enum class ActivePanel {
    None,
    Primary,   // Main Hope window / normal mode UI
    Secondary, // Dual mode HUD / floating pill
    Browser    // Embedded WebView2 viewport
};

// Logical UI focus target inside Hope
enum class LogicalFocusTarget {
    None,
    ChatInput,
    UsernameInput,
    PasswordInput,
    BrowserUrl,
    BrowserContent,
    BrowserType,
    DualActionAutoType,
    DualActionCodeToggle,
    DualActionCopy,
    DualActionCollapse,
    DualActionClose,
    NavPills,
    NavChat,
    NavBrowser,
    NavInterview,
    NavAgent,
    NavDual,
    SettingsButton,
    CloseButton
};

// ============================================================================
// STEP 2 & 7: HOPE STATE MANAGER WITH IDEMPOTENT EVENT NORMALIZATION
// ============================================================================

class HopeStateManager {
private:
    std::mutex m_mutex;
    InteractionState m_interactionState = InteractionState::Inactive;
    InputMode m_inputMode = InputMode::Mouse;
    ActivePanel m_activePanel = ActivePanel::Primary;
    LogicalFocusTarget m_logicalFocus = LogicalFocusTarget::None;
    WindowRole m_currentWindowRole = WindowRole::Primary;
    std::string m_appModeStr = "Chat";

    bool m_logInitialized = false;
    std::string m_logFilePath;

    HopeStateManager() {
        EnsureLogPath();
    }

    void EnsureLogPath() {
        if (!m_logInitialized) {
            char path[MAX_PATH] = { 0 };
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
                std::string dir = std::string(path) + "\\ofradr";
                CreateDirectoryA(dir.c_str(), NULL);
                m_logFilePath = dir + "\\state_transitions.log";
                m_logInitialized = true;
            }
        }
    }

    std::string GetCurrentTimestampStr() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &t);
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << tm.tm_hour << ":"
            << std::setw(2) << tm.tm_min << ":"
            << std::setw(2) << tm.tm_sec << "."
            << std::setw(3) << ms.count();
        return oss.str();
    }

    const char* StateToStr(InteractionState s) {
        return (s == InteractionState::Active) ? "Active" : "Inactive";
    }

    const char* ModeToStr(InputMode m) {
        return (m == InputMode::Keyboard) ? "Keyboard" : "Mouse";
    }

    const char* WindowRoleToStr(WindowRole r) {
        switch (r) {
            case WindowRole::Primary: return "PRIMARY";
            case WindowRole::Secondary: return "SECONDARY";
            case WindowRole::Host: return "HOST";
            default: return "UNKNOWN";
        }
    }

    const char* PanelToStr(ActivePanel p) {
        switch (p) {
            case ActivePanel::Primary: return "Primary";
            case ActivePanel::Secondary: return "Secondary";
            case ActivePanel::Browser: return "Browser";
            default: return "None";
        }
    }

    const char* TargetToStr(LogicalFocusTarget t) {
        switch (t) {
            case LogicalFocusTarget::ChatInput: return "ChatInput";
            case LogicalFocusTarget::UsernameInput: return "UsernameInput";
            case LogicalFocusTarget::PasswordInput: return "PasswordInput";
            case LogicalFocusTarget::BrowserUrl: return "BrowserUrl";
            case LogicalFocusTarget::BrowserContent: return "BrowserContent";
            case LogicalFocusTarget::BrowserType: return "BrowserType";
            case LogicalFocusTarget::DualActionAutoType: return "DualActionAutoType";
            case LogicalFocusTarget::DualActionCodeToggle: return "DualActionCodeToggle";
            case LogicalFocusTarget::DualActionCopy: return "DualActionCopy";
            case LogicalFocusTarget::DualActionCollapse: return "DualActionCollapse";
            case LogicalFocusTarget::DualActionClose: return "DualActionClose";
            case LogicalFocusTarget::NavPills: return "NavPills";
            case LogicalFocusTarget::NavChat: return "NavChat";
            case LogicalFocusTarget::NavBrowser: return "NavBrowser";
            case LogicalFocusTarget::NavInterview: return "NavInterview";
            case LogicalFocusTarget::NavAgent: return "NavAgent";
            case LogicalFocusTarget::NavDual: return "NavDual";
            case LogicalFocusTarget::SettingsButton: return "SettingsButton";
            case LogicalFocusTarget::CloseButton: return "CloseButton";
            default: return "None";
        }
    }

public:
    static HopeStateManager& Get() {
        static HopeStateManager instance;
        return instance;
    }

    // STEP 8: Structured Debug Logging conforming to specification:
    // [TIMESTAMP]
    // EVENT=...
    // WINDOW=...
    // STATE=...
    // ACTION=...
    // REASON=...
    void LogTransition(const char* eventSource, const char* eventType, const char* windowRole,
                       const char* prevState, const char* newState,
                       const char* currentMode, const char* currentPanel,
                       const char* action, const char* reason) {
        std::string ts = GetCurrentTimestampStr();
        std::ostringstream oss;
        oss << "[" << ts << "]\n"
            << "EVENT=" << (eventType ? eventType : "UNKNOWN") << "\n"
            << "WINDOW=" << (windowRole ? windowRole : "PRIMARY") << "\n"
            << "STATE=" << (newState ? newState : "Inactive") << "\n"
            << "ACTION=" << (action ? action : "NoStateChange") << "\n"
            << "REASON=" << (reason ? reason : "NormalOperation") << "\n\n";

        std::string logEntry = oss.str();
        OutputDebugStringA(logEntry.c_str());

        EnsureLogPath();
        if (m_logInitialized && !m_logFilePath.empty()) {
            FILE* f = nullptr;
            fopen_s(&f, m_logFilePath.c_str(), "a");
            if (f) {
                fputs(logEntry.c_str(), f);
                fclose(f);
            }
        }
    }

    // Idempotent state setter
    bool SetInteractionState(InteractionState newState, const char* reason = nullptr, const char* eventSource = nullptr, HWND hwnd = NULL) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const char* prevStr = StateToStr(m_interactionState);
        const char* newStr = StateToStr(newState);
        const char* winRole = WindowRoleToStr(m_currentWindowRole);
        const char* panelStr = PanelToStr(m_activePanel);

        if (m_interactionState == newState) {
            // Guard: Idempotent - no repeated state churn
            LogTransition(eventSource ? eventSource : "SetInteractionState",
                          "InteractionStateChange", winRole, prevStr, newStr,
                          m_appModeStr.c_str(), panelStr,
                          "NoStateChange", reason ? reason : "AlreadyInTargetState");
            return false;
        }

        m_interactionState = newState;
        std::string actionStr = std::string("StateTransition (") + prevStr + " -> " + newStr + ")";
        LogTransition(eventSource ? eventSource : "SetInteractionState",
                      "InteractionStateChange", winRole, prevStr, newStr,
                      m_appModeStr.c_str(), panelStr,
                      actionStr.c_str(), reason ? reason : "ExplicitRequest");
        return true;
    }

    bool SetInputMode(InputMode newMode, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_inputMode == newMode) return false;
        m_inputMode = newMode;
        LogTransition("SetInputMode", "InputModeChange", WindowRoleToStr(m_currentWindowRole),
                      StateToStr(m_interactionState), StateToStr(m_interactionState),
                      m_appModeStr.c_str(), PanelToStr(m_activePanel),
                      ModeToStr(newMode), reason ? reason : "ModalityShift");
        return true;
    }

    bool SetActivePanel(ActivePanel newPanel, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activePanel == newPanel) return false;
        m_activePanel = newPanel;
        LogTransition("SetActivePanel", "ActivePanelChange", WindowRoleToStr(m_currentWindowRole),
                      StateToStr(m_interactionState), StateToStr(m_interactionState),
                      m_appModeStr.c_str(), PanelToStr(newPanel),
                      PanelToStr(newPanel), reason ? reason : "PanelSwitch");
        return true;
    }

    bool SetLogicalFocus(LogicalFocusTarget newTarget, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logicalFocus == newTarget) return false;
        const char* prevTarget = TargetToStr(m_logicalFocus);
        m_logicalFocus = newTarget;
        const char* nextTarget = TargetToStr(newTarget);

        // Keep legacy g_currentFocus in sync
        switch (newTarget) {
            case LogicalFocusTarget::ChatInput: g_currentFocus = FocusState::Chat; break;
            case LogicalFocusTarget::UsernameInput: g_currentFocus = FocusState::Username; break;
            case LogicalFocusTarget::PasswordInput: g_currentFocus = FocusState::Password; break;
            case LogicalFocusTarget::BrowserUrl: g_currentFocus = FocusState::BrowserUrl; break;
            case LogicalFocusTarget::BrowserContent: g_currentFocus = FocusState::BrowserPage; break;
            case LogicalFocusTarget::BrowserType: g_currentFocus = FocusState::BrowserType; break;
            default: g_currentFocus = FocusState::None; break;
        }

        std::string actionStr = std::string("FocusTarget (") + prevTarget + " -> " + nextTarget + ")";
        LogTransition("SetLogicalFocus", "LogicalFocusChange", WindowRoleToStr(m_currentWindowRole),
                      StateToStr(m_interactionState), StateToStr(m_interactionState),
                      m_appModeStr.c_str(), PanelToStr(m_activePanel),
                      actionStr.c_str(), reason ? reason : "NavigationTargetUpdated");
        return true;
    }

    void SetWindowRole(WindowRole role, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentWindowRole == role) return;
        const char* prevRole = WindowRoleToStr(m_currentWindowRole);
        m_currentWindowRole = role;
        const char* nextRole = WindowRoleToStr(role);
        std::string act = std::string("WindowRole (") + prevRole + " -> " + nextRole + ")";
        LogTransition("SetWindowRole", "WindowRoleChange", nextRole,
                      StateToStr(m_interactionState), StateToStr(m_interactionState),
                      m_appModeStr.c_str(), PanelToStr(m_activePanel),
                      act.c_str(), reason ? reason : "RoleUpdate");
    }

    void SetCurrentWindowRole(WindowRole role) {
        SetWindowRole(role);
    }

    void SetAppModeStr(const std::string& modeStr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_appModeStr = modeStr;
    }

    InteractionState GetInteractionState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_interactionState;
    }

    InputMode GetInputMode() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inputMode;
    }

    ActivePanel GetActivePanel() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activePanel;
    }

    LogicalFocusTarget GetLogicalFocus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_logicalFocus;
    }

    WindowRole GetCurrentWindowRole() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentWindowRole;
    }

    std::string GetAppModeStr() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_appModeStr;
    }

    bool IsActive() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_interactionState == InteractionState::Active;
    }

    bool IsInputSwallowingAllowed() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Only swallow typing keys when a text input is explicitly focused
        return (m_logicalFocus == LogicalFocusTarget::ChatInput ||
                m_logicalFocus == LogicalFocusTarget::UsernameInput ||
                m_logicalFocus == LogicalFocusTarget::PasswordInput ||
                m_logicalFocus == LogicalFocusTarget::BrowserType ||
                m_logicalFocus == LogicalFocusTarget::BrowserUrl);
    }

    // STEP 7: Event Normalization - interprets low-level OS messages into normalized state
    void NormalizeOsEvent(UINT msg, WPARAM wParam, LPARAM lParam, const char* source = "WndProc") {
        NormalizeOsEvent(NULL, msg, wParam, lParam, source);
    }

    void NormalizeOsEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, const char* source = "WndProc") {
        const char* winRole = WindowRoleToStr(m_currentWindowRole);
        const char* panelStr = PanelToStr(m_activePanel);

        switch (msg) {
            case WM_MOUSEMOVE: {
                // Moving pointer across window boundaries does NOT toggle application active state!
                // It only informs input modality that mouse is in use.
                m_inputMode = InputMode::Mouse;
                // No state change logged for routine mouse move to prevent spam,
                // but boundary transitions are cleanly ignored.
                break;
            }
            case WM_MOUSELEAVE: {
                // Pointer left Hope window bounds. This is NOT a deactivation!
                LogTransition(source, "WM_MOUSELEAVE", winRole,
                              StateToStr(m_interactionState), StateToStr(m_interactionState),
                              m_appModeStr.c_str(), panelStr,
                              "NoStateChange", "PointerMovedBetweenHopeRegions");
                break;
            }
            case WM_SETFOCUS: {
                // Legitimate OS focus received
                SetInteractionState(InteractionState::Active, "OsWindowGainedFocus", source, hWnd);
                break;
            }
            case WM_KILLFOCUS: {
                // Legitimate OS focus lost
                SetInteractionState(InteractionState::Inactive, "OsWindowLostFocus", source, hWnd);
                break;
            }
            case WM_ACTIVATE: {
                if (LOWORD(wParam) == WA_INACTIVE) {
                    SetInteractionState(InteractionState::Inactive, "OsActivateInactive", source, hWnd);
                } else {
                    SetInteractionState(InteractionState::Active, "OsActivateActive", source, hWnd);
                }
                break;
            }
            default:
                break;
        }
    }
};
