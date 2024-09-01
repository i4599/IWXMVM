#include "StdInclude.hpp"
#include "D3D9.hpp"

#include "MinHook.h"

#include "Events.hpp"
#include "Graphics/Graphics.hpp"
#include "Utilities/HookManager.hpp"
#include "Utilities/PathUtils.hpp"
#include "Mod.hpp"

#include "UI/UIManager.hpp"

namespace IWXMVM::D3D9
{
    HWND gameWindowHandle = nullptr;
    void* d3d9DeviceVTable[119];
    void* d3d9SwapChainVTable[10];
    void* d3d9VTable[17];
    IDirect3DDevice9* device = nullptr;

    typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9* pDevice);
    EndScene_t EndScene;
    EndScene_t ReshadeOriginalEndScene;
    typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
    Reset_t Reset;
    typedef HRESULT(__stdcall* Present_t)(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                                          HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags);
    Present_t SwapChainPresent;
    typedef HRESULT(__stdcall* CreateDevice_t)(IDirect3D9* pInterface, UINT Adapter, D3DDEVTYPE DeviceType,
                                               HWND hFocusWindow, DWORD BehaviorFlags,
                                               D3DPRESENT_PARAMETERS* pPresentationParameters,
                                               IDirect3DDevice9** ppReturnedDeviceInterface);
    CreateDevice_t CreateDevice;

    bool CheckForOverlays(std::uintptr_t returnAddress)
    {
        static constexpr std::array overlayNames{
            "gameoverlay",  // Steam
            "discord"       // Discord
        };
        static std::array<std::uintptr_t, std::size(overlayNames)> returnAddresses{};

        for (std::size_t i = 0; i < returnAddresses.size(); ++i)
        {
            if (!returnAddresses[i])
            {
                MEMORY_BASIC_INFORMATION mbi;
                ::VirtualQuery(reinterpret_cast<LPCVOID>(returnAddress), &mbi, sizeof(MEMORY_BASIC_INFORMATION));

                char module[1024];
                ::GetModuleFileName(static_cast<HMODULE>(mbi.AllocationBase), module, sizeof(module));

                if (std::string_view{module}.find(overlayNames[i]) != std::string_view::npos)
                {
                    returnAddresses[i] = returnAddress;
                    return true;
                }
            }
            else if (returnAddresses[i] == returnAddress)
            {
                return true;
            }
        }

        return false;
    }

    ImVec2 GetBackBufferSize(IDirect3DDevice9* pDevice)
    {
        IDirect3DSurface9* backBuffer = nullptr;
        HRESULT hr = pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        if (SUCCEEDED(hr))
        {
            D3DSURFACE_DESC backBufferDesc = {};
            backBuffer->GetDesc(&backBufferDesc);
            return {static_cast<float>(backBufferDesc.Width), static_cast<float>(backBufferDesc.Height)};
        }
        else
        {
            LOG_ERROR("Failed to retrieve backbuffer when querying size");
            return {};
        }
    }

    HRESULT __stdcall CreateDevice_Hook(IDirect3D9* pInterface, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                        DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                        IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        LOG_DEBUG("CreateDevice called with hwnd {0:x}", (std::uintptr_t)pPresentationParameters->hDeviceWindow);

        GFX::GraphicsManager::Get().Uninitialize();
        UI::Manager::Shutdown();

        HRESULT hr = CreateDevice(pInterface, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters,
                                  ppReturnedDeviceInterface);
        if (hr != D3D_OK)
        {
            return hr;
        }

        device = *ppReturnedDeviceInterface;

        UI::Manager::Initialize(device, pPresentationParameters->hDeviceWindow, GetBackBufferSize(device));
        GFX::GraphicsManager::Get().Initialize();

        return hr;
    }

    std::optional<void*> reshadeEndSceneAddress;
    bool IsReshadePresent()
    {
        return reshadeEndSceneAddress.has_value();
    }

    std::size_t reshadeEndSceneCallCount;
    HRESULT __stdcall EndScene_Hook(IDirect3DDevice9* pDevice)
    {
        const std::uintptr_t returnAddress = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        if (CheckForOverlays(returnAddress))
        {
            return EndScene(pDevice);
        }

        if (!UI::Manager::IsInitialized())
        {
            device = pDevice;
            UI::Manager::Initialize(pDevice, FindWindowHandle(), GetBackBufferSize(device));
            GFX::GraphicsManager::Get().Initialize();
        }

        if (Mod::GetGameInterface()->GetGameState() == Types::GameState::InDemo)
        {
            GFX::GraphicsManager::Get().Render();
        }

        if (!reshadeEndSceneAddress.has_value())
        {
            UI::Manager::Frame();
        }

        return EndScene(pDevice);
    }

    // This is only called if reshade is present
    HRESULT __stdcall ReshadeOriginalEndScene_Hook(IDirect3DDevice9* pDevice)
    {
        if (reshadeEndSceneCallCount > 0)
        {
            return ReshadeOriginalEndScene(pDevice);
        }
        ++reshadeEndSceneCallCount;

        UI::Manager::Frame();
        return ReshadeOriginalEndScene(pDevice);
    }

    HRESULT __stdcall Reset_Hook(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
    {

        GFX::GraphicsManager::Get().Uninitialize();

        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = Reset(pDevice, pPresentationParameters);
        ImGui_ImplDX9_CreateDeviceObjects();

        // do we need to re-initialize the UI components?

        if (UI::Manager::IsInitialized())
        {
            GFX::GraphicsManager::Get().Initialize();
        }

        return hr;
    }

    HRESULT __stdcall SwapChainPresent_Hook(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                                   HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags)
    {
        reshadeEndSceneCallCount = 0;
        return SwapChainPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
    }

    void CheckPresenceReshade()
    {
        auto IsReshadeDllPresent = [](auto dllName) {
            const std::filesystem::path gamePath(PathUtils::GetCurrentGameDirectory());
            const auto reshadePath = gamePath / dllName;
            if (!std::filesystem::exists(reshadePath))
            {
                return false;
            }

            bool found = false;
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            if (SUCCEEDED(hr))
            {
                IShellItem2* pShellItem;
                hr = SHCreateItemFromParsingName(reshadePath.wstring().c_str(), nullptr, IID_PPV_ARGS(&pShellItem));
                if (SUCCEEDED(hr))
                {
                    LPWSTR fileDesc = nullptr;
                    hr = pShellItem->GetString(PKEY_FileDescription, &fileDesc);
                    if (SUCCEEDED(hr))
                    {
                        if (std::wstring_view(fileDesc).find(L"ReShade"))
                        {
                            found = true;
                        }
                        CoTaskMemFree(fileDesc);
                    }
                    pShellItem->Release();
                }
                CoUninitialize();
            }

            return found;
        };

        bool reshadeFound = IsReshadeDllPresent("d3d9.dll") ||
                            IsReshadeDllPresent("dxgi.dll");

        auto device = Mod::GetGameInterface()->GetGameDevicePtr();
        auto vTable = *reinterpret_cast<void***>(device);
        auto orgEndScene = vTable[42];

        if (reshadeFound && orgEndScene != d3d9DeviceVTable[42])
        {
            LOG_DEBUG("Detected Reshade presence; original EndScene address is {}, Reshade EndScene address is {}.",
                      orgEndScene, d3d9DeviceVTable[42]);

            reshadeEndSceneAddress = std::exchange(d3d9DeviceVTable[42], orgEndScene);
        }
    }

    void FindSwapChain()
    {
        const auto device = Mod::GetGameInterface()->GetGameDevicePtr();

        IDirect3DSwapChain9* pSwapChain = nullptr;
        const HRESULT hr = device->GetSwapChain(0, &pSwapChain);

        if (FAILED(hr) || !pSwapChain)
        {
            throw std::runtime_error("Failed to find D3D9 SwapChain!");
        }
        else
        {
            memcpy(d3d9SwapChainVTable, *(void**)pSwapChain, 10 * sizeof(void*));
            pSwapChain->Release();

            LOG_DEBUG("Found D3D9 SwapChain Present address: {}", d3d9SwapChainVTable[3]);
        }
    }

    void CreateDummyDevice()
    {
        IDirect3D9* d3dObj = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3dObj)
        {
            throw std::runtime_error("Failed to create D3D object");
        }

        IDirect3DDevice9* dummyDevice = nullptr;
        D3DPRESENT_PARAMETERS d3d_params{};

        // Try to create device - will fail if in fullscreen
        HRESULT result = d3dObj->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL,
                                              D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                                              &d3d_params, &dummyDevice);

        // Fail -> death
        if (FAILED(result) || !dummyDevice)
        {
            d3dObj->Release();
            throw std::runtime_error("Failed to create dummy D3D device");
        }

        memcpy(d3d9DeviceVTable, *(void**)dummyDevice, 119 * sizeof(void*));
        memcpy(d3d9VTable, *(void**)d3dObj, 17 * sizeof(void*));

        LOG_DEBUG("Created dummy D3D device");

        CheckPresenceReshade();

        dummyDevice->Release();
        d3dObj->Release();
    }

    void Hook()
    {
        // TODO: move minhook initialization somewhere else
        if (MH_Initialize() != MH_OK)
        {
            throw std::runtime_error("Failed to initialize MinHook");
        }

        HookManager::CreateHook((std::uintptr_t)d3d9VTable[16], (std::uintptr_t)CreateDevice_Hook,
                                (std::uintptr_t*)&CreateDevice);
        HookManager::CreateHook((std::uintptr_t)d3d9DeviceVTable[16], (std::uintptr_t)Reset_Hook,
                                (std::uintptr_t*)&Reset);
        HookManager::CreateHook((std::uintptr_t)d3d9SwapChainVTable[3], (std::uintptr_t)SwapChainPresent_Hook,
                                (std::uintptr_t*)&SwapChainPresent);
        HookManager::CreateHook((std::uintptr_t)d3d9DeviceVTable[42], (std::uintptr_t)EndScene_Hook,
                                (std::uintptr_t*)&EndScene);
        
        if (reshadeEndSceneAddress.has_value())
        {
            HookManager::CreateHook((std::uintptr_t)reshadeEndSceneAddress.value(),
                                    (std::uintptr_t)ReshadeOriginalEndScene_Hook,
                                    (std::uintptr_t*)&ReshadeOriginalEndScene);
        }
    }

    void Initialize()
    {
        FindSwapChain();
        CreateDummyDevice();
        Hook();
        LOG_DEBUG("Hooked D3D9");
    }

    HWND FindWindowHandle()
    {
        auto* device = GetDevice();
        if (device == nullptr)
        {
            LOG_CRITICAL("Failed to get the game window");
            return nullptr;
        }

        D3DDEVICE_CREATION_PARAMETERS params{};
        device->GetCreationParameters(&params);

        if (params.hFocusWindow == nullptr)
        {
            LOG_CRITICAL("Failed to get the game window");
            return nullptr;
        }

        return gameWindowHandle = params.hFocusWindow;
    }

    IDirect3DDevice9* GetDevice()
    {
        return device;
    }

    bool CaptureBackBuffer(IDirect3DTexture9* texture)
    {
        auto device = GetDevice();

        IDirect3DSurface9* RenderTarget = NULL;
        auto result = device->GetRenderTarget(0, &RenderTarget);
        if (FAILED(result))
            return false;

        IDirect3DSurface9* textureSurface;
        result = texture->GetSurfaceLevel(0, &textureSurface);
        if (FAILED(result))
        {
            textureSurface->Release();
            RenderTarget->Release();
            return false;
        }

        result = device->StretchRect(RenderTarget, NULL, textureSurface, NULL, D3DTEXF_LINEAR);
        if (FAILED(result))
        {
            textureSurface->Release();
            RenderTarget->Release();
            return false;
        }

        textureSurface->Release();
        RenderTarget->Release();
        return true;
    }

    bool CreateTexture(IDirect3DTexture9*& texture, ImVec2 size)
    {
        if (texture != NULL)
            texture->Release();

        auto device = D3D9::GetDevice();

        auto result = D3DXCreateTexture(device, (UINT)size.x, (UINT)size.y, D3DX_DEFAULT, D3DUSAGE_RENDERTARGET,
                                        D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &texture);
        if (FAILED(result))
            return false;

        return true;
    }
}  // namespace IWXMVM::D3D9
