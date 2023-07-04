// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <process.h>
#include <vector>


#define BUFSIZE 100
#define NAMESIZE 20
#define PORT 4578
#define SERVER_IP "61.83.251.78"
#define NAME "Person111111"
#define MAX_INPUT 512

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

unsigned WINAPI SendMSG(void* arg);
unsigned WINAPI RecvMSG(void* arg);
void ErrorHandling(const char* message);

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

char name[NAMESIZE] = "[Default]";
char message[BUFSIZE];

struct ChatData {
    char InputBuf[MAX_INPUT];
    std::vector<std::string> Items;
    SOCKET ConnectSocket;
    SOCKADDR_IN servAddr;
    HANDLE Mutex;

    ChatData() :
        InputBuf{}
    {

    }
};

// Main code
int main(int, char**)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Create chat data
    ChatData chatData;
    chatData.Mutex = CreateMutex(NULL, FALSE, NULL);

    // Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    // Set name and connect to the server
    sprintf_s(name, "[%s]", NAME);
    chatData.ConnectSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (chatData.ConnectSocket == INVALID_SOCKET)
        ErrorHandling("socket() error");

    chatData.servAddr.sin_family = AF_INET;
    chatData.servAddr.sin_port = htons(PORT);
    chatData.servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(chatData.ConnectSocket, (SOCKADDR*)&chatData.servAddr, sizeof(chatData.servAddr)) == SOCKET_ERROR)
        ErrorHandling("connect() error");

    // Create sender and receiver threads
    HANDLE sendThread = (HANDLE)_beginthreadex(NULL, 0, &SendMSG, &chatData, 0, nullptr);
    HANDLE recvThread = (HANDLE)_beginthreadex(NULL, 0, &RecvMSG, &chatData, 0, nullptr);

    if (sendThread == 0 || recvThread == 0)
        ErrorHandling("쓰레드 생성 오류");

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        bool open = true;
        if (ImGui::Begin("Chat Window", &open))
        {
            ImGui::Text("Chat");
            ImGui::Separator();

            // Display items
            {
                WaitForSingleObject(chatData.Mutex, INFINITE);
                for (const auto& item : chatData.Items) {
                    ImGui::TextUnformatted(item.c_str());
                }
                ReleaseMutex(chatData.Mutex);
            }

            // Input field
            if (ImGui::InputText("Input", chatData.InputBuf, IM_ARRAYSIZE(chatData.InputBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                // Send input text
                {
                    WaitForSingleObject(chatData.Mutex, INFINITE);
                    chatData.Items.push_back(std::string(name) + " " + chatData.InputBuf);
                    ReleaseMutex(chatData.Mutex);
                }
                memset(chatData.InputBuf, 0, sizeof(chatData.InputBuf));
            }

            ImGui::End();
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    WaitForSingleObject(sendThread, INFINITE);
    WaitForSingleObject(recvThread, INFINITE);

    CloseHandle(sendThread);
    CloseHandle(recvThread);

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

unsigned WINAPI SendMSG(void* arg)
{
    auto* chatData = reinterpret_cast<ChatData*>(arg);

    char nameMessage[NAMESIZE + MAX_INPUT];

    while (true)
    {
        sprintf_s(nameMessage, "%s %s", name, chatData->InputBuf);

        if (chatData->InputBuf[0], '\0')
        {
            send(chatData->ConnectSocket, nameMessage, (int)strlen(nameMessage), 0);

            memset(chatData->InputBuf, 0, sizeof(chatData->InputBuf)); // Clear input buffer
        }
    }

    return 0;
}

unsigned WINAPI RecvMSG(void* arg)
{
    auto* chatData = reinterpret_cast<ChatData*>(arg);
    char nameMessage[NAMESIZE + MAX_INPUT];

    while (true)
    {
        // Receive message from the server
        int strLen = recv(chatData->ConnectSocket, nameMessage, NAMESIZE + MAX_INPUT - 1, 0);

        nameMessage[strLen] = '\0';

        // Add received message to chat items
        WaitForSingleObject(chatData->Mutex, INFINITE);
        chatData->Items.push_back(nameMessage);
        ReleaseMutex(chatData->Mutex);
    }

    return 0;
}

void ErrorHandling(const char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
