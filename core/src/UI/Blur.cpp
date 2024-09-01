#include "StdInclude.hpp"
#include "Blur.hpp"

#include "D3D9.hpp"
#include "Resources.hpp"
#include "UI/UIManager.hpp"

namespace IWXMVM::UI
{
    INCBIN_EXTERN(KAWASE_DOWN_SHADER);
    INCBIN_EXTERN(KAWASE_UP_SHADER);
    INCBIN_EXTERN(BLUR_VSHADER);

    struct FS_Vertex
    {
        float pos[3];
        float uv[2];
    };

    struct Pass
    {
        IDirect3DTexture9* texture;
        IDirect3DSurface9* surface;
        float data[4];
    };

    static constexpr std::size_t KAWASE_PASS_COUNT = 5;

    static std::array<Pass, KAWASE_PASS_COUNT + 1> kawasePasses;

    static IDirect3DVertexBuffer9* vertexBuffer;

    static IDirect3DTexture9* blurredTex;
    static IDirect3DSurface9* blurredSurface;
    static IDirect3DTexture9* copyTex;
    static IDirect3DSurface9* copySurface;
    static IDirect3DSurface9* renderTarget;

    static IDirect3DVertexDeclaration9* vertDecl;
    static IDirect3DPixelShader9* kawaseDownPS;
    static IDirect3DPixelShader9* kawaseUpPS;
    static IDirect3DVertexShader9* blurVS;

    static bool CreatePixelShaderDown()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();
        ID3DXBuffer* error_msg_buffer = nullptr;
        ID3DXBuffer* ps_buffer = nullptr;

        HRESULT hr =
            D3DXCompileShader(reinterpret_cast<LPCSTR>(KAWASE_UP_SHADER_data), static_cast<UINT>(KAWASE_UP_SHADER_size),
                              nullptr, nullptr, "main", "ps_3_0", 0, &ps_buffer, &error_msg_buffer, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to compile pixel shader: {}",
                      reinterpret_cast<const char*>(error_msg_buffer->GetBufferPointer()));
            error_msg_buffer->Release();
            return false;
        }

        hr = device->CreatePixelShader(reinterpret_cast<const DWORD*>(ps_buffer->GetBufferPointer()), &kawaseDownPS);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create downscaling pixel shader");
            ps_buffer->Release();
            return false;
        }

        ps_buffer->Release();

        return true;
    }

    static bool CreatePixelShaderUp()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();
        ID3DXBuffer* error_msg_buffer = nullptr;
        ID3DXBuffer* ps_buffer = nullptr;

        HRESULT hr = D3DXCompileShader(reinterpret_cast<LPCSTR>(KAWASE_DOWN_SHADER_data),
                                       static_cast<UINT>(KAWASE_DOWN_SHADER_size), nullptr, nullptr, "main", "ps_3_0",
                                       0, &ps_buffer, &error_msg_buffer, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to compile pixel shader: {}",
                      reinterpret_cast<const char*>(error_msg_buffer->GetBufferPointer()));
            error_msg_buffer->Release();
            return false;
        }

        hr = device->CreatePixelShader(reinterpret_cast<const DWORD*>(ps_buffer->GetBufferPointer()), &kawaseUpPS);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create upscaling pixel shader");
            ps_buffer->Release();
            return false;
        }

        ps_buffer->Release();

        return true;
    }

    static bool CreateVertexShader()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();
        ID3DXBuffer* error_msg_buffer = nullptr;
        ID3DXBuffer* vs_buffer = nullptr;

        HRESULT hr =
            D3DXCompileShader(reinterpret_cast<LPCSTR>(BLUR_VSHADER_data), static_cast<UINT>(BLUR_VSHADER_size),
                              nullptr, nullptr, "main", "vs_3_0", 0, &vs_buffer, &error_msg_buffer, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to compile vertex shader: {}",
                      reinterpret_cast<const char*>(error_msg_buffer->GetBufferPointer()));
            error_msg_buffer->Release();
            return false;
        }

        hr = device->CreateVertexShader(reinterpret_cast<const DWORD*>(vs_buffer->GetBufferPointer()), &blurVS);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create vertex shader");
            vs_buffer->Release();
            return false;
        }
        vs_buffer->Release();

        return true;
    }

    static bool CreateVertexBuffer()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();

        FS_Vertex square_verts[] = {
            {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        };

        HRESULT hr = device->CreateVertexBuffer(static_cast<UINT>(sizeof(square_verts)), D3DUSAGE_WRITEONLY, 0,
                                                D3DPOOL_DEFAULT, &vertexBuffer, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create vertex buffer");
            return false;
        }

        void* temp = nullptr;
        vertexBuffer->Lock(0, static_cast<UINT>(sizeof(square_verts)), &temp, 0);
        std::memcpy(temp, square_verts, sizeof(square_verts));
        vertexBuffer->Unlock();

        return true;
    }

    static bool CreateTextures()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();

        HRESULT hr = device->CreateTexture(static_cast<UINT>(Manager::GetWindowSizeX()),
                                           static_cast<UINT>(Manager::GetWindowSizeY()), 1, D3DUSAGE_RENDERTARGET,
                                           D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &blurredTex, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create blurred texture");
            return false;
        }

        hr = blurredTex->GetSurfaceLevel(0, &blurredSurface);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to get blurred surface");
            return false;
        }

        hr = device->CreateTexture(static_cast<UINT>(Manager::GetWindowSizeX()),
                                   static_cast<UINT>(Manager::GetWindowSizeY()), 1, D3DUSAGE_RENDERTARGET,
                                   D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &copyTex, nullptr);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create copy texture for");
            return false;
        }

        hr = copyTex->GetSurfaceLevel(0, &copySurface);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to get surface from copy texture");
            return false;
        }

        std::int32_t kw = static_cast<std::int32_t>(Manager::GetWindowSizeX());
        std::int32_t kh = static_cast<std::int32_t>(Manager::GetWindowSizeY());
        for (auto& pass : kawasePasses)
        {
            hr = device->CreateTexture(static_cast<UINT>(kw), static_cast<UINT>(kh), 1, D3DUSAGE_RENDERTARGET,
                                       D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pass.texture, nullptr);
            if (FAILED(hr))
            {
                LOG_ERROR("UI Blur: Failed to create texture for kawase filter pass");
                return false;
            }

            IDirect3DTexture9* ktex = pass.texture;
            hr = ktex->GetSurfaceLevel(0, &pass.surface);
            if (FAILED(hr))
            {
                LOG_ERROR("UI Blur: Failed to get surface for kawase filter pass");
                return false;
            }

            pass.data[0] = (1.0f / static_cast<float>(kw)) * 0.5f;
            pass.data[1] = (1.0f / static_cast<float>(kh)) * 0.5f;

            kw /= 2;
            kh /= 2;
        }

        hr = device->GetRenderTarget(0, &renderTarget);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to get render target");
            return false;
        }

        return true;
    }

    static bool CreatePipeline()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();

        D3DVERTEXELEMENT9 decl[] = {
            {0, offsetof(FS_Vertex, pos), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, offsetof(FS_Vertex, uv), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END(),
        };
        HRESULT hr = device->CreateVertexDeclaration(decl, &vertDecl);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to create vertex declaration");
            return false;
        }

        if (!CreatePixelShaderDown())
        {
            return false;
        }

        if (!CreatePixelShaderUp())
        {
            return false;
        }

        if (!CreateVertexShader())
        {
            return false;
        }

        return true;
    }

    static void SetRenderState()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();

        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPING, FALSE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);

        D3DXMATRIX identityMatrix = {};
        D3DXMatrixIdentity(&identityMatrix);
        device->SetTransform(D3DTS_WORLD, &identityMatrix);
        device->SetTransform(D3DTS_VIEW, &identityMatrix);
        device->SetTransform(D3DTS_PROJECTION, &identityMatrix);

        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    }

    bool Blur::CreateResources()
    {
        if (!CreateVertexBuffer())
        {
            DestroyResources();
            return false;
        }

        if (!CreateTextures())
        {
            DestroyResources();
            return false;
        }

        if (!CreatePipeline())
        {
            DestroyResources();
            return false;
        }

        return true;
    }

    void Blur::DestroyResources()
    {
        if (blurVS)
        {
            blurVS->Release();
            blurVS = nullptr;
        }

        if (kawaseDownPS)
        {
            kawaseDownPS->Release();
            kawaseDownPS = nullptr;
        }

        if (kawaseUpPS)
        {
            kawaseUpPS->Release();
            kawaseUpPS = nullptr;
        }

        if (vertDecl)
        {
            vertDecl->Release();
            vertDecl = nullptr;
        }

        if (renderTarget)
        {
            renderTarget->Release();
            renderTarget = nullptr;
        }

        for (auto& pass : kawasePasses)
        {
            if (pass.texture)
            {
                pass.texture->Release();
                pass.texture = nullptr;
            }

            if (pass.surface)
            {
                pass.surface->Release();
                pass.surface = nullptr;
            }
        }

        if (copySurface)
        {
            copySurface->Release();
            copySurface = NULL;
        }

        if (copyTex)
        {
            copyTex->Release();
            copyTex = nullptr;
        }

        if (blurredSurface)
        {
            blurredSurface->Release();
            blurredSurface = nullptr;
        }

        if (blurredTex)
        {
            blurredTex->Release();
            blurredTex = nullptr;
        }
    }

    void Blur::Capture()
    {
        IDirect3DDevice9* device = D3D9::GetDevice();

        IDirect3DStateBlock9* stateBlock = nullptr;
        D3DMATRIX lastWorld = {}, lastView = {}, lastProjection = {};

        HRESULT hr = device->CreateStateBlock(D3DSBT_ALL, &stateBlock);
        if (FAILED(hr))
        {
            LOG_ERROR("UI Blur: Failed to get the D3D9 state block");
            return;
        }

        hr |= device->GetTransform(D3DTS_WORLD, &lastWorld);
        hr |= device->GetTransform(D3DTS_VIEW, &lastView);
        hr |= device->GetTransform(D3DTS_PROJECTION, &lastProjection);

        SetRenderState();

        if (FAILED(device->StretchRect(renderTarget, nullptr, kawasePasses[0].surface, nullptr, D3DTEXF_LINEAR)))
        {
            LOG_WARN("UI Blur: Failed to copy render target to blurred surface");
        }

        IDirect3DSurface9* oldRT = nullptr;
        hr |= device->GetRenderTarget(0, &oldRT);

        const float texelOffset[4] = {-1.0f / Manager::GetWindowSizeX(), 1.0f / Manager::GetWindowSizeX(), 0.0f, 0.0f};
        hr |= device->SetVertexShaderConstantF(0, texelOffset, 1);
        hr |= device->SetVertexShader(blurVS);
        hr |= device->SetVertexDeclaration(vertDecl);
        hr |= device->SetStreamSource(0, vertexBuffer, 0, sizeof(FS_Vertex));

        // Downsample
        hr |= device->SetPixelShader(kawaseDownPS);
        for (std::size_t i = 0; i < KAWASE_PASS_COUNT; i++)
        {
            hr |= device->SetPixelShaderConstantF(0, kawasePasses[i].data, 1);
            hr |= device->SetTexture(0, kawasePasses[i].texture);
            hr |= device->SetRenderTarget(0, kawasePasses[i + 1].surface);
            hr |= device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        }

        // Upsample
        hr |= device->SetPixelShader(kawaseUpPS);
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(KAWASE_PASS_COUNT); i > 0; i--)
        {
            hr |= device->SetPixelShaderConstantF(0, kawasePasses[i].data, 1);
            hr |= device->SetTexture(0, kawasePasses[i].texture);
            hr |= device->SetRenderTarget(0, kawasePasses[i - 1].surface);
            hr |= device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        }

        hr |= device->SetRenderTarget(0, oldRT);

        hr |= device->SetTransform(D3DTS_WORLD, &lastWorld);
        hr |= device->SetTransform(D3DTS_VIEW, &lastView);
        hr |= device->SetTransform(D3DTS_PROJECTION, &lastProjection);

        if (hr != 0)
        {
            LOG_WARN("UI Blur: Something went wrong while capturing");
        }

        stateBlock->Apply();
        stateBlock->Release();
    }

    void Blur::RenderToWindow(ImVec2 size, ImVec2 pos, float scroll)
    {
        if (Manager::IsBlurInitialized())
        {
            ImVec2 oldCursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos({0.0f, scroll});

            ImVec2 uv0 = {pos.x / Manager::GetWindowSizeX(), pos.y / Manager::GetWindowSizeY()};
            ImVec2 uv1 = {(pos.x + size.x) / Manager::GetWindowSizeX(), (pos.y + size.y) / Manager::GetWindowSizeY()};
            ImGui::Image(reinterpret_cast<ImTextureID>(Blur::GetTexture()), size, uv0, uv1, {1.0f, 1.0f, 1.0f, 0.4f});

            ImGui::SetCursorPos(oldCursor);
        }
    }

    IDirect3DTexture9* Blur::GetTexture() noexcept
    {
        return kawasePasses[0].texture;
    }
}  // namespace IWXMVM::UI