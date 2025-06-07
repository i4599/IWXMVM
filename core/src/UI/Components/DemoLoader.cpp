#include "StdInclude.hpp"
#include "DemoLoader.hpp"

#include "Configuration/PreferencesConfiguration.hpp"
#include "Mod.hpp"
#include "Resources.hpp"
#include "Types/GameState.hpp"
#include "UI/Animations.hpp"
#include "UI/UIManager.hpp"
#include "Utilities/PathUtils.hpp"

#include "imgui_internal.h"

namespace IWXMVM::UI
{
    struct DemoDirectory
    {
        std::filesystem::path path;                          // Path
        std::pair<std::size_t, std::size_t> subdirectories;  // Pair of indices representing the [first, second)
                                                             // interval of directories in the 'demoDirectories' vector
        std::pair<std::size_t, std::size_t>
            demos;  // Pair of indices representing the [first, second) interval of demos in the 'demoPaths' vector
        std::optional<std::size_t> parentIdx;  // Parent directory's index in the 'demoDirectories' vector. Search
                                               // paths will contain a nullopt value
        bool relevant;                         // Does this directory ever reach a demo down the line?
    };

    struct DemoInfo
    {
        std::filesystem::path path;
        Animation anim;
    };

    static std::pair<std::size_t, std::size_t> searchPaths;  // Pair of indices representing the [first, second)
                                                             // interval
                                                             // of search paths in the 'demoDirectories' vector
    static std::vector<DemoDirectory> demoDirectories;
    static std::vector<DemoInfo> demoPaths;  // Path to every demo found
    static std::atomic<bool> isScanningDemoPaths;

    static std::string searchBarText;
    static std::string lastSearchBarText;
    static std::vector<std::u8string> searchBarTextSplit;
    struct cachedfilteredDemos_pairhash
    {
        size_t operator()(const std::pair<size_t, size_t>& p) const
        {
            size_t h1 = std::hash<size_t>{}(p.first);
            size_t h2 = std::hash<size_t>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };
    static std::unordered_map<std::pair<size_t, size_t>, std::vector<DemoInfo>, cachedfilteredDemos_pairhash>
        cachedfilteredDemos;
    static std::size_t totalCachedFilteredDemosCount;

    static std::filesystem::path playingDemoPath;  // Path of last played demo

    template <bool caseSensitive, typename T>
        requires std::is_same_v<T, std::string_view> || std::is_same_v<T, std::wstring_view>
    static bool CompareNaturally(const T lhs, const T rhs)
    {
        auto IsDigit = [](auto c) { return std::iswdigit(c); };

        auto ToUpper = [](auto c) {
            if constexpr (caseSensitive)
                return c;
            else
                return std::towupper(c);
        };

        auto StringToUint = [](const auto* str, auto* output, std::size_t* endPtr = nullptr) {
            try
            {
                *output = std::stoull(str, endPtr);
                return true;
            }
            catch (...)
            {
                return false;
            }
        };

        for (auto lhsItr = lhs.begin(), rhsItr = rhs.begin(); lhsItr != lhs.end() && rhsItr != rhs.end();)
        {
            if (IsDigit(*lhsItr) && IsDigit(*rhsItr))
            {
                std::uint64_t lhsNum = 0;
                std::uint64_t rhsNum = 0;
                std::size_t lhsDigitCount = 0;
                std::size_t rhsDigitCount = 0;

                // when sorting a container, move the 'unparsable' string_view to end of container if string-to-uint
                // throws an exception
                if (!StringToUint(std::addressof(*lhsItr), &lhsNum, &lhsDigitCount))
                    return false;
                if (!StringToUint(std::addressof(*rhsItr), &rhsNum, &rhsDigitCount))
                    return true;

                if (lhsNum != rhsNum)
                    return lhsNum < rhsNum;

                assert(lhsDigitCount == std::distance(lhsItr, std::find_if_not(lhsItr, lhs.end(), IsDigit)));
                assert(rhsDigitCount == std::distance(rhsItr, std::find_if_not(rhsItr, rhs.end(), IsDigit)));

                lhsItr += lhsDigitCount;
                rhsItr += rhsDigitCount;
            }
            else
            {
                if (ToUpper(*lhsItr) != ToUpper(*rhsItr))
                    return *lhsItr < *rhsItr;

                ++lhsItr;
                ++rhsItr;
            }
        }

        return lhs.length() < rhs.length();
    }

    static void SortDemoPaths(const auto demos)
    {
        std::sort(demos.begin(), demos.end(), [&](const auto& lhs, const auto& rhs) {
            const std::size_t extLength = Mod::GetGameInterface()->GetDemoExtension().length();
            const std::size_t dirLength = demos.front().path.parent_path().native().length();
            const std::size_t lhsLength = lhs.path.native().length();
            const std::size_t rhsLength = rhs.path.native().length();

            assert(lhsLength > dirLength + extLength + 1 && rhsLength > dirLength + extLength + 1);

            const auto* lhsFileNamePtr = lhs.path.c_str() + dirLength + 1;
            const auto* rhsFileNamePtr = rhs.path.c_str() + dirLength + 1;
            const std::size_t lhsSvLength = lhsLength - dirLength - extLength - 1;
            const std::size_t rhsSvLength = rhsLength - dirLength - extLength - 1;

            using StringView = std::wstring_view;
            return CompareNaturally<false>(StringView{lhsFileNamePtr, lhsSvLength},
                                           StringView{rhsFileNamePtr, rhsSvLength});
        });
    }

    static void SortDemoDirectories(const auto directories, auto GetPath)
    {
        std::sort(directories.begin(), directories.end(), [&](const auto& lhs, const auto& rhs) {
            const auto& lhsPath = GetPath(lhs);
            const auto& rhsPath = GetPath(rhs);

            const std::size_t parentDirLength = GetPath(directories.front()).parent_path().native().length();
            const std::size_t lhsLength = lhsPath.native().length();
            const std::size_t rhsLength = rhsPath.native().length();

            assert(lhsLength > parentDirLength + 1 && rhsLength > parentDirLength + 1);

            const auto* lhsDirPtr = lhsPath.c_str() + parentDirLength + 1;
            const auto* rhsDirNamePtr = rhsPath.c_str() + parentDirLength + 1;
            const std::size_t lhsSvLength = lhsLength - parentDirLength - 1;
            const std::size_t rhsSvLength = rhsLength - parentDirLength - 1;

            using StringView = std::wstring_view;
            return CompareNaturally<false>(StringView{lhsDirPtr, lhsSvLength}, StringView{rhsDirNamePtr, rhsSvLength});
        });
    }

    static bool IsFileDemo(const std::filesystem::path& file)
    {
        return file.extension().compare(Mod::GetGameInterface()->GetDemoExtension()) ? false : true;
    }

    static void AddPathsToSearch(const std::vector<std::filesystem::path>& dirs)
    {
        for (const auto& dir : dirs)
        {
            if (std::filesystem::exists(dir))
            {
                DemoDirectory searchPath = {.path = dir};
                demoDirectories.push_back(searchPath);
            }
        }
        searchPaths = std::make_pair(0, demoDirectories.size());
    }

    static void SearchDir(std::size_t dirIdx)
    {
        auto tempDir = std::string(DEMO_TEMP_DIRECTORY);
        if (demoDirectories[dirIdx].path.wstring().find(std::wstring(tempDir.begin(), tempDir.end())) !=
            std::string::npos)
        {
            demoDirectories[dirIdx].relevant = false;
            return;
        }

        auto subdirsStartIdx = demoDirectories.size();
        auto demosStartIdx = demoPaths.size();

        for (const auto& entry : std::filesystem::directory_iterator(demoDirectories[dirIdx].path))
        {
            if (entry.is_directory())
            {
                DemoDirectory subdir = {.path = entry.path(), .parentIdx = dirIdx};
                demoDirectories.push_back(subdir);
            }
            else if (IsFileDemo(entry.path()))
            {
                demoPaths.push_back({entry.path(), {}});
            }
        }

        SortDemoPaths(std::span{(demoPaths.begin() + demosStartIdx), demoPaths.end()});
        SortDemoDirectories(std::span{(demoDirectories.begin() + subdirsStartIdx), demoDirectories.end()},
                            [](const DemoDirectory& data) -> const std::filesystem::path& { return data.path; });

        demoDirectories[dirIdx].demos = std::make_pair(demosStartIdx, demoPaths.size());
        if (demoDirectories[dirIdx].demos.first != demoDirectories[dirIdx].demos.second)
        {
            demoDirectories[dirIdx].relevant = true;
        }

        demoDirectories[dirIdx].subdirectories = std::make_pair(subdirsStartIdx, demoDirectories.size());
        for (auto i = demoDirectories[dirIdx].subdirectories.first; i < demoDirectories[dirIdx].subdirectories.second;
             i++)
        {
            SearchDir(i);
        }
    }

    static void Search()
    {
        for (auto i = searchPaths.first; i < searchPaths.second; i++)
        {
            SearchDir(i);
        }
    }

    static void MarkDirsRelevancy()
    {
        for (auto it = demoDirectories.rbegin(); it != demoDirectories.rend(); it++)
        {
            std::size_t vecIdx = std::abs(it - demoDirectories.rend() + 1);
            if (demoDirectories[vecIdx].relevant)
            {
                while (demoDirectories[vecIdx].parentIdx.has_value())
                {
                    vecIdx = demoDirectories[vecIdx].parentIdx.value();
                    if (demoDirectories[vecIdx].relevant)
                    {
                        break;
                    }
                    demoDirectories[vecIdx].relevant = true;
                }
            }
        }
    }

    static void DrawUnderline(ImVec2 cursorPos, ImVec4 color, ImVec2 textSize, float thickness, float horizontalOffset,
                       float verticalOffset)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetCurrentWindow()->Pos;
        ImVec2 absoluteCursorPos = {windowPos.x + cursorPos.x - ImGui::GetCurrentWindow()->Scroll.x,
                                    windowPos.y + cursorPos.y - ImGui::GetCurrentWindow()->Scroll.y};

        drawList->AddRectFilled(
            {absoluteCursorPos.x + horizontalOffset, absoluteCursorPos.y + textSize.y + verticalOffset},
            {absoluteCursorPos.x + textSize.x + horizontalOffset,
             absoluteCursorPos.y + textSize.y - thickness + verticalOffset},
            ImGui::ColorConvertFloat4ToU32(color), 0.0f, ImDrawFlags_Closed);
    }

    static float Interp(float a, float b, float t)
    {
        float new_t = powf(sinf(t * glm::pi<float>() / 2.0f), 0.7f);
        return std::lerp(a, b, new_t);
    }

    static float GradientInterp(float a, float b, float t)
    {
        float new_t = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        return std::lerp(a, b, new_t);
    }

    static void RenderDemos(std::vector<DemoInfo>& demos)
    {
        ImGuiListClipper clipper;
        clipper.Begin(demos.size());

        const auto gameState = Mod::GetGameInterface()->GetGameState();
        const bool isInDemo = gameState == Types::GameState::InDemo || gameState == Types::GameState::LoadingDemo;

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                bool isThePlayingDemo = false;
                if (demos[i].path == playingDemoPath && isInDemo)
                {
                    isThePlayingDemo = true;
                }

                const auto demoName = demos[i].path.filename().string();
                const char* demoPrefix = " " ICON_FA_FILE_VIDEO " ";
                const std::size_t demoNameOffset = std::strlen(demoPrefix);

                char demoNameWithIcon[256] = {};
                strcpy_s(demoNameWithIcon, sizeof(demoNameWithIcon), demoPrefix);
				strcpy_s(demoNameWithIcon + demoNameOffset, sizeof(demoNameWithIcon) - demoNameOffset, demoName.c_str());

                ImVec2 textCursorPos = {ImGui::GetCursorPosX() + Manager::GetFontSize() / 12.0f,
                                        ImGui::GetCursorPosY()};

                if (!demos[i].anim.IsInvalid())
                {
                    float t = demos[i].anim.GetVal();
                    ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 1.0f, 1.0f, 1.0f - (t / 2.5f)});
                }

                if (isThePlayingDemo)
                    ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 1.0f, 1.0f, 1.0f - (1.0f / 2.5f)});

                ImGui::Text("%s", demoNameWithIcon);

                if (isThePlayingDemo)
                    ImGui::PopStyleColor(1);

                if (!demos[i].anim.IsInvalid())
                    ImGui::PopStyleColor(1);

                ImVec2 textSize = ImGui::CalcTextSize(demoNameWithIcon);

                if (isThePlayingDemo)
                {
                    demos[i].anim.Invalidate();
                }

                const float cursorPosAfterText = ImGui::GetCursorPosX();

                if (!demos[i].anim.IsInvalid() && demos[i].anim.IsFinished())
                {
                    demos[i].anim.Invalidate();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_None))
                {
                    if (!isThePlayingDemo)
                    {
                        if (demos[i].anim.IsInvalid())
                            demos[i].anim = Animation::Create(0.2f, 0.0f, 1.0f, Interp);
                        else
                            demos[i].anim.GoForward();
                    }

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isThePlayingDemo)
                    {
						Mod::GetGameInterface()->PlayDemo(demos[i].path);
						playingDemoPath = demos[i].path;
                    }
                }
                else if (!demos[i].anim.IsInvalid())
                {
                    demos[i].anim.GoBackward();
                }

                if (!demos[i].anim.IsInvalid())
                {
                    ImVec4 color = {1.0f, 1.0f, 1.0f, 1.0f - (1.0f / 2.5f)};
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                        color.w = 1.0f - (1.0f / 1.5f);

                    DrawUnderline(textCursorPos, color, {textSize.x * demos[i].anim.GetVal(), textSize.y}, 1.0f,
                                  Manager::GetFontSize() / 18.0f, Manager::GetFontSize() / 9.0f);
                }
            }
        }
    }

    static void RecacheSearchBarTextSplit()
    {
        searchBarTextSplit.clear();

        std::stringstream ss(searchBarText);
        std::string token;

        while (std::getline(ss, token, ' '))
        {
            searchBarTextSplit.push_back(std::u8string(token.begin(), token.end()));
        }
    }

    static bool DemoFilter(const std::u8string& demoFileName)
    {
        // TODO: possibly make filter more advanced, add features like "" for exact search etc
        bool keepDemo = true;
        std::u8string lowerDemoFileName = demoFileName;
        std::transform(lowerDemoFileName.begin(), lowerDemoFileName.end(), lowerDemoFileName.begin(), ::tolower);
        for (auto& word : searchBarTextSplit)
        {
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            if (lowerDemoFileName.find(word) == std::u8string::npos)
            {
                keepDemo = false;
                break;
            }
        }
        return keepDemo;
    }

    // Wrapper for RenderDemos
    static void FilteredRenderDemos(const std::pair<std::size_t, std::size_t>& demos)
    {
        auto it = cachedfilteredDemos.find(demos);
        if (it != cachedfilteredDemos.end())
        {
            RenderDemos(it->second);
            return;
        }

        std::vector<DemoInfo> filteredDemos;
        for (std::size_t i = demos.first; i < demos.second; i++)
        {
            try
            {
                if (searchBarText.empty() || DemoFilter(demoPaths[i].path.filename().u8string()))
                {
                    filteredDemos.push_back(demoPaths[i]);
                }
            }
            catch (std::exception&)
            {
                LOG_ERROR("Error filtering demoPath");
                // catching errors just in case
            }
        }
        totalCachedFilteredDemosCount += filteredDemos.size();
        cachedfilteredDemos[demos] = filteredDemos;
        RenderDemos(filteredDemos);
    }

    static void RenderDir(const DemoDirectory& dir)
    {
        try
        {
            if (ImGui::TreeNodeEx(dir.path.filename().string().c_str(),
                                  searchBarText.empty() ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto i = dir.subdirectories.first; i < dir.subdirectories.second; i++)
                {
                    if (demoDirectories[i].relevant)
                    {
                        RenderDir(demoDirectories[i]);
                    }
                }

                FilteredRenderDemos(dir.demos);

                ImGui::TreePop();
            }
        }
        catch (std::exception&)
        {
            ImGui::BeginDisabled();
            ImGui::TreeNode("<invalid directory name>");
            ImGui::TreePop();
            ImGui::EndDisabled();
        }
    }

    static void RenderSearchPaths()
    {
        for (auto i = searchPaths.first; i < searchPaths.second; i++)
        {
            if (ImGui::TreeNodeEx(demoDirectories[i].path.string().c_str(),
                                  searchBarText.empty() ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (demoDirectories[i].relevant)
                {
                    for (auto j = demoDirectories[i].subdirectories.first; j < demoDirectories[i].subdirectories.second;
                         j++)
                    {
                        if (demoDirectories[j].relevant)
                        {
                            RenderDir(demoDirectories[j]);
                        }
                    }
                }
                else
                {
                    ImGui::Text("Search path is empty.");
                }

                FilteredRenderDemos(demoDirectories[i].demos);

                ImGui::TreePop();
            }
        }
    }

    static void Refresh()
    {
        if (isScanningDemoPaths.load())
        {
            return;
        }

        isScanningDemoPaths.store(true);
        std::thread([&] {
            demoDirectories.clear();
            demoPaths.clear();

            auto searchPaths = std::vector(PreferencesConfiguration::Get().additionalDemoSearchDirectories);
            searchPaths.push_back(PathUtils::GetCurrentGameDirectory());

            AddPathsToSearch(searchPaths);
            Search();

            // 'demoDirectories' and 'demoPaths' are complete here, but we still need to find out the relevancy of each
            // directory
            MarkDirsRelevancy();

            isScanningDemoPaths.store(false);
        }).detach();
    }

    void DemoLoader::Initialize()
    {
        Refresh();
    }

    void DemoLoader::Show()
    {
        ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoCollapse);

        ImVec2 size = ImGui::GetWindowSize();

        // Refresh button
        {
            if (ImGui::Button(ICON_FA_REPEAT " Refresh"))
            {
				Refresh();
				ImGui::End();
				return;
            }
        }

        ImGui::SameLine();

        // Disconnect button
        {
            if (ImGui::Button(ICON_FA_CIRCLE " Disconnect"))
            {
                Mod::GetGameInterface()->Disconnect();
            }
        }

        // Search bar
        {
            ImGui::Text(ICON_FA_MAGNIFYING_GLASS);
            ImGui::SameLine();
            ImGui::InputTextWithHint(
                "##SearchInput", "Search...", &searchBarText[0], searchBarText.capacity() + 1,
                ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData* data) -> int {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        std::string* str = reinterpret_cast<std::string*>(data->UserData);
                        str->resize(data->BufTextLen);
                        data->Buf = &(*str)[0];
                    }
                    return 0;
                },
                &searchBarText);

            if (lastSearchBarText != searchBarText)
            {
                RecacheSearchBarTextSplit();
                cachedfilteredDemos.clear();
                totalCachedFilteredDemosCount = 0;
                lastSearchBarText = searchBarText;
            }
        }

        ImGuiWindowFlags childWindowFlags =
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
        ImGui::BeginChild("Demos_Window", {}, 0, childWindowFlags);

        if (!isScanningDemoPaths.load())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {Manager::GetFontSize() * 0.1f, 0.0f});
            RenderSearchPaths();
            ImGui::PopStyleVar();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Text("Loading...");
            ImGui::EndDisabled();
        }

        ImGui::EndChild();

        ImGui::End();
    }

    const char* DemoLoader::GetWindowName() noexcept
    {
        return "Demos";
    }
}  // namespace IWXMVM::UI