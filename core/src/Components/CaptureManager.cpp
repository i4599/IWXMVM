#include "StdInclude.hpp"
#include "CaptureManager.hpp"

#include "Configuration/PreferencesConfiguration.hpp"
#include "Components/Playback.hpp"
#include "Components/Rendering.hpp"
#include "Components/Rewinding.hpp"
#include "D3D9.hpp"
#include "Events.hpp"
#include "Graphics/Graphics.hpp"
#include "Mod.hpp"
#include "UI/Components/Notifications.hpp"
#include "Utilities/PathUtils.hpp"

namespace IWXMVM::Components
{
    std::string_view CaptureManager::GetVideoCodecLabel(VideoCodec codec)
    {
        switch (codec)
        {
            case VideoCodec::Prores4444XQ:
                return "Prores 4444 XQ";
            case VideoCodec::Prores4444:
                return "Prores 4444";
            case VideoCodec::Prores422HQ:
                return "Prores 422 HQ";
            case VideoCodec::Prores422:
                return "Prores 422";
            case VideoCodec::Prores422LT:
                return "Prores 422 LT";
            default:
                return "Unknown Video Codec";
        }
    }

    void CaptureManager::Initialize()
    {
        // Set r_smp_backend to 0.
        // This should ensure that there are no separate threads for game logic and rendering
        // which frees us from having to do synchronizations for our recording code.
        auto r_smp_backend = Mod::GetGameInterface()->GetDvar("r_smp_backend");
        if (!r_smp_backend.has_value())
        {
            LOG_ERROR("Could not set r_smp_backend; dvar not found");
        }
        else
        {
            r_smp_backend.value().value->int32 = false;
        }

        IDirect3DDevice9* device = D3D9::GetDevice();

        if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        {
            LOG_ERROR("Failed to capture backbuffer. Capture resolution not found.");
            return;
        }

        D3DSURFACE_DESC bbDesc = {};
        if (FAILED(backBuffer->GetDesc(&bbDesc)))
        {
            LOG_ERROR("Failed to get backbuffer description. Capture resolution not found.");
            return;
        }

        backBuffer->Release();
        backBuffer = nullptr;

        const Resolution gameResolution = {
            static_cast<std::int32_t>(bbDesc.Width),
            static_cast<std::int32_t>(bbDesc.Height),
        };
        for (std::ptrdiff_t i = 0; i < std::ssize(supportedResolutions); i++)
        {
            supportedResolutions[i] = {
                gameResolution.width / (i + 1),
                gameResolution.height / (i + 1),
            };
        }

        captureSettings = {0, 0, VideoCodec::Prores4444, gameResolution, 250};

        auto& outputDirectory = PreferencesConfiguration::Get().captureOutputDirectory;
        if (outputDirectory.empty())
        {
            outputDirectory = std::filesystem::path(PathUtils::GetCurrentGameDirectory()) / "IWXMVM" / "recordings";
        }

        Events::RegisterListener(EventType::OnDemoBoundsDetermined, [&]() {
            if (captureSettings.startTick == 0 || captureSettings.endTick == 0)
            {
                auto endTick = Mod::GetGameInterface()->GetDemoInfo().endTick;
                captureSettings.startTick = static_cast<int32_t>(endTick * 0.1);
                captureSettings.endTick = static_cast<int32_t>(endTick * 0.9);
            }
        });

        Events::RegisterListener(EventType::OnFrame, [&]() { OnRenderFrame(); });
    }

    void CaptureManager::CaptureFrame()
    {
        framePrepared = false;

        FILE* outputPipe = pipe;
        if (MultiPassEnabled())
        {
            const auto passIndex = static_cast<std::size_t>(capturedFrameCount) % captureSettings.passes.size();
            GFX::GraphicsManager::Get().DrawShaderForPassIndex(passIndex);

            outputPipe = captureSettings.passes[passIndex].pipe;
        }

        IDirect3DDevice9* device = D3D9::GetDevice();

        if (FAILED(device->StretchRect(backBuffer, NULL, downsampledRenderTarget, NULL, D3DTEXF_NONE)))
        {
            LOG_ERROR("Failed to copy data from backbuffer to render target");
            StopCapture();
            return;
        }

        if (FAILED(device->GetRenderTargetData(downsampledRenderTarget, tempSurface)))
        {
            LOG_ERROR("Failed copy render target data to surface");
            StopCapture();
            return;
        }

        D3DLOCKED_RECT lockedRect = {};
        if (FAILED(tempSurface->LockRect(&lockedRect, nullptr, 0)))
        {
            LOG_ERROR("Failed to lock surface");
            StopCapture();
            return;
        }

        const auto surfaceByteSize = screenDimensions.width * screenDimensions.height * 4;
        std::fwrite(lockedRect.pBits, surfaceByteSize, 1, outputPipe);

        capturedFrameCount++;

        if (FAILED(tempSurface->UnlockRect()))
        {
            LOG_ERROR("Failed to unlock surface");
            StopCapture();
            return;
        }

        const auto currentTick = Mod::GetGameInterface()->GetDemoInfo().gameTick;
        if (!Rewinding::IsRewinding() && currentTick > captureSettings.endTick)
        {
            StopCapture();
        }
    }

    void CaptureManager::PrepareFrame()
    {
        if (!isCapturing.load())
            return;

        if (MultiPassEnabled())
        {
            const auto passIndex = static_cast<std::size_t>(capturedFrameCount) % captureSettings.passes.size();
            const auto& pass = captureSettings.passes[passIndex];

            Rendering::SetVisibleElements(pass.elements);
        }

        framePrepared = true;
    }

    void CaptureManager::OnRenderFrame()
    {
        if (!isCapturing || Rewinding::IsRewinding())
            return;
    }

    int32_t CaptureManager::OnGameFrame()
    {
        if (MultiPassEnabled())
        {
            return capturedFrameCount % captureSettings.passes.size() == 0 ? 1000 / GetCaptureSettings().framerate : 0;
        }
        else
        {
            return 1000 / GetCaptureSettings().framerate;
        }
    }

    void CaptureManager::ToggleCapture()
    {
        if (!isCapturing)
        {
            StartCapture();
        }
        else
        {
            StopCapture();
        }
    }

    std::filesystem::path GetFFmpegPath()
    {
        auto appdataPath = std::filesystem::path(getenv("APPDATA"));
        return appdataPath / "codmvm_launcher" / "ffmpeg.exe";
    }

    std::string GetFFmpegCommand(const Components::CaptureSettings& captureSettings,
                                 const std::filesystem::path& outputDirectory, const Resolution screenDimensions,
                                 const std::string& name)
    {
        auto path = GetFFmpegPath();
        std::int32_t profile = 0;
        const char* pixelFormat = nullptr;
        switch (captureSettings.videoCodec.value())
        {
            case VideoCodec::Prores4444XQ:
                profile = 5;
                pixelFormat = "yuv444p10le";
                break;
            case VideoCodec::Prores4444:
                profile = 4;
                pixelFormat = "yuv444p10le";
                break;
            case VideoCodec::Prores422HQ:
                profile = 3;
                pixelFormat = "yuv422p10le";
                break;
            case VideoCodec::Prores422:
                profile = 2;
                pixelFormat = "yuv422p10le";
                break;
            case VideoCodec::Prores422LT:
                profile = 1;
                pixelFormat = "yuv422p10le";
                break;
            default:
                profile = 4;
                pixelFormat = "yuv444p10le";
                LOG_ERROR("Unsupported video codec. Choosing default ({})",
                          static_cast<std::int32_t>(VideoCodec::Prores4444));
                break;
        }

        return std::format(
            "{} -f rawvideo -pix_fmt bgra -s {}x{} -r {} -i - -c:v prores -profile:v {} -q:v 1 "
            "-pix_fmt {} -vf scale={}:{} -y \"{}\\{}\" 2>&1",
            path.string(), screenDimensions.width, screenDimensions.height, captureSettings.framerate, profile,
            pixelFormat, captureSettings.resolution.width, captureSettings.resolution.height, outputDirectory.string(),
            name + ".mov");
    }

    void CaptureManager::StartCapture()
    {
        if (captureSettings.startTick >= captureSettings.endTick)
        {
            LOG_ERROR("Start tick must be less than end tick");
            UI::Notifications::Send(UI::Notifications::Notification::Error, "Start tick must be less than end tick!");
            return;
        }

        if (captureSettings.name.empty())
        {
            LOG_ERROR("Capture settings name is empty");
            UI::Notifications::Send(UI::Notifications::Notification::Error, "Recording must have a name!");
            return;
        }

        // Check for pass duplicates
        if (captureSettings.multipass)
        {
            bool identicalWarning = false;
            for (std::size_t i = 0; i < captureSettings.passes.size(); i++)
            {
                const auto& pass1 = captureSettings.passes[i];
                for (std::size_t j = 0; j < captureSettings.passes.size(); j++)
                {
                    if (i == j)
                    {
                        continue;
                    }

                    const auto& pass2 = captureSettings.passes[j];

                    if (pass1.name == pass2.name && !pass2.name.empty())
                    {
                        LOG_ERROR("Passes {} and {} have the same name", i + 1, j + 1);
                        UI::Notifications::Send(UI::Notifications::Notification::Error,
                                                "One or more passes have the same name. Stopping capture!");
                        return;
                    }

                    if (pass1.elements == pass2.elements && pass1.type == pass2.type &&
                        pass1.useReshade == pass2.useReshade && !identicalWarning)
                    {
                        UI::Notifications::Send(UI::Notifications::Notification::Warning,
                                                "One or more passes are identical!");
                        identicalWarning = true;
                    }
                }
            }
        }

        // ensure output directory exists
        const auto& outputDirectory = PreferencesConfiguration::Get().captureOutputDirectory;
        if (!std::filesystem::exists(outputDirectory))
        {
            if (!std::filesystem::create_directories(outputDirectory))
            {
                LOG_ERROR("Something went wrong while creating the output directory ({})", outputDirectory.string());
                UI::Notifications::Send(UI::Notifications::Notification::Error,
                                        "Something went wrong while creating the output directory!");
                return;
            }
        }

        // skip to start tick
        auto currentTick = Mod::GetGameInterface()->GetDemoInfo().gameTick;
        Playback::SetTickDelta(captureSettings.startTick - currentTick, true);

        capturedFrameCount = 0;

        IDirect3DDevice9* device = D3D9::GetDevice();

        if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        {
            LOG_ERROR("Failed to capture backbuffer");
            StopCapture();
            return;
        }

        D3DSURFACE_DESC bbDesc = {};
        if (FAILED(backBuffer->GetDesc(&bbDesc)))
        {
            LOG_ERROR("Failed to get backbuffer description");
            StopCapture();
            return;
        }
        if (FAILED(device->CreateOffscreenPlainSurface(bbDesc.Width, bbDesc.Height, bbDesc.Format, D3DPOOL_SYSTEMMEM,
                                                       &tempSurface, nullptr)))
        {
            LOG_ERROR("Failed to create temporary surface");
            StopCapture();
            return;
        }

        if (FAILED(device->CreateRenderTarget(bbDesc.Width, bbDesc.Height, bbDesc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                              &downsampledRenderTarget, NULL)))
        {
            LOG_ERROR("Failed to create render target");
            StopCapture();
            return;
        }

        screenDimensions.width = static_cast<std::int32_t>(bbDesc.Width);
        screenDimensions.height = static_cast<std::int32_t>(bbDesc.Height);

        if (!std::filesystem::exists(GetFFmpegPath()))
        {
            LOG_ERROR("ffmpeg is not present in the game directory");
            UI::Notifications::Send(UI::Notifications::Notification::Error,
                                    "FFmpeg is not present in the game directory!");
            ffmpegNotFound = true;
            StopCapture();
            return;
        }
        ffmpegNotFound = false;

        LOG_INFO("Starting capture at {0} ({1} fps)", captureSettings.resolution.ToString(), captureSettings.framerate);

        UI::Notifications::Send(UI::Notifications::Notification::Info,
                                std::format("Starting capture ({}, {}, {} fps)", captureSettings.name,
                                            captureSettings.resolution.ToString(), captureSettings.framerate));

        if (!captureSettings.multipass)
        {
            std::string ffmpegCommand = GetFFmpegCommand(captureSettings, outputDirectory, screenDimensions, captureSettings.name);
            LOG_DEBUG("ffmpeg command: {}", ffmpegCommand);
            pipe = _popen(ffmpegCommand.c_str(), "wb");
            if (!pipe)
            {
                LOG_ERROR("ffmpeg pipe open error");
                StopCapture();
                return;
            }
        }
        else
        {
            // If multi-pass mode is on, the passes must be placed inside a directory
            const auto newDirectory = outputDirectory / captureSettings.name;
            if (!std::filesystem::exists(newDirectory))
            {
                if (!std::filesystem::create_directory(newDirectory))
                {
                    LOG_ERROR("Something went wrong while creating the recording directory ({})",
                              outputDirectory.string());
                    UI::Notifications::Send(UI::Notifications::Notification::Error,
                                            "Something went wrong while creating the recording directory!");
                    return;
                }
            }

            for (auto& pass : captureSettings.passes)
            {
                std::string passName;
                if (pass.name.empty())
                {
                    passName = std::format("Pass {}", pass.id);
                }
                else
                {
                    passName = pass.name;
                }

                std::string ffmpegCommand = GetFFmpegCommand(captureSettings, newDirectory, screenDimensions, passName);
                LOG_DEBUG("ffmpeg command: {}", ffmpegCommand);
                pass.pipe = _popen(ffmpegCommand.c_str(), "wb");
                if (!pass.pipe)
                {
                    LOG_ERROR("ffmpeg pipe open error");
                    StopCapture();
                    return;
                }
            }
        }

        isCapturing.store(true);
    }

    void CaptureManager::StopCapture()
    {
        LOG_INFO("Stopped capture (wrote {0} frames)", capturedFrameCount);
        UI::Notifications::Send(UI::Notifications::Notification::Info,
                                std::format("Finished capture ({0} frames)", capturedFrameCount));
        isCapturing.store(false);

        Rendering::ResetVisibleElements();
        framePrepared = false;

        if (pipe)
        {
            fflush(pipe);
            fclose(pipe);
            pipe = nullptr;
        }

        for (auto& pass : captureSettings.passes)
        {
            if (pass.pipe)
            {
                fflush(pass.pipe);
                fclose(pass.pipe);
                pass.pipe = nullptr;
            }
        }

        if (tempSurface)
        {
            tempSurface->Release();
            tempSurface = nullptr;
        }

        if (backBuffer)
        {
            backBuffer->Release();
            backBuffer = nullptr;
        }

        if (downsampledRenderTarget)
        {
            downsampledRenderTarget->Release();
            downsampledRenderTarget = nullptr;
        }

        if (depthSurface)
        {
            depthSurface->Release();
            depthSurface = nullptr;
        }

        if (depthShader)
        {
            depthShader->Release();
            depthShader = nullptr;
        }
    }
}  // namespace IWXMVM::Components