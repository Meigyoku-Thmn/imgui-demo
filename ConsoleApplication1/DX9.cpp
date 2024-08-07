#include <imgui.h>
#include <imgui_impl_dx9.h>
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include <Windows.h>
#include <hidusage.h>
#include <vector>

using namespace std;

static LPDIRECT3D9              g_pD3D;
static LPDIRECT3DDEVICE9        g_pd3dDevice;
static UINT                     g_ResizeWidth;
static UINT                     g_ResizeHeight;
static D3DPRESENT_PARAMETERS    g_d3dpp;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static UINT DesiredRenderWidth = 1067;
static UINT DesiredRenderHeight = 600;

static struct {
    int dx = 0;
    int dy = 0;
} mouseInput;
static ImVec2 winPos;
static auto rawInputEnabled = false;
static void EnableRawInput(HWND hwnd) {
    if (rawInputEnabled)
        return;
    rawInputEnabled = true;
    RAWINPUTDEVICE device{
        .usUsagePage = HID_USAGE_PAGE_GENERIC,
        .usUsage = HID_USAGE_GENERIC_MOUSE,
        .hwndTarget = hwnd,
    };
    auto rs = RegisterRawInputDevices(&device, 1, sizeof(device));
    winPos = {};
}
static void DisableRawInput() {
    rawInputEnabled = false;
}
static void HandleRawInput(HRAWINPUT hRawInput) {
    if (!rawInputEnabled)
        return;
    static vector<BYTE> buffer;
    UINT size{};
    auto rs = GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    buffer.reserve(size);
    auto raw = (RAWINPUT*)buffer.data();
    rs = GetRawInputData(hRawInput, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
    if (raw->header.dwType == RIM_TYPEMOUSE) {
        mouseInput.dx = raw->data.mouse.lLastX;
        mouseInput.dy = raw->data.mouse.lLastY;
        winPos.x += mouseInput.dx;
        winPos.y += mouseInput.dy;
        if (winPos.x < 0) winPos.x = 0;
        if (winPos.y < 0) winPos.y = 0;
        if (winPos.x >= g_d3dpp.BackBufferWidth) winPos.x = g_d3dpp.BackBufferWidth - 1;
        if (winPos.y >= g_d3dpp.BackBufferHeight) winPos.y = g_d3dpp.BackBufferHeight - 1;
    }
}

static POINT GetPointerPosition(HWND hwnd) {
    POINT pointerPosition;
    GetCursorPos(&pointerPosition);
    ScreenToClient(hwnd, &pointerPosition);
    return pointerPosition;
}

int useDX9() {
    WNDCLASSEXW wc{
        .cbSize = sizeof(wc),
        .style = CS_CLASSDC,
        .lpfnWndProc = WndProc,
        .hInstance = GetModuleHandleW(NULL),
        .lpszClassName = L"ImGui Example",
    };
    RegisterClassExW(&wc);

    RECT r{ .right = (LONG)DesiredRenderWidth, .bottom = (LONG)DesiredRenderHeight };
    auto rs = AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    auto hwnd = CreateWindowW(
        wc.lpszClassName,                   // Class name
        L"Dear ImGui DirectX9 Example",     // Window name
        WS_OVERLAPPEDWINDOW,                // Style
        100, 100,                           // X, Y
        r.right - r.left, r.bottom - r.top, // Width, Height
        NULL,                               // Parent Window
        NULL,                               // Menu
        wc.hInstance,                       // Module Handle
        NULL                                // WM_CREATE Parameter
    );

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (!io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/tahoma.ttf", 18))
        io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    ImGui_ImplWin32_SetMousePosScale(1, 1);

    auto show_demo_window = true;
    auto show_debug_window = true;
    auto clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    auto done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            ImGui_ImplWin32_SetMousePosScale(
                (float)g_ResizeWidth / g_d3dpp.BackBufferWidth,
                (float)g_ResizeHeight / g_d3dpp.BackBufferHeight
            );
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        if (ImGui::Begin("Hello, world!")) {
            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Debug Window", &show_debug_window);
            static float f = 0.0f;
            static int counter = 0;
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        }
        ImGui::End();

        if (show_debug_window) {
            if (ImGui::Begin("Debug Window", &show_debug_window)) {

                if (ImGui::Button(rawInputEnabled ? "Disable Raw Input" : "Enable Raw Input")) {
                    if (!rawInputEnabled)
                        EnableRawInput(hwnd);
                    else
                        DisableRawInput();
                }
                ImGui::Text("DX = %d, DY = %d", mouseInput.dx, mouseInput.dy);
            }
            ImGui::End();
        }

        auto cursorPos = winPos;
        if (!rawInputEnabled) {
            auto pointerPos = GetPointerPosition(hwnd);
            cursorPos.x = pointerPos.x;
            cursorPos.y = pointerPos.y;
        }
        ImGui::SetNextWindowPos(cursorPos);
        if (ImGui::Begin("Test Window", nullptr, ImGuiWindowFlags_NoMouseInputs)) {
            ImGui::Text("Test Window");
        }
        ImGui::End();

        ImGui::EndFrame();

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        auto clear_col_dx = D3DCOLOR_RGBA(
            (int)(clear_color.x * clear_color.w * 255.0f),
            (int)(clear_color.y * clear_color.w * 255.0f),
            (int)(clear_color.z * clear_color.w * 255.0f),
            (int)(clear_color.w * 255.0f)
        );
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);

        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }

        auto result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

static bool CreateDeviceD3D(HWND hWnd) {
    auto d3d9 = GetModuleHandleW(L"d3d9.dll");
    if (!d3d9)
        d3d9 = LoadLibraryW(L"d3d9.dll");
    if (!d3d9)
        return false;
    auto _Direct3DCreate9 = (decltype(&Direct3DCreate9))GetProcAddress(d3d9, "Direct3DCreate9");
    if (!_Direct3DCreate9)
        return false;

    if ((g_pD3D = _Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    g_d3dpp = {
        .BackBufferWidth = DesiredRenderWidth,
        .BackBufferHeight = DesiredRenderHeight,
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .Windowed = TRUE,
        .EnableAutoDepthStencil = TRUE,
        .AutoDepthStencilFormat = D3DFMT_D16,
        .PresentationInterval = D3DPRESENT_INTERVAL_ONE,
    };
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

static void CleanupDeviceD3D() {
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
    if (g_pD3D) {
        g_pD3D->Release();
        g_pD3D = NULL;
    }
}

static void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    auto hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_INPUT: {
            HandleRawInput((HRAWINPUT)lParam);
            return 0;
        }
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            g_ResizeWidth = (UINT)LOWORD(lParam);
            g_ResizeHeight = (UINT)HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            DisableRawInput();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}