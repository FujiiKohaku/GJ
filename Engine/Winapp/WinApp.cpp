#include "WinApp.h"
#include "imgui/imgui.h" // ImGui 本体
#include "imgui/imgui_impl_dx12.h" // DirectX12 連携
#include "imgui/imgui_impl_win32.h" // Win32 連携
#include <Windows.h>
#include <cstdint>


std::unique_ptr<WinApp> WinApp::instance_ = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
}


WinApp* WinApp::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<WinApp>(ConstructorKey());
    }

    return instance_.get();
}

void WinApp::FinalizeInstance()
{
    if (!instance_) {
        return;
    }

    instance_->Finalize();
    instance_.reset();
}






//==================================================================
//  ウィンドウプロシージャ
//  Windowsからのメッセージを処理する
//==================================================================
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // WM_CHAR reflects the active keyboard layout and IME, so runtime text
    // fields can receive Japanese as UTF-16 before it is converted to UTF-8.
    if (msg == WM_CHAR && wparam >= 0x20 && wparam != 0x7f && instance_) {
        instance_->textInput_.push_back(static_cast<wchar_t>(wparam));
    }

    // ImGui用メッセージ処理（優先）
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    // メッセージに応じた固有処理
    switch (msg) {
    case WM_DESTROY: // ウィンドウが破棄された
        PostQuitMessage(0); // OSにアプリ終了を通知
        return 0;
    }

    // 標準のメッセージ処理を実行
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

std::string WinApp::ConsumeTextInput()
{
    if (textInput_.empty()) return {};
    std::wstring input;
    input.swap(textInput_);
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
        result.data(), length, nullptr, nullptr);
    return result;
}

//==================================================================
//  メッセージ処理関数
//  戻り値 true: 終了メッセージを受け取った
//==================================================================
bool WinApp::ProcessMessage()
{
    MSG msg {};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 終了メッセージを検出
    if (msg.message == WM_QUIT) {
        return true;
    }

    return false;
}

int32_t WinApp::GetClientWidth() const
{
    if (hwnd_ == nullptr) {
        return kClientWidth;
    }

    RECT clientRect {};
    GetClientRect(hwnd_, &clientRect);
    return clientRect.right - clientRect.left;
}

int32_t WinApp::GetClientHeight() const
{
    if (hwnd_ == nullptr) {
        return kClientHeight;
    }

    RECT clientRect {};
    GetClientRect(hwnd_, &clientRect);
    return clientRect.bottom - clientRect.top;
}

bool WinApp::ToggleFullscreen()
{
    if (hwnd_ == nullptr) {
        return false;
    }

    if (!isFullscreen_) {
        windowedStyle_ = static_cast<DWORD>(GetWindowLongPtr(hwnd_, GWL_STYLE));
        windowedExStyle_ = static_cast<DWORD>(GetWindowLongPtr(hwnd_, GWL_EXSTYLE));
        windowedPlacement_.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(hwnd_, &windowedPlacement_)) {
            return false;
        }

        MONITORINFO monitorInfo { sizeof(MONITORINFO) };
        if (!GetMonitorInfo(
                MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                &monitorInfo)) {
            return false;
        }

        SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtr(
            hwnd_, GWL_EXSTYLE,
            windowedExStyle_ &
                ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME));
        if (!SetWindowPos(
                hwnd_, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER)) {
            return false;
        }
        isFullscreen_ = true;
        return true;
    }

    SetWindowLongPtr(hwnd_, GWL_STYLE, windowedStyle_);
    SetWindowLongPtr(hwnd_, GWL_EXSTYLE, windowedExStyle_);
    if (!SetWindowPlacement(hwnd_, &windowedPlacement_)) {
        return false;
    }
    SetWindowPos(
        hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
        SWP_NOZORDER | SWP_NOOWNERZORDER);
    isFullscreen_ = false;
    return true;
}

//==================================================================
//  初期化処理
//  ウィンドウクラス登録・生成・表示
//==================================================================
void WinApp::initialize()
{
    // DPI
    SetProcessDPIAware();

    // COMライブラリ初期化（マルチスレッド対応）
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // ウィンドウクラス設定
    wc_.lpfnWndProc = WindowProc; // ウィンドウプロシージャ
    wc_.lpszClassName = L"CG2WindowClass"; // クラス名
    wc_.hInstance = GetModuleHandle(nullptr); // インスタンスハンドル
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソル設定

    // ウィンドウクラスを登録
    wc_.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClass(&wc_);

    // クライアント領域を元にウィンドウサイズを調整
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&wrc, kWindowStyle, false);

    // ウィンドウ生成
    hwnd_ = CreateWindow(
        wc_.lpszClassName, // クラス名
        L"3029_死因：最適解", // タイトル
        kWindowStyle, // スタイル
        CW_USEDEFAULT, CW_USEDEFAULT, // 位置（自動）
        wrc.right - wrc.left, // 幅
        wrc.bottom - wrc.top, // 高さ
        nullptr, nullptr, wc_.hInstance, nullptr);

}

void WinApp::Show()
{
    // ウィンドウ表示
    ShowWindow(hwnd_, SW_SHOW);
}

//==================================================================
//  終了処理
//  ウィンドウ破棄とCOM解放
//==================================================================
void WinApp::Finalize()
{
    ClipCursor(nullptr);
    CloseWindow(hwnd_);
    CoUninitialize();
}
