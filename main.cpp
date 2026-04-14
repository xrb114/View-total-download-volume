#include <windows.h>
#include <iphlpapi.h>
#include <thread>
#include <string>
#include <mutex>

#pragma comment(lib, "iphlpapi.lib")

HWND hwnd;
DWORD lastIn = 0, lastOut = 0;

std::wstring text = L"Loading...";
std::mutex mtx;

// 获取主网卡
MIB_IFROW GetMainInterface()
{
    PMIB_IFTABLE pIfTable;
    DWORD size = 0;

    GetIfTable(NULL, &size, FALSE);
    pIfTable = (MIB_IFTABLE *)malloc(size);

    GetIfTable(pIfTable, &size, FALSE);

    DWORD maxBytes = 0;
    MIB_IFROW best = {};

    for (DWORD i = 0; i < pIfTable->dwNumEntries; i++)
    {
        MIB_IFROW row = pIfTable->table[i];

        DWORD total = row.dwInOctets + row.dwOutOctets;

        if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL &&
            total > maxBytes)
        {
            maxBytes = total;
            best = row;
        }
    }

    free(pIfTable);
    return best;
}

// 格式化速度
std::wstring formatSpeed(DWORD bytes)
{
    double kb = bytes / 1024.0;
    wchar_t buf[64];

    if (kb < 1024)
        swprintf(buf, 64, L"%.0f KB/s", kb);
    else
        swprintf(buf, 64, L"%.2f MB/s", kb / 1024.0);

    return buf;
}

// 格式化总流量
std::wstring formatBytes(DWORD bytes)
{
    double mb = bytes / 1024.0 / 1024.0;
    wchar_t buf[64];
    swprintf(buf, 64, L"%.2f MB", mb);
    return buf;
}

// 网速线程
void NetThread()
{
    while (true)
    {
        MIB_IFROW row = GetMainInterface();

        DWORD in = row.dwInOctets;
        DWORD out = row.dwOutOctets;

        if (lastIn != 0)
        {
            DWORD down = in - lastIn;
            DWORD up = out - lastOut;

            std::lock_guard<std::mutex> lock(mtx);
            text = L"下载↓ " + formatSpeed(down) +
                   L"\n上传↑ " + formatSpeed(up) +
                   L"\n总量: " + formatBytes(in + out);
        }

        lastIn = in;
        lastOut = out;

        InvalidateRect(hwnd, NULL, TRUE);
        Sleep(1000);
    }
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static POINT offset;

    switch (msg)
    {
    case WM_LBUTTONDOWN:
        offset.x = LOWORD(lParam);
        offset.y = HIWORD(lParam);
        SetCapture(hwnd);
        break;

    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON)
        {
            POINT p;
            GetCursorPos(&p);
            SetWindowPos(hwnd, NULL, p.x - offset.x, p.y - offset.y, 0, 0, SWP_NOSIZE);
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // 清屏
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 0));

        // 安全读取文本
        std::wstring safeText;
        {
            std::lock_guard<std::mutex> lock(mtx);
            safeText = text;
        }

        // 分三行
        size_t p1 = safeText.find(L'\n');
        size_t p2 = safeText.find(L'\n', p1 + 1);

        std::wstring line1 = safeText.substr(0, p1);
        std::wstring line2 = safeText.substr(p1 + 1, p2 - p1 - 1);
        std::wstring line3 = safeText.substr(p2 + 1);

        // 字体
        HFONT hFont = CreateFontW(
            22, 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Consolas");
        SelectObject(hdc, hFont);

        // 绘制三行
        TextOutW(hdc, 20, 15, line1.c_str(), line1.length());
        TextOutW(hdc, 20, 45, line2.c_str(), line2.length());
        TextOutW(hdc, 20, 75, line3.c_str(), line3.length());

        DeleteObject(hFont);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    const wchar_t CLASS_NAME[] = L"NetFloat";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLASS_NAME,
        L"",
        WS_POPUP,
        100, 100, 280, 120,
        NULL, NULL, hInstance, NULL);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    std::thread(NetThread).detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}