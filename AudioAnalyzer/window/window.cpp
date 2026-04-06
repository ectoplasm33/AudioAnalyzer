#include <iostream>
#include <windows.h>

#include "window.hpp"

static HWND hwnd = nullptr;
static HDC hdc = nullptr;
static HBITMAP backbuffer = nullptr;
static HINSTANCE h_instance;

HBRUSH black_brush = CreateSolidBrush(RGB(0, 0, 0));

static int win_x;
static int win_y;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        win_x = LOWORD(lParam);  // width
        win_y = HIWORD(lParam); // height
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool init_window(int width, int height) {
    h_instance = GetModuleHandleW(nullptr);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = h_instance;
    wc.lpszClassName = L"Audio Analyzer";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClass(&wc)) return false;

    hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Audio Analyzer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, h_instance, nullptr
    );

    if (!hwnd) return false;

    RECT rect;
    GetClientRect(hwnd, &rect);
    win_x = rect.right - rect.left;
    win_y = rect.bottom - rect.top;

    ShowWindow(hwnd, SW_SHOW);

    hdc = GetDC(hwnd);

    return true;
}

bool update_window() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

void render_frame(float* audio_buffer, int buffer_size) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    // clear screen
    FillRect(hdc, &rect, black_brush);

    // simple waveform visualization
    int step = std::max<int>(buffer_size / win_x, 1);
    for (int x = 0; x < win_x; x++) {
        int idx = x * step;
        int y = (int)((audio_buffer[idx] * 0.5f + 0.5f) * win_y);
        SetPixel(hdc, x, win_y - y, (255 << 16) | (90 << 8) | 60);
    }
}

void cleanup_window() {
    DeleteObject(black_brush);

    if (hdc && hwnd) ReleaseDC(hwnd, hdc);
    hwnd = nullptr;
    hdc = nullptr;
}