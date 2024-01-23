// cl main.c /Zi /nologo /link /SUBSYSTEM:windows

#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#define COBJMACROS

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <d3d11_1.h>
#include <dxgidebug.h>
#include <Dxgi1_3.h>
#include <assert.h>
#include <float.h>
#include <stddef.h>
#include <intrin.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define Assert(c) do { if (!(c)) __debugbreak(); } while (0)
#define AssertHR(hr) Assert(SUCCEEDED(hr))

#pragma comment (lib, "user32")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "d3dcompiler")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "gdi32")
#pragma comment (lib, "dxgi")

// Types.

typedef uint32_t U32;
typedef float    F32;

typedef struct { 
    float m[4][4]; 
} Matrix;

typedef struct {
	Matrix model;
} D3D11Constants;

// Globals.

U32 client_width;
U32 client_height;

ID3D11Buffer *constant_buffer;
ID3D11Device *device;
ID3D11DeviceContext *context;
ID3D11RenderTargetView *render_target_view;
ID3D11InputLayout *input_layout;
ID3D11SamplerState *sampler_state;
IDXGISwapChain1 *swap_chain;
ID3D11ShaderResourceView *shader_resource_view;
D3D11_VIEWPORT viewport;

LRESULT CALLBACK 
win_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch(msg) {
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
        case WM_KEYDOWN: {
            switch(w_param) {
                case 'O': {
                    DestroyWindow(window);
                } break;
            }
        } break;
        default: {
            return DefWindowProcW(window, msg, w_param, l_param);
        }
    }
    return 0;
}

int WINAPI 
WinMain(HINSTANCE instance, HINSTANCE p_instance, LPSTR cmd, int cmd_show) {
    
    // Window creation.
    
    WNDCLASSW wc = { 
        .lpszClassName = L"MyWindowClass",
        .lpfnWndProc = win_proc,
        .hInstance = instance,
        .hCursor = LoadCursor(NULL, IDC_CROSS),
    };
    ATOM atom = RegisterClassW(&wc);
    Assert(atom && "Failed to register a window");
    
    HWND window = CreateWindowW(wc.lpszClassName, 
                                L"Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                client_width, client_height,
                                NULL, NULL, instance, NULL);
    Assert(window && "Failed to create a window");
    ShowWindow(window, cmd_show);
    
    // Fullscreen.
    
    DWORD window_style = GetWindowLong(window, GWL_STYLE);
    MONITORINFO mi = { sizeof(mi) };
    if(GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        client_width = mi.rcMonitor.right - mi.rcMonitor.left;
        client_height = mi.rcMonitor.bottom - mi.rcMonitor.top;
        SetWindowLong(window, GWL_STYLE, window_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     client_width,
                     client_height,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    
    // Viewport.
    
    viewport.Width = client_width;
    viewport.Height = client_height;
    viewport.MaxDepth = 1.0f;
    
    // Device & Context.
    
    UINT creation_flags = 0;
#ifndef NDEBUG
    creation_flags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
    
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creation_flags, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &device, NULL, &context);
    
    AssertHR(hr);
    
    // Debug.
    
#ifndef NDEBUG
    {
        ID3D11InfoQueue *info;
        ID3D11Device_QueryInterface(device, &IID_ID3D11InfoQueue, (void**)&info);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        ID3D11InfoQueue_Release(info);
    }
    {
        IDXGIInfoQueue *dxgi_info;
        hr = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, (void**)&dxgi_info);
        AssertHR(hr);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
        IDXGIInfoQueue_Release(dxgi_info);
    }
#endif
    
    // Swap chain.
    
    IDXGIDevice *dxgi_device;
    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    AssertHR(hr);
    
    IDXGIAdapter *dxgi_adapter;
    hr = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);
    AssertHR(hr);
    
    IDXGIFactory2 *dxgi_factory;
    hr = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void **)&dxgi_factory);
    AssertHR(hr);
    
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { 
        .Width = 0,
        .Height = 0,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc.Count = 1,
        .SampleDesc.Quality = 0,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = 0, 
    };
    
    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)device, window, &swap_chain_desc, 0, 0, &swap_chain);
    AssertHR(hr);
    
    IDXGIFactory_MakeWindowAssociation(dxgi_factory, window, DXGI_MWA_NO_ALT_ENTER);
    
    IDXGIFactory2_Release(dxgi_factory);
    IDXGIAdapter_Release(dxgi_adapter);
    ID3D11Device_Release(dxgi_device);
    
    // Render target view.
    
    ID3D11Texture2D *back_buffer;
    hr = IDXGISwapChain1_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
    AssertHR(hr);
    
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, NULL, &render_target_view);
    AssertHR(hr);
    
    ID3D11Texture2D_Release(back_buffer);
    
    // Shader.
    
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D10Blob *vs_blob;
    ID3D10Blob *ps_blob;
    ID3D11InputLayout *il;
    ID3D11Buffer *buffer;
    
    hr = D3DCompileFromFile(L"shader.hlsl", NULL, NULL, "vs_main", "vs_5_0", NULL, NULL, &vs_blob, NULL);
    AssertHR(hr);
    
    hr = D3DCompileFromFile(L"shader.hlsl", NULL, NULL, "ps_main", "ps_5_0", NULL, NULL, &ps_blob, NULL);
    AssertHR(hr);
    
    hr = ID3D11Device1_CreateVertexShader(
                                          device, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), 
                                          0, &vs);
    AssertHR(hr);
    
    hr = ID3D11Device1_CreatePixelShader(
                                         device, ID3D10Blob_GetBufferPointer(ps_blob), ID3D10Blob_GetBufferSize(ps_blob), 
                                         0, &ps);
    AssertHR(hr);
    
    // Input layout.
    
    D3D11_INPUT_ELEMENT_DESC
        input_element_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    
    
    hr = ID3D11Device1_CreateInputLayout(device, input_element_desc, ARRAYSIZE(input_element_desc), ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), &input_layout);
    AssertHR(hr);
    
    // Mesh.
    
    F32 vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
    };
    
    U32 stride = 5 * sizeof(F32);
    U32 num_vertices = sizeof(vertices) / stride;
    U32 offset = 0;
    
    D3D11_BUFFER_DESC buffer_desc = { 
        sizeof(vertices), 
        D3D11_USAGE_DEFAULT, 
        D3D11_BIND_VERTEX_BUFFER, 
        0, 0, 0 
    };
    
    D3D11_SUBRESOURCE_DATA initial_data = { vertices };
    
    ID3D11Device1_CreateBuffer(device, &buffer_desc, &initial_data, &buffer);
    
    // Texture.
    
    U32 image_width = 0;
    U32 image_height = 0;
    U32 image_channels = 0;
    U32 image_desired_channels = 4;
    
    unsigned char *image_data = stbi_load("image.png", &image_width, &image_height, &image_channels, image_desired_channels);
    Assert(image_data);
    
    U32 image_pitch = image_width * 4;
    
    D3D11_TEXTURE2D_DESC texture_desc = { 
        .Width = image_width,
        .Height = image_height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc.Count = 1,
        .SampleDesc.Quality = 0,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    
    D3D11_SUBRESOURCE_DATA texture_subresource_data = {
        .pSysMem = image_data,
        .SysMemPitch = image_pitch,
    };
    
    ID3D11Texture2D *texture;
    
    hr = ID3D11Device1_CreateTexture2D(device, &texture_desc, &texture_subresource_data, &texture);
    AssertHR(hr);
    
    free(image_data);
    
    hr = ID3D11Device1_CreateShaderResourceView(device, (ID3D11Resource *)texture, NULL, &shader_resource_view);
    AssertHR(hr);
    
    // Sampler.
    
    D3D11_SAMPLER_DESC sampler_desc = { 
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .BorderColor[0] = 1.0f,
        .BorderColor[1] = 1.0f,
        .BorderColor[2] = 1.0f,
        .BorderColor[3] = 1.0f,
        .MinLOD = -FLT_MAX,
        .MaxLOD = FLT_MAX,
    };
    
    hr =  ID3D11Device1_CreateSamplerState(device, &sampler_desc, &sampler_state);
    AssertHR(hr);
    
    // Constant buffer.
    
    D3D11_BUFFER_DESC constant_buffer_desc = {
        .ByteWidth = sizeof(D3D11Constants),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    
    hr =
        ID3D11Device1_CreateBuffer(device, &constant_buffer_desc, 
                                   NULL, &constant_buffer);
    AssertHR(hr);
    
    // Loop.
    
    for(;;) {
        MSG msg;
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        
        F32 color[] = { 0.2f, 0.2f, 0.2f, 1.0f};
        
        ID3D11DeviceContext1_ClearRenderTargetView(context, render_target_view, color);
        ID3D11DeviceContext1_RSSetViewports(context, 1, &viewport);
        ID3D11DeviceContext1_OMSetRenderTargets(context, 1, &render_target_view, NULL);
        ID3D11DeviceContext1_VSSetConstantBuffers(context, 0, 1, &constant_buffer);
        ID3D11DeviceContext1_PSSetShaderResources(context, 0, 1, &shader_resource_view);
        ID3D11DeviceContext1_PSSetSamplers(context, 0, 1, &sampler_state);
        
        ID3D11DeviceContext1_IASetInputLayout(context, input_layout);
        ID3D11DeviceContext1_VSSetShader(context, vs, 0, 0);
        ID3D11DeviceContext1_PSSetShader(context, ps, 0, 0);
        ID3D11DeviceContext1_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11DeviceContext1_IASetVertexBuffers(context, 0, 1, &buffer, &stride, &offset);
        
        // Constants.
        
        D3D11_MAPPED_SUBRESOURCE mapped_subresource;
        
        ID3D11DeviceContext1_Map(context, 
                                 (ID3D11Resource *)constant_buffer, 
                                 0, D3D11_MAP_WRITE_DISCARD, 
                                 0, &mapped_subresource);
        D3D11Constants *constants = (D3D11Constants *)mapped_subresource.pData;
        
        constants->model = (Matrix){
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 1.0f
        };
        
        ID3D11DeviceContext1_Unmap(context, (ID3D11Resource *)constant_buffer, 0);
        
        // Draw.
        
        ID3D11DeviceContext1_Draw(context, num_vertices, 0);
        
        // Swap.
        
        IDXGISwapChain1_Present(swap_chain, 1, 0); 
        
    }
    return 0;
}
