#include "StdInclude.hpp"
#include "DemoLoader.hpp"

#include "Mod.hpp"
#include "UI/UIManager.hpp"
#include "Utilities/PathUtils.hpp"
#include "Resources.hpp"
#include "Configuration/PreferencesConfiguration.hpp"

namespace IWXMVM::UI
{
    template <bool caseSensitive, typename T>
        requires std::is_same_v<T, std::string_view> || std::is_same_v<T, std::wstring_view>
    bool CompareNaturally(const T lhs, const T rhs)
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

    void SortDemoPaths(const auto demos)
    {
        std::sort(demos.begin(), demos.end(), [&](const auto& lhs, const auto& rhs) {
            const std::size_t extLength = Mod::GetGameInterface()->GetDemoExtension().length();
            const std::size_t dirLength = demos.front().parent_path().native().length();
            const std::size_t lhsLength = lhs.native().length();
            const std::size_t rhsLength = rhs.native().length();

            assert(lhsLength > dirLength + extLength + 1 && rhsLength > dirLength + extLength + 1);

            const auto* lhsFileNamePtr = lhs.c_str() + dirLength + 1;
            const auto* rhsFileNamePtr = rhs.c_str() + dirLength + 1;
            const std::size_t lhsSvLength = lhsLength - dirLength - extLength - 1;
            const std::size_t rhsSvLength = rhsLength - dirLength - extLength - 1;

            using StringView = std::wstring_view;
            return CompareNaturally<false>(StringView{lhsFileNamePtr, lhsSvLength},
                                           StringView{rhsFileNamePtr, rhsSvLength});
        });
    }

    void SortDemoDirectories(const auto directories, auto GetPath)
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

    bool IsFileDemo(const std::filesystem::path& file)
    {
        return file.extension().compare(Mod::GetGameInterface()->GetDemoExtension())
                   ? false
                   : true;  // .compare() == 0 => strings are equal
    }

    void DemoLoader::AddPathsToSearch(const std::vector<std::filesystem::path>& dirs)
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

    void DemoLoader::SearchDir(std::size_t dirIdx)
    {
        auto tempDir = std::string(DEMO_TEMP_DIRECTORY);
        if (demoDirectories[dirIdx].path.wstring().find(std::wstring(tempDir.begin(), tempDir.end())) != std::string::npos)
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
                demoPaths.push_back(entry.path());
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

    void DemoLoader::Search()
    {
        for (auto i = searchPaths.first; i < searchPaths.second; i++)
        {
            SearchDir(i);
        }
    }

    void DemoLoader::MarkDirsRelevancy()
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

    void DemoLoader::FindAllDemos()
    {
        if (isScanningDemoPaths.load())
			return;

        LOG_DEBUG("Searching for demo files...");
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

            LOG_DEBUG("Found {0} demo files", demoPaths.size());

            isScanningDemoPaths.store(false);            
        }).detach();
    }

    bool DemoLoader::DemoFilter(const std::u8string& demoFileName)
    {
        // TODO: possibly make filter more advanced, add features like "" for exact search etc
        bool keepDemo = true;
        std::u8string lowerDemoFileName = demoFileName;
        std::transform(lowerDemoFileName.begin(), lowerDemoFileName.end(), lowerDemoFileName.begin(),
                       ::tolower);
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


    void DemoLoader::RenderDemos(const std::vector<std::filesystem::path>& demos)
    {
        ImGuiListClipper clipper;
        clipper.Begin(demos.size());

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                try
                {
                    auto demoName = demos[i].filename().string();

                    if (Mod::GetGameInterface()->GetGameState() == Types::GameState::InDemo &&
                        Mod::GetGameInterface()->GetDemoInfo().name == demoName)
                    {
                        if (ImGui::Button(std::format(ICON_FA_STOP " STOP##{0}", demoName).c_str(),
                                          ImVec2(ImGui::GetFontSize() * 4, ImGui::GetFontSize() * 1.5f)))
                        {
                            Mod::GetGameInterface()->Disconnect();
                        }
                    }
                    else
                    {
                        if (ImGui::Button(std::format(ICON_FA_PLAY " PLAY##{0}", demoName).c_str(),
                                          ImVec2(ImGui::GetFontSize() * 4, ImGui::GetFontSize() * 1.5f)))
                        {
                            Mod::GetGameInterface()->PlayDemo(demos[i]);
                        }
                    }

                    ImGui::SameLine();

                    ImGui::Text("%s", demoName.c_str());
                }
                catch (std::exception&)
                {
                    ImGui::BeginDisabled();
                    ImGui::Button(ICON_FA_TRIANGLE_EXCLAMATION " ERROR");
                    ImGui::SameLine();
                    ImGui::Text("<invalid demo name>");
                    ImGui::EndDisabled();
                }
            }
        }
    }

    // Wrapper for RenderDemos
    void DemoLoader::FilteredRenderDemos(const std::pair<std::size_t, std::size_t>& demos)
    {
        auto it = cachedfilteredDemos.find(demos);
        if (it != cachedfilteredDemos.end())
        {
            RenderDemos(it->second);
            return;
        }

        std::vector<std::filesystem::path> filteredDemos;
        for (std::size_t i = demos.first; i < demos.second; i++)
        {
            try
            {
                if (searchBarText.empty() || DemoFilter(demoPaths[i].filename().u8string()))
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


    void DemoLoader::RenderDir(const DemoDirectory& dir)
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

    void DemoLoader::RecacheSearchBarTextSplit()
    {
        searchBarTextSplit.clear();

        std::stringstream ss(searchBarText);
        std::string token;

        while (std::getline(ss, token, ' '))
        {
            searchBarTextSplit.push_back(std::u8string(token.begin(), token.end()));
        }
    }

    void DemoLoader::RenderSearchBar()
    {
        int flags = ImGuiInputTextFlags_CallbackResize;
        bool disableInputText =
            Mod::GetGameInterface()->IsConsoleOpen() ||
            (UI::UIManager::Get().IsControllableCameraModeSelected() && UI::UIManager::Get().GetUIComponent(UI::Component::GameView)->HasFocus());
        if (disableInputText)
        {
            ImGui::BeginDisabled();
            flags = flags | ImGuiInputTextFlags_ReadOnly;
        }
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint(
            "##SearchInput", "Search...", &searchBarText[0], searchBarText.capacity() + 1, flags,
            [](ImGuiInputTextCallbackData* data) -> int
            {
                if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                {
                    std::string* str = reinterpret_cast<std::string*>(data->UserData);
                    str->resize(data->BufTextLen);
                    data->Buf = &(*str)[0];
                }
                return 0;
            },
            &searchBarText);

        if (disableInputText)
        {
            ImGui::EndDisabled();
        }

        if (lastSearchBarText != searchBarText)
        {
            RecacheSearchBarTextSplit();
            cachedfilteredDemos.clear();
            totalCachedFilteredDemosCount = 0;
            lastSearchBarText = searchBarText;
        }
    }

    void DemoLoader::RenderSearchPaths()
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

    void DemoLoader::Initialize()
    {
        FindAllDemos();
    }

    void DemoLoader::Render()
    {
        ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;
        if (ImGui::Begin("Demos", nullptr, flags))
        {
            ImGui::AlignTextToFramePadding();

            if (isScanningDemoPaths.load())
            {
                ImGui::Text("Searching for demo files...");
            }
            else
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%d demos found!",
                            searchBarText.empty() ? demoPaths.size() : totalCachedFilteredDemosCount);
                ImGui::SameLine();

                auto addPathButtonLabel = std::string(ICON_FA_FOLDER_OPEN " Add path");
                auto refreshButtonLabel = std::string(ICON_FA_ROTATE_RIGHT " Refresh");
                auto buttonSize = ImVec2(ImGui::GetFontSize() * 6.0f, ImGui::GetFontSize() * 1.75f);

                if (ImGui::GetWindowWidth() < buttonSize.x * 4)
                {
                    buttonSize = ImVec2(buttonSize.y, buttonSize.y);
                    addPathButtonLabel = addPathButtonLabel.substr(0, addPathButtonLabel.find(" "));
                    refreshButtonLabel = refreshButtonLabel.substr(0, refreshButtonLabel.find(" "));
                }

                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 2 - ImGui::GetStyle().ItemSpacing.x -
                                     ImGui::GetStyle().WindowPadding.x);
                
                if (ImGui::Button(addPathButtonLabel.c_str(), buttonSize))
                {
                    auto path = PathUtils::OpenFolderBrowseDialog();
                    if (path.has_value())
                    {
                        PreferencesConfiguration::Get().additionalDemoSearchDirectories.push_back(path.value());
                        Configuration::Get().Write(true); 
						FindAllDemos();
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button(refreshButtonLabel.c_str(), buttonSize))
                {
                    FindAllDemos();
                    ImGui::End();
                    return;
                }

                // Search paths will always be rendered, even if empty
                RenderSearchBar();
                RenderSearchPaths();
            }

            ImGui::End();
        }
    }

    void DemoLoader::Release()
    {
    }
}  // namespace IWXMVM::UI
