#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <random>
#include <string>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Forward declaration (intentionally behind #if 0 in imgui_impl_win32.h)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Overlay
{
public:
    HWND Window = nullptr;
    HWND TargetWindow = nullptr;
    int Width = 1920;
    int Height = 1080;

    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11RenderTargetView* RenderTargetView = nullptr;

    bool Running = true;

    // Randomized names (generated once at init)
    std::wstring ClassName;
    std::wstring WindowTitle;

    bool Init(HWND targetWindow)
    {
        TargetWindow = targetWindow;

        RECT rect;
        GetClientRect(TargetWindow, &rect);
        Width = rect.right - rect.left;
        Height = rect.bottom - rect.top;
        if (Width <= 0) Width = 1920;
        if (Height <= 0) Height = 1080;

        // Generate random class/window names to avoid signature detection
        ClassName = GenerateRandomName();
        WindowTitle = GenerateRandomName();

        if (!CreateOverlayWindow()) return false;
        if (!CreateDevice()) return false;
        if (!InitImGui()) return false;

        return true;
    }

    bool CreateOverlayWindow()
    {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = ClassName.c_str();
        RegisterClassEx(&wc);

        // WS_EX_TOOLWINDOW: hide from taskbar
        // WS_EX_TOPMOST: stay above game
        // WS_EX_TRANSPARENT: click-through (toggled at runtime)
        // WS_EX_LAYERED: for transparency
        // NOTE: We avoid WS_EX_NOACTIVATE to reduce the flag combo fingerprint
        Window = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            ClassName.c_str(),
            WindowTitle.c_str(),
            WS_POPUP,
            0, 0, Width, Height,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!Window) return false;

        // Use per-pixel alpha via UpdateLayeredWindow approach
        // But for DX11 rendering we need LWA_COLORKEY with a color key
        SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 0, LWA_COLORKEY);

        // Hide from screen capture / recording / screenshots
        // This also prevents BE from capturing our overlay in screenshots
        SetWindowDisplayAffinity(Window, WDA_EXCLUDEFROMCAPTURE);

        ShowWindow(Window, SW_SHOWNOACTIVATE);
        UpdateWindow(Window);

        // Position over target
        RECT targetRect;
        GetWindowRect(TargetWindow, &targetRect);
        SetWindowPos(Window, HWND_TOPMOST, targetRect.left, targetRect.top, Width, Height, SWP_NOACTIVATE);

        return true;
    }

    bool CreateDevice()
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = Width;
        sd.BufferDesc.Height = Height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 0;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = Window;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, 1, D3D11_SDK_VERSION,
            &sd, &SwapChain, &Device, &featureLevel, &DeviceContext
        );

        if (FAILED(hr)) return false;

        CreateRenderTarget();
        return true;
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (backBuffer)
        {
            Device->CreateRenderTargetView(backBuffer, nullptr, &RenderTargetView);
            backBuffer->Release();
        }
    }

    bool InitImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr;  // Don't create imgui.ini on disk
        io.LogFilename = nullptr;  // Don't create imgui_log.txt
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(Window);
        ImGui_ImplDX11_Init(Device, DeviceContext);

        return true;
    }

    void BeginFrame()
    {
        // Sync overlay position with target window
        if (TargetWindow)
        {
            RECT targetRect;
            GetWindowRect(TargetWindow, &targetRect);
            SetWindowPos(Window, HWND_TOPMOST, targetRect.left, targetRect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EndFrame()
    {
        ImGui::Render();

        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
        DeviceContext->ClearRenderTargetView(RenderTargetView, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        SwapChain->Present(1, 0);
    }

    bool ProcessMessages()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                Running = false;
                return false;
            }
        }
        return true;
    }

    void Cleanup()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
        if (SwapChain) { SwapChain->Release(); SwapChain = nullptr; }
        if (DeviceContext) { DeviceContext->Release(); DeviceContext = nullptr; }
        if (Device) { Device->Release(); Device = nullptr; }
        if (Window) { DestroyWindow(Window); Window = nullptr; }
    }

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

private:
    // Generate a random name that looks like a legitimate Windows class
    static std::wstring GenerateRandomName()
    {
        // Mix of plausible-looking prefixes
        static const wchar_t* prefixes[] = {
            L"Windows.UI.", L"Microsoft.", L"Shell.", L"DWM.",
            L"Input.", L"Runtime.", L"Desktop.", L"Core."
        };
        static const wchar_t* suffixes[] = {
            L"Handler", L"Manager", L"Service", L"Worker",
            L"Provider", L"Host", L"Bridge", L"Monitor"
        };

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> prefDist(0, _countof(prefixes) - 1);
        std::uniform_int_distribution<> sufDist(0, _countof(suffixes) - 1);
        std::uniform_int_distribution<> numDist(100, 9999);

        std::wstring name = prefixes[prefDist(gen)];
        name += suffixes[sufDist(gen)];
        name += L".";
        name += std::to_wstring(numDist(gen));

        return name;
    }
};
