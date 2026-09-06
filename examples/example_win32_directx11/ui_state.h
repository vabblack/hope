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
// DECOUPLED APPLICATION STATE MODEL
// ============================================================================

// High-level interaction state: Is the user currently engaging Hope?
// Separated from physical mouse position and mouse hover events.
enum class InteractionState {
    Inactive,  // Hope is passive/idle; host applications retain input priority
    Active     // Hope is actively engaged by the user (via deliberate hotkey, typing, or click)
};

// Window activation state as reported by OS window manager
enum class WindowActivationState {
    Inactive,
    Active
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

// Region / panel under interaction inside Hope
enum class ActivePanel {
    None,
    Primary,   // Main Hope window / normal mode UI
    Secondary, // Dual mode HUD / floating pill
    Browser    // Embedded WebView2 viewport
};

// Mouse-over location: Which physical region is the mouse hovering over?
// Changing MouseRegion does NOT automatically change application active state!
enum class MouseRegion {
    None,
    Host,      // External host window (e.g. browser page)
    Primary,   // Hope main window region
    Secondary  // Hope dual mode HUD / pill overlay
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
// HOPE STATE MANAGER WITH IDEMPOTENT EVENT NORMALIZATION & DECOUPLED STATE
// ============================================================================

class HopeStateManager {
private:
    std::mutex m_mutex;
    InteractionState m_interactionState = InteractionState::Inactive;
    WindowActivationState m_windowActivation = WindowActivationState::Inactive;
    MouseRegion m_mouseRegion = MouseRegion::None;
    ActivePanel m_activePanel = ActivePanel::Primary;
    LogicalFocusTarget m_logicalFocus = LogicalFocusTarget::None;
    WindowRole m_currentWindowRole = WindowRole::Primary;
    InputMode m_inputMode = InputMode::Mouse;
    HWND m_focusedHwnd = NULL;
    bool m_dualMode = false;
    bool m_isVisible = false;
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

    const char* MouseRegionToStr(MouseRegion m) {
        switch (m) {
            case MouseRegion::Host: return "Host";
            case MouseRegion::Primary: return "Primary";
            case MouseRegion::Secondary: return "Secondary";
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

    void LogTransitionInternal(const char* eventSource, const char* eventType, HWND hwnd,
                               const char* prevState, const char* newState,
                               const char* prevRegion, const char* currentRegion,
                               const char* keyboardFocus, const char* mouseRegion,
                               const char* action, const char* reason) {
        std::string ts = GetCurrentTimestampStr();
        std::ostringstream oss;
        oss << "[" << ts << "]\n"
            << "EVENT=" << (eventType ? eventType : "UNKNOWN") << "\n"
            << "HWND=" << WindowRoleToStr(m_currentWindowRole) << " (0x" << std::hex << (uintptr_t)hwnd << std::dec << ")\n"
            << "ApplicationActive=" << (newState ? newState : "Inactive") << " (Previous=" << (prevState ? prevState : "Inactive") << ")\n"
            << "CurrentRegion=" << (currentRegion ? currentRegion : "None") << " (Previous=" << (prevRegion ? prevRegion : "None") << ")\n"
            << "MouseRegion=" << (mouseRegion ? mouseRegion : "None") << "\n"
            << "KeyboardFocus=" << (keyboardFocus ? keyboardFocus : "None") << "\n"
            << "Action=" << (action ? action : "NoStateChange") << "\n"
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

public:
    static HopeStateManager& Get() {
        static HopeStateManager instance;
        return instance;
    }

    // Diagnostic logging helper conforming to specification
    void LogTransition(const char* eventSource, const char* eventType, const char* windowRole,
                       const char* prevState, const char* newState,
                       const char* currentMode, const char* currentPanel,
                       const char* action, const char* reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        LogTransitionInternal(eventSource, eventType, m_focusedHwnd,
                              prevState, newState,
                              currentPanel, currentPanel,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              action, reason);
    }

    // Idempotent application active state setter
    // Crucially: Only called upon deliberate user engagement (hotkeys, text entry, clicks)
    // NEVER automatically toggled by mouse hover!
    bool SetApplicationActive(bool active, const char* reason = nullptr, const char* eventSource = nullptr, HWND hwnd = NULL) {
        return SetInteractionState(active ? InteractionState::Active : InteractionState::Inactive, reason, eventSource, hwnd);
    }

    bool SetInteractionState(InteractionState newState, const char* reason = nullptr, const char* eventSource = nullptr, HWND hwnd = NULL) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const char* prevStr = StateToStr(m_interactionState);
        const char* newStr = StateToStr(newState);
        const char* panelStr = PanelToStr(m_activePanel);

        if (m_interactionState == newState) {
            // Guard: Idempotent - no repeated state churn
            LogTransitionInternal(eventSource ? eventSource : "SetInteractionState",
                                  "InteractionStateChange", hwnd ? hwnd : m_focusedHwnd,
                                  prevStr, newStr, panelStr, panelStr,
                                  TargetToStr(m_logicalFocus), MouseRegionToStr(m_mouseRegion),
                                  "NoStateChange", reason ? reason : "AlreadyInTargetState");
            return false;
        }

        m_interactionState = newState;
        std::string actionStr = std::string("StateTransition (") + prevStr + " -> " + newStr + ")";
        LogTransitionInternal(eventSource ? eventSource : "SetInteractionState",
                              "InteractionStateChange", hwnd ? hwnd : m_focusedHwnd,
                              prevStr, newStr, panelStr, panelStr,
                              TargetToStr(m_logicalFocus), MouseRegionToStr(m_mouseRegion),
                              actionStr.c_str(), reason ? reason : "ExplicitRequest");
        return true;
    }

    // Mouse movement only updates MouseRegion:
    // DOES NOT automatically activate or deactivate Hope!
    bool SetMouseRegion(MouseRegion newRegion, const char* reason = nullptr, const char* eventSource = nullptr, HWND hwnd = NULL) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_mouseRegion == newRegion) return false;

        MouseRegion prevRegion = m_mouseRegion;
        m_mouseRegion = newRegion;
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);

        std::string actStr = std::string("MouseRegion (") + MouseRegionToStr(prevRegion) + " -> " + MouseRegionToStr(newRegion) + ")";
        // Region transition is idempotent and decoupled from application active state
        LogTransitionInternal(eventSource ? eventSource : "SetMouseRegion",
                              "MouseRegionChange", hwnd ? hwnd : m_focusedHwnd,
                              stateStr, stateStr,
                              panelStr, panelStr,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(newRegion),
                              actStr.c_str(),
                              reason ? reason : "PointerPositionChanged");
        return true;
    }

    bool SetInputMode(InputMode newMode, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_inputMode == newMode) return false;
        m_inputMode = newMode;
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);
        LogTransitionInternal("SetInputMode", "InputModeChange", m_focusedHwnd,
                              stateStr, stateStr,
                              panelStr, panelStr,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              ModeToStr(newMode), reason ? reason : "ModalityShift");
        return true;
    }

    // Moving between Hope Region A -> Region B simply updates current region
    // rather than performing deactivate -> activate -> focus -> deactivate -> activate.
    bool SetActivePanel(ActivePanel newPanel, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activePanel == newPanel) return false;

        ActivePanel prev = m_activePanel;
        m_activePanel = newPanel;
        const char* prevPanelStr = PanelToStr(prev);
        const char* newPanelStr = PanelToStr(newPanel);
        const char* stateStr = StateToStr(m_interactionState);

        std::string actionStr = std::string("RegionTransition (") + prevPanelStr + " -> " + newPanelStr + ")";
        LogTransitionInternal("SetActivePanel", "ActivePanelChange", m_focusedHwnd,
                              stateStr, stateStr,
                              prevPanelStr, newPanelStr,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              actionStr.c_str(), reason ? reason : "RegionSwitch");
        return true;
    }

    bool SetCurrentRegion(ActivePanel newPanel, const char* reason = nullptr) {
        return SetActivePanel(newPanel, reason);
    }

    bool SetLogicalFocus(LogicalFocusTarget newTarget, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logicalFocus == newTarget) return false;

        const char* prevTarget = TargetToStr(m_logicalFocus);
        m_logicalFocus = newTarget;
        const char* nextTarget = TargetToStr(newTarget);
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);

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
        LogTransitionInternal("SetLogicalFocus", "LogicalFocusChange", m_focusedHwnd,
                              stateStr, stateStr,
                              panelStr, panelStr,
                              nextTarget,
                              MouseRegionToStr(m_mouseRegion),
                              actionStr.c_str(), reason ? reason : "NavigationTargetUpdated");
        return true;
    }

    void SetWindowRole(WindowRole role, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentWindowRole == role) return;

        const char* prevRole = WindowRoleToStr(m_currentWindowRole);
        m_currentWindowRole = role;
        const char* nextRole = WindowRoleToStr(role);
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);

        std::string act = std::string("WindowRole (") + prevRole + " -> " + nextRole + ")";
        LogTransitionInternal("SetWindowRole", "WindowRoleChange", m_focusedHwnd,
                              stateStr, stateStr,
                              panelStr, panelStr,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              act.c_str(), reason ? reason : "RoleUpdate");
    }

    void SetCurrentWindowRole(WindowRole role) {
        SetWindowRole(role);
    }

    void SetDualMode(bool active, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dualMode == active) return;
        m_dualMode = active;
        m_currentWindowRole = active ? WindowRole::Secondary : WindowRole::Primary;
        m_activePanel = active ? ActivePanel::Secondary : ActivePanel::Primary;
        const char* stateStr = StateToStr(m_interactionState);
        LogTransitionInternal("SetDualMode", "DualModeToggled", m_focusedHwnd,
                              stateStr, stateStr,
                              active ? "Primary" : "Secondary",
                              active ? "Secondary" : "Primary",
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              active ? "EnteredDualMode" : "ExitedDualMode",
                              reason ? reason : "ModeToggle");
    }

    void SetWindowActivation(bool active, HWND hwnd = NULL, const char* reason = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        WindowActivationState s = active ? WindowActivationState::Active : WindowActivationState::Inactive;
        if (m_windowActivation == s && m_focusedHwnd == hwnd) return;
        m_windowActivation = s;
        m_focusedHwnd = active ? hwnd : NULL;
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);
        LogTransitionInternal("SetWindowActivation", "WindowActivationChange", hwnd,
                              stateStr, stateStr,
                              panelStr, panelStr,
                              TargetToStr(m_logicalFocus),
                              MouseRegionToStr(m_mouseRegion),
                              active ? "WindowActivated" : "WindowDeactivated",
                              reason ? reason : "OsActivationChanged");
    }

    void SetVisibility(bool visible) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isVisible = visible;
    }

    void SetAppModeStr(const std::string& modeStr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_appModeStr = modeStr;
    }

    InteractionState GetInteractionState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_interactionState;
    }

    bool IsActive() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_interactionState == InteractionState::Active;
    }

    WindowActivationState GetWindowActivationState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_windowActivation;
    }

    InputMode GetInputMode() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inputMode;
    }

    ActivePanel GetActivePanel() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activePanel;
    }

    ActivePanel GetCurrentRegion() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activePanel;
    }

    MouseRegion GetMouseRegion() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_mouseRegion;
    }

    LogicalFocusTarget GetLogicalFocus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_logicalFocus;
    }

    WindowRole GetCurrentWindowRole() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentWindowRole;
    }

    HWND GetFocusedWindow() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_focusedHwnd;
    }

    bool IsDualMode() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dualMode;
    }

    bool IsVisible() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isVisible;
    }

    std::string GetAppModeStr() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_appModeStr;
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

    // Interprets low-level OS messages into normalized state:
    // Decoupled from automatic application activation/deactivation!
    void NormalizeOsEvent(UINT msg, WPARAM wParam, LPARAM lParam, const char* source = "WndProc") {
        NormalizeOsEvent(NULL, msg, wParam, lParam, source);
    }

    void NormalizeOsEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, const char* source = "WndProc") {
        std::lock_guard<std::mutex> lock(m_mutex);
        const char* panelStr = PanelToStr(m_activePanel);
        const char* stateStr = StateToStr(m_interactionState);

        switch (msg) {
            case WM_MOUSEMOVE: {
                m_inputMode = InputMode::Mouse;
                MouseRegion currentM = (m_currentWindowRole == WindowRole::Secondary) ? MouseRegion::Secondary : MouseRegion::Primary;
                if (m_mouseRegion != currentM) {
                    m_mouseRegion = currentM;
                    LogTransitionInternal(source, "WM_MOUSEMOVE", hWnd,
                                          stateStr, stateStr,
                                          panelStr, panelStr,
                                          TargetToStr(m_logicalFocus),
                                          MouseRegionToStr(currentM),
                                          "UpdateMouseRegionOnly", "CursorEnteredHopeWindow");
                }
                break;
            }
            case WM_MOUSELEAVE: {
                // Pointer left Hope window bounds. This is NOT a deactivation of the application!
                m_mouseRegion = MouseRegion::Host;
                LogTransitionInternal(source, "WM_MOUSELEAVE", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      "UpdateMouseRegionOnly", "CursorMovedToHostWindow");
                break;
            }
            case WM_SETFOCUS: {
                m_focusedHwnd = hWnd;
                m_windowActivation = WindowActivationState::Active;
                LogTransitionInternal(source, "WM_SETFOCUS", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      "WindowGainedOsFocus", "OsWindowGainedFocus");
                break;
            }
            case WM_KILLFOCUS: {
                if (m_focusedHwnd == hWnd) m_focusedHwnd = NULL;
                m_windowActivation = WindowActivationState::Inactive;
                LogTransitionInternal(source, "WM_KILLFOCUS", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      "WindowLostOsFocus", "OsWindowLostFocus");
                break;
            }
            case WM_ACTIVATE: {
                bool isActivating = (LOWORD(wParam) != WA_INACTIVE);
                m_windowActivation = isActivating ? WindowActivationState::Active : WindowActivationState::Inactive;
                LogTransitionInternal(source, "WM_ACTIVATE", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      isActivating ? "OsActivateActive" : "OsActivateInactive",
                                      "OsWindowActivationMessage");
                break;
            }
            case WM_MOUSEACTIVATE: {
                LogTransitionInternal(source, "WM_MOUSEACTIVATE", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      "NoStateChange", "FocuslessOverlaySwallowsMouseActivate");
                break;
            }
            case WM_NCACTIVATE: {
                LogTransitionInternal(source, "WM_NCACTIVATE", hWnd,
                                      stateStr, stateStr,
                                      panelStr, panelStr,
                                      TargetToStr(m_logicalFocus),
                                      MouseRegionToStr(m_mouseRegion),
                                      "NoStateChange", "NonClientActivationIntercepted");
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN: {
                m_inputMode = InputMode::Mouse;
                m_mouseRegion = (m_currentWindowRole == WindowRole::Secondary) ? MouseRegion::Secondary : MouseRegion::Primary;
                if (m_interactionState != InteractionState::Active) {
                    m_interactionState = InteractionState::Active;
                    LogTransitionInternal(source, "WM_LBUTTONDOWN", hWnd,
                                          "Inactive", "Active",
                                          panelStr, panelStr,
                                          TargetToStr(m_logicalFocus),
                                          MouseRegionToStr(m_mouseRegion),
                                          "StateTransition (Inactive -> Active)", "UserClickedHopeUi");
                }
                break;
            }
            default:
                break;
        }
    }
};
