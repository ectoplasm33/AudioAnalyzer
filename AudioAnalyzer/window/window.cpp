#include <iostream>
#include <windows.h>

#include "window.hpp"

static HWND hwnd = nullptr;
static HDC hdc = nullptr;
static HINSTANCE h_instance;

static HDC memDC;
static HBITMAP back_buffer;

static RECT memDC_rect;

static HBITMAP last_buffer;

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



        SelectObject(memDC, last_buffer);
        DeleteObject(back_buffer);
        back_buffer = CreateCompatibleBitmap(hdc, win_x, win_y);

        last_buffer = (HBITMAP)SelectObject(memDC, back_buffer);

        memDC_rect = {0, 0, win_x, win_y};

        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool init_window(const int width, const int height) {
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

    memDC = CreateCompatibleDC(hdc);
    back_buffer = CreateCompatibleBitmap(hdc, win_x, win_y);
    
    last_buffer = (HBITMAP)SelectObject(memDC, back_buffer); // memDC draws to back_buffer

    memDC_rect = {0, 0, win_x, win_y};

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

void render_frame(const float* fft_buffer, const float* audio_buffer, const int buffer_size) {
    // clear back buffer
    FillRect(memDC, &memDC_rect, black_brush);

    int end = std::min<int>(win_x, buffer_size);
    for (int x = 0; x < end; x++) {
        int f_y = (int)((audio_buffer[x] + 0.05f) * win_y);
        int a_y = (int)((fft_buffer[x] + 0.75f) * win_y);
        if (x < buffer_size >> 1) SetPixel(memDC, x, win_y - f_y, (255 << 16) | (90 << 8) | 60);
        SetPixel(memDC, x, win_y - a_y, (85 << 16) | (255 << 8) | 40);
    }

    BitBlt(hdc, 0, 0, win_x, win_y, memDC, 0, 0, SRCCOPY);
}

void cleanup_window() {
    DeleteObject(black_brush);

    SelectObject(memDC, last_buffer);
    DeleteObject(back_buffer);
    DeleteDC(memDC);

    if (hdc && hwnd) ReleaseDC(hwnd, hdc);
    hwnd = nullptr;
    hdc = nullptr;
}