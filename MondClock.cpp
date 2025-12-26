#include <windows.h>
#include <ctime>
#include <fstream>
#include <string>

// ================= CONFIG =================
static const wchar_t* MUTEX_NAME = L"MondClock_SingleInstance";
static const wchar_t* CONFIG_FILE = L"mondclock.dat";
static const UINT_PTR TIMER_ID = 1;
static const UINT TIMER_INTERVAL = 60000; // 1 minute

// ================= GLOBAL STATE =================
static HWND g_hwnd = nullptr;
static HWND g_panel = nullptr;
static HFONT hFontDay, hFontDate, hFontUI;
static HANDLE g_mutex = nullptr;

static int  g_scale = 3;
static bool g_darkTheme = true;
static bool g_isExiting = false;

// ================= DRAG =================
static bool g_dragging = false;
static POINT g_dragStartMouse = {};
static RECT g_dragStartRect = {};

// ================= INSTANCE =================
static HINSTANCE g_hInst = nullptr;

// ================= DESKTOP ATTACHMENT =================
HWND GetDesktopWorkerW()
{
    HWND progman = FindWindowW(L"Progman", nullptr);
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

    HWND workerw = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        HWND shell_dll = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
        if (shell_dll != nullptr)
        {
            *(HWND*)lParam = FindWindowExW(nullptr, hwnd, L"WorkerW", nullptr);
            return FALSE;
        }
        return TRUE;
        }, (LPARAM)&workerw);

    return workerw;
}

// ================= UTIL =================
std::wstring GetConfigPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    return path + CONFIG_FILE;
}

void SaveSettings()
{
    RECT r;
    GetWindowRect(g_hwnd, &r);
    std::wstring configPath = GetConfigPath();
    std::ofstream f(configPath, std::ios::binary);
    if (f)
    {
        f.write((char*)&r.left, sizeof(int));
        f.write((char*)&r.top, sizeof(int));
        f.write((char*)&g_scale, sizeof(int));
        f.write((char*)&g_darkTheme, sizeof(bool));
    }
}

void LoadSettings(POINT& pos)
{
    pos = { 700, 200 };
    std::wstring configPath = GetConfigPath();
    std::ifstream f(configPath, std::ios::binary);
    if (f)
    {
        f.read((char*)&pos.x, sizeof(int));
        f.read((char*)&pos.y, sizeof(int));
        f.read((char*)&g_scale, sizeof(int));
        f.read((char*)&g_darkTheme, sizeof(bool));

        // Validate scale
        if (g_scale < 2 || g_scale > 4) g_scale = 3;
    }

    // Validate position is on screen
    int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

    if (pos.x < screenLeft - 100 || pos.x > screenLeft + screenWidth - 50)
        pos.x = 700;
    if (pos.y < screenTop - 50 || pos.y > screenTop + screenHeight - 50)
        pos.y = 200;
}

int GetWindowWidth() { return 170 * g_scale; }
int GetWindowHeight() { return 65 * g_scale; }

// Forward declaration
LRESULT CALLBACK ClockWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void RecreateClockWindow()
{
    if (g_hwnd)
    {
        // Save current position
        RECT oldRect;
        GetWindowRect(g_hwnd, &oldRect);
        int posX = oldRect.left;
        int posY = oldRect.top;

        // Kill the timer on old window
        KillTimer(g_hwnd, TIMER_ID);

        // Destroy old window (this will trigger desktop to repaint that area)
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;

        // Small delay to let desktop repaint
        Sleep(50);

        // Get desktop worker again
        HWND desktopWorker = GetDesktopWorkerW();

        // Create new window at same position with new size
        g_hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"MondClockWnd", L"",
            WS_POPUP | WS_CHILD,
            posX, posY, GetWindowWidth(), GetWindowHeight(),
            desktopWorker, nullptr, g_hInst, nullptr
        );

        SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(g_hwnd, SW_SHOW);

        // Restart timer
        SetTimer(g_hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);

        // Force repaint
        InvalidateRect(g_hwnd, nullptr, TRUE);
        UpdateWindow(g_hwnd);
    }
}

void RecreateFonts()
{
    if (hFontDay)  DeleteObject(hFontDay);
    if (hFontDate) DeleteObject(hFontDate);
    if (hFontUI)   DeleteObject(hFontUI);

    hFontDay = CreateFontW(50 * g_scale, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH,
        L"BankGothic Md BT");

    hFontDate = CreateFontW(10 * g_scale, 0, 0, 0, FW_LIGHT,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH,
        L"BankGothic Md BT");

    hFontUI = CreateFontW(14, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH,
        L"BankGothic Md BT");
}

// ================= PANEL =================
void CreateControlPanel();
void DestroyControlPanel();

// Timer ID for checking if user clicked outside panel
static const UINT_PTR PANEL_CHECK_TIMER = 100;

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // Start a timer to periodically check if we should close
        SetTimer(hwnd, PANEL_CHECK_TIMER, 100, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == PANEL_CHECK_TIMER)
        {
            // Check if panel is visible and mouse is pressed outside
            if (IsWindowVisible(hwnd))
            {
                // Check if left mouse button is pressed
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    RECT panelRect;
                    GetWindowRect(hwnd, &panelRect);

                    // If click is outside panel and not on the clock widget
                    if (!PtInRect(&panelRect, pt))
                    {
                        RECT clockRect;
                        GetWindowRect(g_hwnd, &clockRect);
                        if (!PtInRect(&clockRect, pt))
                        {
                            ShowWindow(hwnd, SW_HIDE);
                        }
                    }
                }
            }
        }
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case 1: g_scale = 2; break;  // Small
            case 2: g_scale = 3; break;  // Medium
            case 3: g_scale = 4; break;  // Large
            case 4: g_darkTheme = !g_darkTheme; break;  // Theme
            case 5:  // Exit
                SaveSettings();
                g_isExiting = true;
                DestroyWindow(g_hwnd);
                return 0;
            }
            RecreateFonts();
            RecreateClockWindow();  // Recreate window to fix ghosting
            SaveSettings();
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            // Check if focus went to a child button - if so, don't hide
            HWND focusWnd = GetFocus();
            if (focusWnd == nullptr || GetParent(focusWnd) != hwnd)
            {
                ShowWindow(hwnd, SW_HIDE);
            }
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, PANEL_CHECK_TIMER);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CreateControlPanel()
{
    if (g_panel && IsWindow(g_panel)) return;

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PanelProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = L"MondClockPanel";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        classRegistered = true;
    }

    // Get desktop worker to parent the panel (so it stays on desktop only)
    HWND desktopWorker = GetDesktopWorkerW();

    // Panel dimensions: 2 columns x 3 rows of buttons + title bar
    // Button height = 28, spacing = 8, title bar ~25
    // Total client height needed: 10 + 28 + 8 + 28 + 8 + 28 + 10 = 120
    // Total window height with title bar: ~150
    g_panel = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"MondClockPanel",
        L"MondClock Settings",
        WS_POPUP | WS_BORDER | WS_CAPTION | WS_CHILD,
        0, 0, 190, 160,
        desktopWorker, nullptr, g_hInst, nullptr
    );

    // Row 1: Small, Medium
    HWND b1 = CreateWindowW(L"BUTTON", L"Small", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 10, 10, 75, 28, g_panel, (HMENU)1, g_hInst, nullptr);
    HWND b2 = CreateWindowW(L"BUTTON", L"Medium", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 95, 10, 75, 28, g_panel, (HMENU)2, g_hInst, nullptr);

    // Row 2: Large, Theme
    HWND b3 = CreateWindowW(L"BUTTON", L"Large", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 10, 46, 75, 28, g_panel, (HMENU)3, g_hInst, nullptr);
    HWND b4 = CreateWindowW(L"BUTTON", L"Theme", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 95, 46, 75, 28, g_panel, (HMENU)4, g_hInst, nullptr);

    // Row 3: Exit (centered, full width)
    HWND b5 = CreateWindowW(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 10, 82, 160, 28, g_panel, (HMENU)5, g_hInst, nullptr);

    SendMessage(b1, WM_SETFONT, (WPARAM)hFontUI, TRUE);
    SendMessage(b2, WM_SETFONT, (WPARAM)hFontUI, TRUE);
    SendMessage(b3, WM_SETFONT, (WPARAM)hFontUI, TRUE);
    SendMessage(b4, WM_SETFONT, (WPARAM)hFontUI, TRUE);
    SendMessage(b5, WM_SETFONT, (WPARAM)hFontUI, TRUE);
}

void DestroyControlPanel()
{
    if (g_panel)
    {
        DestroyWindow(g_panel);
        g_panel = nullptr;
    }
}

// ================= CLOCK =================
LRESULT CALLBACK ClockWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_RBUTTONDOWN:
    {
        RECT r;
        GetWindowRect(hwnd, &r);

        CreateControlPanel();

        int panelX = r.right + 8;
        int panelY = r.top;

        int screenRight = GetSystemMetrics(SM_CXSCREEN);
        if (panelX + 180 > screenRight)
            panelX = r.left - 180 - 8;

        SetWindowPos(
            g_panel,
            HWND_TOPMOST,
            panelX,
            panelY,
            0, 0,
            SWP_NOSIZE | SWP_SHOWWINDOW
        );
        SetForegroundWindow(g_panel);
        SetFocus(g_panel);
        return 0;
    }

    case WM_LBUTTONDOWN:
        g_dragging = true;
        SetCapture(hwnd);
        GetCursorPos(&g_dragStartMouse);
        GetWindowRect(hwnd, &g_dragStartRect);
        return 0;

    case WM_MOUSEMOVE:
        if (g_dragging)
        {
            POINT p;
            GetCursorPos(&p);
            SetWindowPos(hwnd, nullptr,
                g_dragStartRect.left + (p.x - g_dragStartMouse.x),
                g_dragStartRect.top + (p.y - g_dragStartMouse.y),
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        g_dragging = false;
        ReleaseCapture();
        SaveSettings();
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_darkTheme ? RGB(255, 255, 255) : RGB(16, 16, 16));

        SYSTEMTIME st;
        GetLocalTime(&st);

        wchar_t day[64], date[128];
        GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"dddd", day, 64);
        GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"dd  MMMM,  yyyy.", date, 128);

        RECT rc;
        GetClientRect(hwnd, &rc);

        SelectObject(hdc, hFontDay);
        RECT r1 = { 0,0,rc.right,80 * g_scale };
        DrawTextW(hdc, day, -1, &r1, DT_CENTER);

        SelectObject(hdc, hFontDate);
        RECT r2 = { 0,44 * g_scale,rc.right,60 * g_scale };
        DrawTextW(hdc, date, -1, &r2, DT_CENTER);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_ID)
        {
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        // Only do full cleanup if we're actually exiting
        if (g_isExiting)
        {
            SaveSettings();
            // Clean up fonts
            if (hFontDay) { DeleteObject(hFontDay);  hFontDay = nullptr; }
            if (hFontDate) { DeleteObject(hFontDate); hFontDate = nullptr; }
            if (hFontUI) { DeleteObject(hFontUI);   hFontUI = nullptr; }
            DestroyControlPanel();
            ReleaseMutex(g_mutex);
            CloseHandle(g_mutex);
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================= ENTRY =================
int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    g_mutex = CreateMutex(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    g_hInst = hInst;
    SetProcessDPIAware();

    POINT pos;
    LoadSettings(pos);
    RecreateFonts();

    WNDCLASSW wc{};
    wc.lpfnWndProc = ClockWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MondClockWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND desktopWorker = GetDesktopWorkerW();

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"",
        WS_POPUP | WS_CHILD,
        pos.x, pos.y, GetWindowWidth(), GetWindowHeight(),
        desktopWorker, nullptr, hInst, nullptr
    );

    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hwnd, SW_SHOW);

    // Start timer for date updates (checks every minute)
    SetTimer(g_hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}