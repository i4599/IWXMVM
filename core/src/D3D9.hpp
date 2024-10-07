#pragma once

namespace IWXMVM::D3D9
{
    void Initialize();

    HWND FindWindowHandle();
    IDirect3DDevice9* GetDevice();
    IDirect3DTexture9* GetDepthTexture();
    bool CaptureBackBuffer(IDirect3DTexture9* texture);
    bool CreateTexture(IDirect3DTexture9*& texture, ImVec2 size);
    bool IsReshadePresent();
}  // namespace IWXMVM::D3D9