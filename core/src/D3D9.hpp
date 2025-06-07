#pragma once

namespace IWXMVM::D3D9
{
    void Initialize();

    HWND FindWindowHandle();
    IDirect3DDevice9* GetDevice();
    IDirect3DTexture9* GetDepthTexture();
    bool IsReshadePresent();
}  // namespace IWXMVM::D3D9