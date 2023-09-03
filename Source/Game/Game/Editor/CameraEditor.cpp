#include "CameraEditor.h"

#include <Base/Math/Color.h>
#include <Base/Math/Interpolation.h>
#include <Base/Memory/FileReader.h>

#include <Game/Util/ImguiUtil.h>
#include <Game/Util/ServiceLocator.h>
#include <Game/Application/EnttRegistries.h>
#include <Game/Rendering/GameRenderer.h>
#include <Game/ECS/Util/CameraUtil.h>
#include <Game/ECS/Singletons/ActiveCamera.h>
#include <Game/ECS/Singletons/CinematicDB.h>
#include <Game/ECS/Singletons/SplineDataDB.h>
#include <Game/ECS/Components/Transform.h>
#include <Game/ECS/Components/Camera.h>

#include <entt/entt.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>

#include <filesystem>

namespace Editor
{
    CameraEditor::CameraEditor()
        : BaseEditor(GetName(), false)
    {
        _splineData.resize(Spline::SplinePath::NUM_SEQUENCES);
        _cacheSplinePosition.resize(Spline::SplinePath::NUM_SEQUENCES);
        _cacheSplineTarget.resize(Spline::SplinePath::NUM_SEQUENCES);
    }

    void CameraEditor::DrawImGui()
    {
        EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
        entt::registry& registry = *registries->gameRegistry;
        entt::registry::context& ctx = registry.ctx();

        GameRenderer* gameRenderer = ServiceLocator::GetGameRenderer();
        DebugRenderer* debugRenderer = gameRenderer->GetDebugRenderer();
        auto& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
        ECS::Components::Camera& camera = registry.get<ECS::Components::Camera>(activeCamera.entity);

        if (ImGui::Begin(GetName(), &IsVisible()))
        {
            ReceiveDrop();
            DrawInterface();
            DrawGizmo(camera);
            DrawInViewport(debugRenderer);
        }
        ImGui::End();
    }

    void CameraEditor::ReceiveDrop()
    {
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE"))
            {
                auto* file = reinterpret_cast<const std::filesystem::path*>(payload->Data);
                if (file)
                {

                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void CameraEditor::LoadCinematic()
    {
        _isStarting = false;
        _cinematicTime = 0.0f;
        _currentTarget = vec3( 0.0f );

        _splineData.clear();

        EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
        entt::registry& registry = *registries->gameRegistry;
        entt::registry::context& ctx = registry.ctx();

        auto& splineDataDB = ctx.at<ECS::Singletons::SplineDataDB>();

        for (const auto& sequence : _currentCinematic.sequences)
        {
            SplineData& data = _splineData.emplace_back();

            // Position
            if (sequence.positionSpline > 0)
            {
                fs::path splineFilePath = splineDataDB.splineEntryIDToPath[sequence.positionSpline];
                fs::path absoluteFilePath = fs::absolute(splineFilePath);

                if (fs::is_regular_file(absoluteFilePath) && absoluteFilePath.extension() == ".spline")
                {
                    if (!LoadSplineFile(absoluteFilePath.string(), data.Position))
                    {
                        _splineData.clear();
                        return;
                    }
                }
            }

            // Target
            if (sequence.targetSpline > 0)
            {
                fs::path splineFilePath = splineDataDB.splineEntryIDToPath[sequence.targetSpline];
                fs::path absoluteFilePath = fs::absolute(splineFilePath);

                if (fs::is_regular_file(absoluteFilePath) && absoluteFilePath.extension() == ".spline")
                {
                    if (!LoadSplineFile(absoluteFilePath.string(), data.Target))
                    {
                        _splineData.clear();
                        return;
                    }
                }
            }

            // Roll
            if (sequence.rollSpline > 0)
            {
                fs::path splineFilePath = splineDataDB.splineEntryIDToPath[sequence.rollSpline];
                fs::path absoluteFilePath = fs::absolute(splineFilePath);

                if (fs::is_regular_file(absoluteFilePath) && absoluteFilePath.extension() == ".spline")
                {
                    if (!LoadSplineFile(absoluteFilePath.string(), data.Roll))
                    {
                        _splineData.clear();
                        return;
                    }
                }
            }

            // Fov
            if (sequence.fovSpline > 0)
            {
                fs::path splineFilePath = splineDataDB.splineEntryIDToPath[sequence.fovSpline];
                fs::path absoluteFilePath = fs::absolute(splineFilePath);

                if (fs::is_regular_file(absoluteFilePath) && absoluteFilePath.extension() == ".spline")
                {
                    if (!LoadSplineFile(absoluteFilePath.string(), data.Fov))
                    {
                        _splineData.clear();
                        return;
                    }
                }
            }
        }
    }

    bool CameraEditor::LoadSplineFile(const std::string& path, Spline::SplinePath& out)
    {
        FileReader file(path);
        if (!file.Open())
        {
            DebugHandler::PrintError("CinematicEditor: Failed to open Spline file: {0}", path);
            return false;
        }

        size_t fileSize = file.Length();
        std::shared_ptr<Bytebuffer> fileBuffer = Bytebuffer::BorrowRuntime(fileSize);

        file.Read(fileBuffer.get(), fileSize);
        file.Close();

        bool result = Spline::SplinePath::Read(fileBuffer, out);
        return result;
    }

    void CameraEditor::MarkAllSplineAsDirty()
    {
        for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
        {
            SplineData& data = _splineData[i];

            data.Position.MarkAsDirty();
            data.Target.MarkAsDirty();
            data.Roll.MarkAsDirty();
            data.Fov.MarkAsDirty();
        }
    }

    u32 CameraEditor::GetTotalTimestamp()
    {
        u32 result = 0;

        for (const auto& sequence : _currentCinematic.sequences)
            result += sequence.timestamp;

        return result;
    }

    vec3 CameraEditor::GetLastPosition()
    {
        vec3 result = vec3{ 0.0f };

        for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
        {
            SplineData& data = _splineData[i];
            if (_currentCinematic.sequences[i].timestamp > 0)
            {
                result = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, data.Position.GetInterpolatedStorage4D().Storage().back());
            }
        }

        return result;
    }

    vec3 CameraEditor::GetLastTarget()
    {
        vec3 result = vec3{ 0.0f };

        for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
        {
            SplineData& data = _splineData[i];
            if (_currentCinematic.sequences[i].timestamp > 0)
            {
                result = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, data.Target.GetInterpolatedStorage4D().Storage().back());
            }
        }

        return result;
    }

    void CameraEditor::Update(f32 deltaTime)
    {
        if (_playCinematicForLineOfView)
        {
            _lineOfViewTime += static_cast<i32>(deltaTime * 1000.0f);
        }

        if (_isStarting)
        {
            if (_cinematicTime > GetTotalTimestamp())
            {
                ECS::Util::CameraUtil::MoveTo(GetLastPosition() + vec3(0.0f, 1.0f, 0.0f));
                ECS::Util::CameraUtil::LookAt(GetLastTarget());
                ECS::Util::CameraUtil::ResetRoll();
                ECS::Util::CameraUtil::ResetFov();
                ECS::Util::CameraUtil::MarkAsDirty();

                _isStarting = false;
                _cinematicTime = 0.0f;
                return;
            }

            _cinematicTime += static_cast<u32>(deltaTime * 1000.0f);

            vec3 newPosition    = vec3{ 0.0f };
            vec3 newTarget      = vec3{ 0.0f };
            f32 newRoll         = ECS::Util::CameraUtil::GetRoll();
            f32 newFov          = ECS::Util::CameraUtil::GetFov();

            HandleCinematic(_currentCinematic, _cinematicTime, _splineData, newPosition, newTarget, newRoll, newFov);

            ECS::Util::CameraUtil::MoveTo(newPosition);
            ECS::Util::CameraUtil::LookAt(newTarget);
            ECS::Util::CameraUtil::SetRoll(newRoll);
            ECS::Util::CameraUtil::SetFov(newFov);
            ECS::Util::CameraUtil::MarkAsDirty();

            _currentTarget = newTarget;
        }
    }

    /* BEGIN STATIC */

    void CameraEditor::HandleCinematic(DB::Client::Definitions::Cinematic& cinematic, u32 time, std::vector<SplineData>& splineData, vec3& position, vec3& target, f32& roll, f32& fov)
    {
        SplineData& data                    = splineData[GetCurrentSequence(cinematic, time)];
        Spline::Spline4D splinePosition     = data.Position.GetSpline4D();
        Spline::Spline4D splineTarget       = data.Target.GetSpline4D();
        Spline::Spline2D splineRoll         = data.Roll.GetSpline2D();
        Spline::Spline2D splineFov          = data.Fov.GetSpline2D();

        if (splinePosition.data.empty() || splineTarget.data.empty())
        {
            return;
        }

        u32 offsetTime = time - GetTimestampUntilSequence(cinematic, time);
        f32 sequenceTimestamp = GetTimestampSequence(cinematic, time);

        u32 rowPosition     = 0;
        u32 rowTarget       = 0;
        u32 rowRoll         = 0;
        u32 rowFov          = 0;

        u32 lastPositionTimestamp   = 0;
        u32 lastTargetTimestamp     = 0;
        u32 lastRollTimestamp       = 0;
        u32 lastFovTimestamp        = 0;

        f32 tPosition   = 0.0f;
        f32 tTarget     = 0.0f;
        f32 tRoll       = 0.0f;
        f32 tFov        = 0.0f;

        for (u32 i = 0; i < splinePosition.data.size(); i++)
        {
            u32 timestamp = static_cast<u32>(splinePosition.data[i].timestamp * sequenceTimestamp);
            if (timestamp <= offsetTime)
            {
                rowPosition = i;
                lastPositionTimestamp = timestamp;
            }
            else
            {
                break;
            }
        }

        for (u32 i = 0; i < splineTarget.data.size(); i++)
        {
            u32 timestamp = static_cast<u32>(splineTarget.data[i].timestamp * sequenceTimestamp);
            if (timestamp <= offsetTime)
            {
                rowTarget = i;
                lastTargetTimestamp = timestamp;
            }
            else
            {
                break;
            }
        }

        for (u32 i = 0; i < splineRoll.data.size(); i++)
        {
            u32 timestamp = static_cast<u32>(splineRoll.data[i].timestamp * sequenceTimestamp);
            if (timestamp <= offsetTime)
            {
                rowRoll = i;
                lastRollTimestamp = timestamp;
            }
            else
            {
                break;
            }
        }

        for (u32 i = 0; i < splineFov.data.size(); i++)
        {
            u32 timestamp = static_cast<u32>(splineFov.data[i].timestamp * sequenceTimestamp);
            if (timestamp <= offsetTime)
            {
                rowFov = i;
                lastFovTimestamp = timestamp;
            }
            else
            {
                break;
            }
        }

        f32 tNow = static_cast<f32>(offsetTime) / sequenceTimestamp;

        if (rowPosition < splinePosition.data.size() - 1)
        {
            f32 tMin = static_cast<f32>(lastPositionTimestamp) / sequenceTimestamp;
            f32 tMax = splinePosition.data[rowPosition + 1].timestamp;

            tPosition = Math::Map(tNow, tMin, tMax, 0.0f, 1.0f);
        }

        if (rowTarget < splineTarget.data.size() - 1)
        {
            f32 tMin = static_cast<f32>(lastTargetTimestamp) / sequenceTimestamp;
            f32 tMax = splineTarget.data[rowTarget + 1].timestamp;

            tTarget = Math::Map(tNow, tMin, tMax, 0.0f, 1.0f);
        }

        if (rowRoll < splineRoll.data.size() - 1)
        {
            f32 tMin = static_cast<f32>(lastRollTimestamp) / sequenceTimestamp;
            f32 tMax = splineRoll.data[rowRoll + 1].timestamp;

            tRoll = Math::Map(tNow, tMin, tMax, 0.0f, 1.0f);
        }

        if (rowFov < splineFov.data.size() - 1)
        {
            f32 tMin = static_cast<f32>(lastFovTimestamp) / sequenceTimestamp;
            f32 tMax = splineFov.data[rowFov + 1].timestamp;

            tFov = Math::Map(tNow, tMin, tMax, 0.0f, 1.0f);
        }

        // Camera movement
        {

            if (splinePosition.data.size() == 1)
            {
                position = CoordinateSpaces::SplineSpaceToWorld(cinematic.position, cinematic.rotation, splinePosition.data[0].point);
            }
            else
            {
                if (rowPosition < splinePosition.data.size() - 1)
                {
                    position = CoordinateSpaces::SplineSpaceToWorld(cinematic.position, cinematic.rotation, data.Position.Interpolation4D(tPosition, rowPosition));
                }
            }

            if (splineTarget.data.size() == 1)
            {
                target = CoordinateSpaces::SplineSpaceToWorld(cinematic.position, cinematic.rotation, splineTarget.data[0].point);
            }
            else
            {
                if (rowTarget < splineTarget.data.size() - 1)
                {
                    target = CoordinateSpaces::SplineSpaceToWorld(cinematic.position, cinematic.rotation, data.Target.Interpolation4D(tTarget, rowTarget));
                }
            }

            if (splineRoll.data.size() == 1)
            {
                roll = glm::degrees(splineRoll.data[0].point);
            }
            else
            {
                if (rowRoll < splineRoll.data.size() - 1)
                {
                    roll = glm::degrees(data.Roll.Interpolation2D(tRoll, rowRoll));
                }
            }

            if (splineFov.data.size() == 1)
            {
                fov = glm::degrees(splineFov.data[0].point);
            }
            else
            {
                if (rowFov < splineFov.data.size() - 1)
                {
                    fov = glm::degrees(data.Fov.Interpolation2D(tFov, rowFov));
                }
            }
        }
    }

    u32 CameraEditor::GetCurrentSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time)
    {
        u32 result = 0;
        u32 total = 0;

        for (const auto& sequence : cinematic.sequences)
        {
            total += sequence.timestamp;
            if (time >= total)
            {
                result++;
            }
        }

        result = static_cast<u32>(Math::Clamp(static_cast<i32>(result), 0u, Spline::SplinePath::NUM_SEQUENCES - 1u));
        return result;
    }

    f32 CameraEditor::GetTimestampSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time)
    {
        u32 sequence = GetCurrentSequence(cinematic, time);
        u32 result = cinematic.sequences[sequence].timestamp;
        return static_cast<f32>(result);
    }

    u32 CameraEditor::GetTimestampUntilSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time)
    {
        u32 sequence = GetCurrentSequence(cinematic, time);
        u32 result = 0;

        for (u32 i = 0; i < sequence; i++)
        {
            result += cinematic.sequences[i].timestamp;
        }

        return result;
    }

    /* END STATIC */

    void CameraEditor::DrawInterface()
    {
        EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
        entt::registry& registry = *registries->gameRegistry;
        entt::registry::context& ctx = registry.ctx();

        auto& cinematicDB = ctx.at<ECS::Singletons::CinematicDB>();
        const std::vector<std::string>& cinematicNames = cinematicDB.cinematicNames;

        auto& splineDataDB = ctx.at<ECS::Singletons::SplineDataDB>();

        u32 numCinematic = cinematicDB.entries.data.size();
        u32 numSplineData = splineDataDB.entries.data.size();

        static u32 currentCinematic = 0;

        static std::string dummyString;
        static const std::string* previewCinematic = &dummyString;
        // Cinematic comboBox
        {
            ImGui::Text("Select a Cinematic");
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##cinematicList", previewCinematic->c_str()))
            {
                for (u32 i = 0; i < numCinematic; i++)
                {
                    u32 id = cinematicDB.entries.data[i].id;
                    const std::string& name = cinematicNames[i];

                    bool isSelected = id == currentCinematic;

                    if (ImGui::Selectable(name.c_str(), &isSelected))
                    {
                        currentCinematic = id;
                        previewCinematic = &name;

                        _currentCinematic = cinematicDB.entries.data[i];
                        LoadCinematic();

                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        }

        ImGui::BeginDisabled(currentCinematic == 0);

        DrawSelector();
        DrawCinematic(splineDataDB);
        DrawDisplaySettings();
        DrawDataInformation();

        ImGui::EndDisabled();
    }

    void CameraEditor::DrawSelector()
    {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f ,0.4f, 1.0f));
        if (Util::Imgui::BeginGroupPanel("Selector [DEBUG]"))
        {
            f32 width = ImGui::GetContentRegionAvail().x;

            u32 numPositionsPoints = _splineData[_currentSequence].Position.GetSize();
            u32 numTargetPoints = _splineData[_currentSequence].Target.GetSize();

            u32 selectorSplineType = static_cast<u32>(_currentSelection.splineType);
            u32 selectorSplinePoint = static_cast<u32>(_currentSelection.splinePointType);

            std::string splineStr = "Spline";
            std::string pointTypeStr = "Point Type";
            std::string pointStr = "Point";
            std::string timestampStr = "Timestamp";
            std::string controlModeStr = "Control Mode";
            std::string operationStr = "Operation";

            f32 size = 0.0f;
            size = std::max(size, ImGui::CalcTextSize(splineStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(pointTypeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(pointStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(timestampStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(controlModeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(operationStr.c_str()).x);
            size += 8.0f; // padding

            ImGui::Checkbox("Draw Line Of View", &_drawLineOfView);
            if (_drawLineOfView)
            {
                ImGui::Checkbox("Play Fake Cinematic", &_playCinematicForLineOfView);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", timestampStr.c_str()); ImGui::SameLine(size);
                ImGui::PushItemWidth(width - size);

                i32 min = 0;
                i32 max = static_cast<i32>(GetTotalTimestamp());

                ImGui::SliderInt("##sliderLineOfView", &_lineOfViewTime, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopItemWidth();
            }

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", splineStr.c_str()); ImGui::SameLine(size);
            ImGui::PushItemWidth(width - size);
            if (ImGui::BeginCombo("##splineTypeSelector", SplineTypeName[selectorSplineType].c_str()))
            {
                for (u32 i = 0; i < static_cast<u32>(SplineSelector::SplineType::COUNT); i++)
                {
                    bool isSelected = i == selectorSplineType;

                    if (ImGui::Selectable(SplineTypeName[i].c_str(), isSelected))
                    {
                        _currentSelection.splineType = static_cast<SplineSelector::SplineType>(i);
                        _currentSelection.pointSelected = 0;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", pointTypeStr.c_str()); ImGui::SameLine(size);
            ImGui::PushItemWidth(width - size);
            if (ImGui::BeginCombo("##splinePointTypeSelector", SplinePointTypeName[selectorSplinePoint].c_str()))
            {
                for (u32 i = 0; i < static_cast<u32>(SplineSelector::SplinePointType::COUNT); i++)
                {
                    bool isSelected = i == selectorSplinePoint;

                    if (ImGui::Selectable(SplinePointTypeName[i].c_str(), isSelected))
                    {
                        _currentSelection.splinePointType = static_cast<SplineSelector::SplinePointType>(i);
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            i32 min = 0;
            i32 max = 0;
            switch (_currentSelection.splineType)
            {
                case SplineSelector::SplineType::Position:
                {
                    max = static_cast<i32>(numPositionsPoints - 1u);
                    break;
                }
                case SplineSelector::SplineType::Target:
                {
                    max = static_cast<i32>(numTargetPoints - 1u);
                    break;
                }
                default:
                {
                    Util::Imgui::EndGroupPanel();
                    ImGui::PopStyleColor();
                    return;
                }
            }

            if (max < 0)
                max = 0;

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", pointStr.c_str()); ImGui::SameLine(size);
            ImGui::PushItemWidth(width - size);
            ImGui::SliderInt("##selectorCurrentPoint", &_currentSelection.pointSelected, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopItemWidth();

            // Timestamp for current selected point
            {
                SplineData& data = _splineData[_currentSequence];
                u32 index = static_cast<u32>(_currentSelection.pointSelected);

                if (_currentSelection.splineType == SplineSelector::SplineType::Position)
                {
                    Spline::Spline4D& spline = data.Position.GetSpline4D();
                    if (index > 0 && index < spline.data.size() - 1)
                    {
                        f32 timestampMin = spline.data[index - 1].timestamp;
                        f32 timestampMax = spline.data[index + 1].timestamp;

                        f32& timestamp = spline.data[index].timestamp;
                        u32 timestampToMS = static_cast<u32>(GetTimestampSequence(_currentCinematic, _cinematicTime) * timestamp);

                        bool isDirty = false;

                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%s", timestampStr.c_str()); ImGui::SameLine(size);
                        ImGui::PushItemWidth(width - size);
                        isDirty |= ImGui::SliderFloat("##timestampPositionPoint", &timestamp, timestampMin, timestampMax, "%.6f", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::PopItemWidth();
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%d ms", timestampToMS);
                            ImGui::EndTooltip();
                        }

                        if (isDirty)
                        {
                            data.Position.MarkAsDirty();
                        }
                    }
                }
                else if (_currentSelection.splineType == SplineSelector::SplineType::Target)
                {
                    Spline::Spline4D& spline = data.Target.GetSpline4D();
                    if (index > 0 && index < spline.data.size() - 1)
                    {
                        f32 timestampMin = spline.data[index - 1].timestamp;
                        f32 timestampMax = spline.data[index + 1].timestamp;

                        f32& timestamp = spline.data[index].timestamp;
                        u32 timestampToMS = static_cast<u32>(GetTimestampSequence(_currentCinematic, _cinematicTime) * timestamp);

                        bool isDirty = false;

                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%s", timestampStr.c_str()); ImGui::SameLine(size);
                        ImGui::PushItemWidth(width - size);
                        isDirty |= ImGui::SliderFloat("##timestampPositionPoint", &timestamp, timestampMin, timestampMax, "%.6f", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::PopItemWidth();
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%d ms", timestampToMS);
                            ImGui::EndTooltip();
                        }

                        if (isDirty)
                        {
                            data.Target.MarkAsDirty();
                        }
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Options");

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", controlModeStr.c_str()); ImGui::SameLine(size);
            ImGui::PushItemWidth(width - size);
            if (ImGui::BeginCombo("##splineControlMode", SplineControlModeName[static_cast<u32>(_currentControlMode)].c_str()))
            {
                for (u32 i = 0; i < static_cast<u32>(SplineControlMode::COUNT); i++)
                {
                    bool isSelected = i == static_cast<u32>(_currentControlMode);

                    if (ImGui::Selectable(SplineControlModeName[i].c_str(), isSelected))
                    {
                        _currentControlMode = static_cast<SplineControlMode>(i);
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", operationStr.c_str()); ImGui::SameLine(size);
            ImGui::PushItemWidth(width - size);
            if (ImGui::BeginCombo("##splineOperationSelection", SplineOperationName[_currentOperation].c_str()))
            {
                for (u32 i = 0; i < 3; i++)
                {
                    bool isSelected = i == _currentOperation;

                    if (ImGui::Selectable(SplineOperationName[i].c_str(), isSelected))
                    {
                        _currentOperation = i;
                        _currentSelection.operation = _selectorOperation[_currentOperation];
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

        }
        Util::Imgui::EndGroupPanel();
        ImGui::PopStyleColor();
    }

    void CameraEditor::DrawCinematic(ECS::Singletons::SplineDataDB& splineDataDB)
    {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        if (Util::Imgui::BeginGroupPanel("Cinematic"))
        {
            if (ImGui::Button("Play"))
            {
                _cinematicTime = 0.0f;
                _isStarting = true;
            }
            ImGui::SameLine();

            ImGui::BeginDisabled(_cinematicTime == 0);
            std::string secondaryButton = _isStarting ? "Pause" : "Resume";
            if (ImGui::Button(secondaryButton.c_str()))
            {
                _isStarting = !_isStarting;
            }
            ImGui::EndDisabled();

            ImGui::Text("Total timestamp: %d ms", GetTotalTimestamp());
            ImGui::Text("Current time: %d ms", _cinematicTime);
            ImGui::Text("Current sequence: %d", GetCurrentSequence(_currentCinematic, _cinematicTime));

            if (Util::Imgui::BeginGroupPanel("Origin"))
            {
                f32 internalWidth = ImGui::GetContentRegionAvail().x;

                if (ImGui::Button("Teleport to origin"))
                {
                    ECS::Util::CameraUtil::CenterOnObject(_currentCinematic.position, 20.f);
                }

                bool isDirty = false;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Rotation");
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##Rotation", _currentCinematic.rotation, ImVec4(0.686f, 0.478f, 0.773f, 1.0f), internalWidth, 1.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Position");
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##Position X", _currentCinematic.position.x, ImVec4(1.0f, 0.0f, 0.0f, 1.0f), internalWidth, 1.0f);
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##Position Y", _currentCinematic.position.y, ImVec4(0.0f, 1.0f, 0.0f, 1.0f), internalWidth, 1.0f);
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##Position Z", _currentCinematic.position.z, ImVec4(0.0f, 0.0f, 1.0f, 1.0f), internalWidth, 1.0f);

                if (isDirty)
                {
                    MarkAllSplineAsDirty();
                }
            }
            Util::Imgui::EndGroupPanel();

            if (Util::Imgui::BeginGroupPanel("Sequences"))
            {
                f32 internalWidth = ImGui::GetContentRegionAvail().x;

                static std::string previewSequenceName[8] = {
                    "Sequence 0", "Sequence 1", "Sequence 2", "Sequence 3",
                    "Sequence 4", "Sequence 5", "Sequence 6", "Sequence 7"
                };

                // Sequence ComboBox
                {
                    ImGui::PushItemWidth(internalWidth);
                    if (ImGui::BeginCombo("##sequenceList", previewSequenceName[_currentSequence].c_str()))
                    {
                        for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
                        {
                            bool isSelected = i == _currentSequence;

                            if (ImGui::Selectable(previewSequenceName[i].c_str(), isSelected))
                            {
                                _currentSequence = i;
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                }

                ImGui::Separator();

                DB::Client::Definitions::Cinematic::Sequence& sequence = _currentCinematic.sequences[_currentSequence];

                std::string timestampStr = "Timestamp: ";
                f32 size = ImGui::CalcTextSize(timestampStr.c_str()).x + 8.0f;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", timestampStr.c_str()); ImGui::SameLine(size);
                ImGui::PushItemWidth(internalWidth - size);
                ImGui::SliderInt("##sequenceTimestamp", reinterpret_cast<int*>(&sequence.timestamp), 0, 180000, "%d", ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopItemWidth();

                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f ,0.4f, 1.0f));
                if (Util::Imgui::BeginGroupPanel("Splines"))
                {
                    f32 internalSubWidth = ImGui::GetContentRegionAvail().x;
                    ImVec2 buttonSize(internalSubWidth, 42);

                    ImGui::BeginGroup(); // Position
                    {
                        bool splineExist = sequence.positionSpline > 0;

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImVec2 cursorPosition = ImGui::GetCursorPos();
                        ImVec2 headerTextPosition = ImVec2(cursorPosition.x + 8.0f, cursorPosition.y + 2.0f);
                        ImVec2 pathTextPosition = ImVec2(headerTextPosition.x, headerTextPosition.y + buttonSize.y / 2);
                        ImGui::Button("##", ImVec2(buttonSize.x, (splineExist) ? buttonSize.y : buttonSize.y / 2));
                        ImVec2 finalPosition = ImGui::GetCursorPos();

                        // PUT EVENT ON BUTTON HERE

                        ImGui::PopStyleVar();

                        ImGui::SetCursorPos(headerTextPosition);
                        ImGui::Text("Position");
                        if (splineExist)
                        {
                            ImGui::SetCursorPos(pathTextPosition);
                            ImGui::Text(" - %s", splineDataDB.splineEntryIDToPath[sequence.positionSpline].c_str());
                            DrawComboBoxInterpolation(finalPosition, internalSubWidth, _splineData[_currentSequence].Position, "##interpolationPosition");
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::Separator();

                    ImGui::BeginGroup(); // Target
                    {
                        bool splineExist = sequence.targetSpline > 0;

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImVec2 cursorPosition = ImGui::GetCursorPos();
                        ImVec2 headerTextPosition = ImVec2(cursorPosition.x + 8.0f, cursorPosition.y + 2.0f);
                        ImVec2 pathTextPosition = ImVec2(headerTextPosition.x, headerTextPosition.y + buttonSize.y / 2);
                        ImGui::Button("##", ImVec2(buttonSize.x, (splineExist) ? buttonSize.y : buttonSize.y / 2));
                        ImVec2 finalPosition = ImGui::GetCursorPos();

                        // PUT EVENT ON BUTTON HERE

                        ImGui::PopStyleVar();

                        ImGui::SetCursorPos(headerTextPosition);
                        ImGui::Text("Target");
                        if (splineExist)
                        {
                            ImGui::SetCursorPos(pathTextPosition);
                            ImGui::Text(" - %s", splineDataDB.splineEntryIDToPath[sequence.targetSpline].c_str());
                            DrawComboBoxInterpolation(finalPosition, internalSubWidth, _splineData[_currentSequence].Target, "##interpolationTarget");
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::Separator();

                    ImGui::BeginGroup(); // Roll
                    {
                        bool splineExist = sequence.rollSpline > 0;

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImVec2 cursorPosition = ImGui::GetCursorPos();
                        ImVec2 headerTextPosition = ImVec2(cursorPosition.x + 8.0f, cursorPosition.y + 2.0f);
                        ImVec2 pathTextPosition = ImVec2(headerTextPosition.x, headerTextPosition.y + buttonSize.y / 2);
                        ImGui::Button("##", ImVec2(buttonSize.x, (splineExist) ? buttonSize.y : buttonSize.y / 2));
                        ImVec2 finalPosition = ImGui::GetCursorPos();

                        // PUT EVENT ON BUTTON HERE

                        ImGui::PopStyleVar();

                        ImGui::SetCursorPos(headerTextPosition);
                        ImGui::Text("Roll");
                        if (splineExist)
                        {
                            ImGui::SetCursorPos(pathTextPosition);
                            ImGui::Text(" - %s", splineDataDB.splineEntryIDToPath[sequence.rollSpline].c_str());
                            DrawComboBoxInterpolation(finalPosition, internalSubWidth, _splineData[_currentSequence].Roll, "##interpolationRoll");
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::Separator();

                    ImGui::BeginGroup(); // Fov
                    {
                        bool splineExist = sequence.fovSpline > 0;

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImVec2 cursorPosition = ImGui::GetCursorPos();
                        ImVec2 headerTextPosition = ImVec2(cursorPosition.x + 8.0f, cursorPosition.y + 2.0f);
                        ImVec2 pathTextPosition = ImVec2(headerTextPosition.x, headerTextPosition.y + buttonSize.y / 2);
                        ImGui::Button("##", ImVec2(buttonSize.x, (splineExist) ? buttonSize.y : buttonSize.y / 2));
                        ImVec2 finalPosition = ImGui::GetCursorPos();

                        // PUT EVENT ON BUTTON HERE

                        ImGui::PopStyleVar();

                        ImGui::SetCursorPos(headerTextPosition);
                        ImGui::Text("Fov");
                        if (splineExist)
                        {
                            ImGui::SetCursorPos(pathTextPosition);
                            ImGui::Text(" - %s", splineDataDB.splineEntryIDToPath[sequence.fovSpline].c_str());
                            DrawComboBoxInterpolation(finalPosition, internalSubWidth, _splineData[_currentSequence].Fov, "##interpolationFov");
                        }
                    }
                    ImGui::EndGroup();
                }
                Util::Imgui::EndGroupPanel();
                ImGui::PopStyleColor();

            }
            Util::Imgui::EndGroupPanel();
        }
        Util::Imgui::EndGroupPanel();
        ImGui::PopStyleColor();
    }

    void CameraEditor::DrawDisplaySettings()
    {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f ,0.4f, 1.0f));
        if (Util::Imgui::BeginGroupPanel("Display Settings"))
        {
            f32 width = ImGui::GetContentRegionAvail().x;

            std::string stepStr = "Step";
            std::string widthStr = "Width";
            std::string rotationStr = "Rotation";
            std::string radiusStr = "Radius";
            std::string longitudeStr = "Longitude";
            std::string latitudeStr = "Latitude";
            std::string segmentStr = "Segment";
            std::string accelerationSlow = "Slow";
            std::string accelerationHigh = "High";
            std::string accelerationReference = "Reference";
            std::string accelerationPercent = "Percent";

            f32 size = 0.0f;
            size = std::max(size, ImGui::CalcTextSize(stepStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(widthStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(rotationStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(radiusStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(longitudeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(latitudeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(segmentStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(accelerationSlow.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(accelerationHigh.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(accelerationReference.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(accelerationPercent.c_str()).x);
            size += 8.0f; // padding

            bool isDirty = false;

            isDirty |= ImGui::Checkbox("Show Acceleration Color", &_drawAcceleration);
            isDirty |= ImGui::Checkbox("Draw Position path", &_drawPosition);
            isDirty |= ImGui::Checkbox("Draw Target path", &_drawTarget);

            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", stepStr.c_str()); ImGui::SameLine(size);
            Util::Imgui::DrawColoredRectAndDragI32("##Step count", _stepBetweenEachPoint, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);

            ImGui::Separator();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Sphere");

            ImGui::Text("%s", radiusStr.c_str()); ImGui::SameLine(size);
            isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##sRadius", _sphereRadius, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size, 1.0f);
            ImGui::Text("%s", longitudeStr.c_str()); ImGui::SameLine(size);
            isDirty |= Util::Imgui::DrawColoredRectAndDragI32("##sLongitude", _sphereLongitude, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);
            ImGui::Text("%s", latitudeStr.c_str()); ImGui::SameLine(size);
            isDirty |= Util::Imgui::DrawColoredRectAndDragI32("##sLatitude", _sphereLatitude, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);

            ImGui::Separator();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Path");

            ImGui::Text("%s", widthStr.c_str()); ImGui::SameLine(size);
            isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##pWidth", _pathWidth, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size, 1.0f);

            ImGui::Separator();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Acceleration Color");

            ImGui::Text("%s", accelerationSlow.c_str()); ImGui::SameLine(size);
            ImGui::PushStyleColor(ImGuiCol_Button, _accelerationColorSlow);
            if (ImGui::Button("##accelerationColorSlowPreview", ImVec2(width - size, 0.0f)))
            {
                ImGui::OpenPopup("Acceleration Slow Color Picker");
                _savedAccelerationColorSlow = _accelerationColorSlow;
            }
            ImGui::PopStyleColor();

            ImGui::Text("%s", accelerationHigh.c_str()); ImGui::SameLine(size);
            ImGui::PushStyleColor(ImGuiCol_Button, _accelerationColorHigh);
            if (ImGui::Button("##accelerationColorHighPreview", ImVec2(width - size, 0.0f)))
            {
                ImGui::OpenPopup("Acceleration High Color Picker");
                _savedAccelerationColorHigh = _accelerationColorHigh;
            }
            ImGui::PopStyleColor();

            if (ImGui::BeginPopup("Acceleration Slow Color Picker"))
            {
                f32 internalWidth = ImGui::GetContentRegionAvail().x;

                if (ImGui::Button("Reset", ImVec2(internalWidth, 0.0f)))
                {
                    _accelerationColorSlow = _savedAccelerationColorSlow;
                }

                f32* color = glm::value_ptr(_accelerationColorSlow);
                isDirty |= ImGui::ColorPicker4("##accelerationSlow", color, ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha);
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Acceleration High Color Picker"))
            {
                f32 internalWidth = ImGui::GetContentRegionAvail().x;

                if (ImGui::Button("Reset", ImVec2(internalWidth, 0.0f)))
                {
                    _accelerationColorHigh = _savedAccelerationColorHigh;
                }

                f32* color = glm::value_ptr(_accelerationColorHigh);
                isDirty |= ImGui::ColorPicker4("##accelerationHigh", color, ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha);
                ImGui::EndPopup();
            }

            isDirty |= ImGui::Checkbox("Use Reference", &_useReference);

            if (_useReference)
            {
                ImGui::Text("%s", accelerationReference.c_str()); ImGui::SameLine(size);
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##pReferenceAcceleration", _referenceAcceleration, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size, 2.0f);

                ImGui::Text("%s", accelerationPercent.c_str()); ImGui::SameLine(size);
                isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##pPercentAcceleration", _percentAcceleration, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size, 0.0005f);

                f32 minRange = _referenceAcceleration - (_referenceAcceleration * _percentAcceleration);
                f32 maxRange = _referenceAcceleration + (_referenceAcceleration * _percentAcceleration);

                ImGui::Text("Range: [%.1f, %.1f]", minRange, maxRange);
            }

            // Update Step
            for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
            {
                SplineData& data = _splineData[i];

                if (data.Position.Step() != _stepBetweenEachPoint)
                    data.Position.SetStep(_stepBetweenEachPoint);

                if (data.Target.Step() != _stepBetweenEachPoint)
                    data.Target.SetStep(_stepBetweenEachPoint);

                if (data.Roll.Step() != _stepBetweenEachPoint)
                    data.Roll.SetStep(_stepBetweenEachPoint);

                if (data.Fov.Step() != _stepBetweenEachPoint)
                    data.Fov.SetStep(_stepBetweenEachPoint);
            }

            // Update Display settings
            if (isDirty)
            {
                MarkAllSplineAsDirty();
            }
        }
        Util::Imgui::EndGroupPanel();
        ImGui::PopStyleColor();
    }

    void CameraEditor::DrawDataInformation()
    {

    }

    void CameraEditor::DrawComboBoxInterpolation(ImVec2 cursorPosition, f32 width, Spline::SplinePath& spline, const std::string& id)
    {
        std::string interpolationStr = "Interpolation: ";
        f32 size = ImGui::CalcTextSize(interpolationStr.c_str()).x + 8.0f;

        ImGui::SetCursorPos(cursorPosition);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", interpolationStr.c_str());
        ImGui::SameLine(size);
        ImGui::PushItemWidth(width - size);

        u32 interpolationIndex = static_cast<u32>(spline.GetInterpolationType());
        if (ImGui::BeginCombo(id.c_str(), Spline::InterpolationName[interpolationIndex].c_str()))
        {
            /// @TODO: Right now I just handle control spline interpolation
            for (u32 i = 0; i < static_cast<u32>(Spline::InterpolationType::BSpline); i++)
            {
                bool isSelected = i == interpolationIndex;

                if (ImGui::Selectable(Spline::InterpolationName[i].c_str(), isSelected))
                {
                    spline.SetInterpolationType(static_cast<Spline::InterpolationType>(i));
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
    }

    bool CameraEditor::DrawGizmo(ECS::Components::Camera& camera)
    {
        if (_currentSelection.splineType == SplineSelector::SplineType::None)
            return false;

        if (_currentSelection.splineType == SplineSelector::SplineType::Position)
        {
            Spline::SplinePath& position = _splineData[_currentSequence].Position;
            return ApplyGizmo(camera, position);
        }
        else if (_currentSelection.splineType == SplineSelector::SplineType::Target)
        {
            Spline::SplinePath& target = _splineData[_currentSequence].Target;
            return ApplyGizmo(camera, target);
        }

        return false;
    }

    bool CameraEditor::ApplyGizmo(ECS::Components::Camera& camera, Spline::SplinePath& splinePath)
    {
        Spline::Spline4D& spline = splinePath.GetSpline4D();
        vec3& point = spline.data[_currentSelection.pointSelected].point;
        vec3& in = spline.controls[_currentSelection.pointSelected].in;
        vec3& out = spline.controls[_currentSelection.pointSelected].out;

        vec3 pointDirection;
        vec3 rotation = vec3{ 0.0f };

        if (_currentSelection.splinePointType == SplineSelector::SplinePointType::Point)
        {
            vec3 savedPoint = point;
            bool isDirty = ComputeGizmo(camera, splinePath, point, rotation);

            if (isDirty)
            {
                if (_currentControlMode != SplineControlMode::Free)
                {
                    vec3 offset = point - savedPoint;
                    in += offset;
                    out += offset;
                }
            }

            return isDirty;
        }
        else
        {
            if (_currentSelection.splinePointType == SplineSelector::SplinePointType::In)
            {
                bool isDirty = ComputeGizmo(camera, splinePath, in, rotation);

                if (isDirty)
                {
                    if (_currentControlMode == SplineControlMode::Aligned)
                    {
                        f32 distance = glm::length(out - point);
                        vec3 direction = glm::normalize(in - point);
                        out = point - distance * direction;
                    }
                    else if (_currentControlMode == SplineControlMode::Mirrored)
                    {
                        f32 distance = glm::length(in - point);
                        vec3 direction = glm::normalize(in - point);
                        out = point - distance * direction;
                    }
                }

                return isDirty;
            }
            else if (_currentSelection.splinePointType == SplineSelector::SplinePointType::Out)
            {
                bool isDirty = ComputeGizmo(camera, splinePath, out, rotation);

                if (isDirty)
                {
                    if (_currentControlMode == SplineControlMode::Aligned)
                    {
                        f32 distance = glm::length(in - point);
                        vec3 direction = glm::normalize(out - point);
                        in = point - distance * direction;
                    }
                    else if (_currentControlMode == SplineControlMode::Mirrored)
                    {
                        f32 distance = glm::length(out - point);
                        vec3 direction = glm::normalize(out - point);
                        in = point - distance * direction;
                    }
                }

                return isDirty;
            }
        }

        return false;
    }

    bool CameraEditor::ComputeGizmo(ECS::Components::Camera& camera, Spline::SplinePath& spline, vec3& point, vec3& rotation)
    {
        point = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, point);

        vec3 scale(1.0f);

        mat4x4& viewMatrix = camera.worldToView;
        mat4x4& projMatrix = camera.viewToClip;

        auto operation = _currentSelection.operation;
        auto mode = ImGuizmo::MODE::WORLD;

        if (operation == ImGuizmo::OPERATION::ROTATE)
             mode = ImGuizmo::MODE::WORLD;

        mat4x4 matrix;
        f32* instanceMatrixPtr = glm::value_ptr(matrix);
        ImGuizmo::RecomposeMatrixFromComponents(&point.x, &rotation.x, &scale.x, instanceMatrixPtr);

        bool isDirty = ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), operation, mode, instanceMatrixPtr, nullptr);

        if (isDirty)
        {
            ImGuizmo::DecomposeMatrixToComponents(instanceMatrixPtr, glm::value_ptr(point), glm::value_ptr(rotation), glm::value_ptr(scale));
            spline.MarkAsDirty();
        }

        point = CoordinateSpaces::WorldSpaceToSpline(_currentCinematic.position, _currentCinematic.rotation, point);
        return isDirty;
    }

    void CameraEditor::DrawInViewport(DebugRenderer* debugRenderer)
    {
        // Draw .cdb offset & rotation
        {
            vec3 offsetXMin = _currentCinematic.position - vec3(20.0f, 0.0f, 0.0f);
            vec3 offsetXMax = _currentCinematic.position + vec3(20.0f, 0.0f, 0.0f);
            debugRenderer->DrawLine3D(offsetXMin, offsetXMax, Color::Black);

            vec3 offsetYMin = _currentCinematic.position - vec3(0.0f, 20.0f, 0.0f);
            vec3 offsetYMax = _currentCinematic.position + vec3(0.0f, 20.0f, 0.0f);
            debugRenderer->DrawLine3D(offsetYMin, offsetYMax, Color::Black);

            vec3 offsetZMin = _currentCinematic.position - vec3(0.0f, 0.0f, 20.0f);
            vec3 offsetZMax = _currentCinematic.position + vec3(0.0f, 0.0f, 20.0f);
            debugRenderer->DrawLine3D(offsetZMin, offsetZMax, Color::Black);

            vec3 rotateDirection(20.0f, 0.0f, 0.0f);
            mat4x4 rotationMatrix = glm::rotate(mat4x4(1.0f), _currentCinematic.rotation, vec3(0.0f, 1.0f, 0.0f));
            rotateDirection = rotationMatrix * vec4(rotateDirection, 1.0f);
            debugRenderer->DrawLine3D(_currentCinematic.position, (_currentCinematic.position + rotateDirection), Color::Red);
        }

        if (_drawLineOfView)
        {
            vec3 position, target;
            f32 roll, fov;

            HandleCinematic(_currentCinematic, _lineOfViewTime, _splineData, position, target, roll, fov);

            std::vector<vec3> empty;
            std::vector<DebugRenderer::DebugVertexSolid3D> cache;
            debugRenderer->GeneratePipe(cache, {position, target}, 0.3f, 0.0f, 6, Color(1.0f, 0.0f, 0.75f), empty);
            debugRenderer->DrawVerticesSolid3D(cache);
        }

        SplineColor colorPosition;
        colorPosition.pathColor = Color::Blue;
        colorPosition.enterColor = Color::PastelBlue;
        colorPosition.valueColor = colorPosition.pathColor;
        colorPosition.exitColor = colorPosition.enterColor;

        SplineColor colorTarget;
        colorTarget.pathColor = Color::Red;
        colorTarget.enterColor = Color::PastelOrange;
        colorTarget.valueColor = colorTarget.pathColor;
        colorTarget.exitColor = colorTarget.enterColor;

        for (u32 i = 0; i < Spline::SplinePath::NUM_SEQUENCES; i++)
        {
            SplineData& data = _splineData[i];
            std::vector<DebugRenderer::DebugVertexSolid3D>& cachePosition = _cacheSplinePosition[i];
            std::vector<DebugRenderer::DebugVertexSolid3D>& cacheTarget = _cacheSplineTarget[i];

            if (!data.Position.Is2DSpline() && _drawPosition)
            {
                DrawCurve(cachePosition, debugRenderer, data.Position, data.Roll, data.Fov, colorPosition, true);
            }

            if (!data.Target.Is2DSpline() && _drawTarget)
            {
                Spline::SplinePath empty;
                DrawCurve(cacheTarget, debugRenderer, data.Target, empty, empty, colorTarget);
            }
        }
    }

    void CameraEditor::DrawCurve(std::vector<DebugRenderer::DebugVertexSolid3D>& cache, DebugRenderer* renderer, Spline::SplinePath& curve, Spline::SplinePath& roll, Spline::SplinePath& fov, SplineColor color, bool isPosition)
    {
        if (_isStarting)
        {
            // like that the position path is not in the middle of the camera
            vec3 yOffset(0.0f, 0.0f, 0.0f);
            if (isPosition)
                yOffset.y -= 1.0f;

            const auto& points = curve.GetInterpolatedStorage4D().Storage();
            if (points.empty())
                return;

            for (auto ite = points.begin(); ite != points.end() - 1; ite++)
            {
                vec3 from   = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, (*ite + yOffset));
                vec3 to     = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, (*(ite + 1) + yOffset));
                renderer->DrawLine3D(from, to, color.pathColor);
            }

            if (!isPosition)
            {
                f32 distance = glm::distance(_currentTarget, ECS::Util::CameraUtil::GetPosition());
                f32 radius = distance / 100.0f;

                std::vector<DebugRenderer::DebugVertexSolid3D> targetSphere;
                renderer->GenerateSphere(targetSphere, _currentTarget, radius, _sphereLongitude, _sphereLatitude, color.pathColor, true);
                renderer->DrawVerticesSolid3D(targetSphere);
            }
            return;
        }

        if (curve.Interpolate())
        {
            cache.clear();

            Spline::InterpolatedStorage<vec3> interpolatedStorage = curve.GetInterpolatedStorage4D();
            std::vector<vec3> points = interpolatedStorage.Storage();
            for (auto& point : points)
            {
                point = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, point);
            }

            std::vector<Color> accelerations;
            if (_drawAcceleration)
            {
                accelerations.reserve(points.size());
                Spline::SplineInformation splineInformation = interpolatedStorage.GetInformation();
                Spline::SplineDataInformation distanceInformation = splineInformation.distance;
                Spline::SplineDataInformation timeInformation = splineInformation.time;

                std::vector<f32> speeds;
                speeds.reserve(points.size());
                f32 minSpeed = std::numeric_limits<f32>::max();
                f32 maxSpeed = std::numeric_limits<f32>::min();
                for (u32 i = 0; i < points.size(); i++)
                {
                    f32 distance = interpolatedStorage.Distance()[i];
                    f32 time = interpolatedStorage.Time()[i];
                    f32 speed = distance / time;
                    minSpeed = std::min(minSpeed, speed);
                    maxSpeed = std::max(maxSpeed, speed);
                    speeds.emplace_back(speed);
                }

                for (u32 i = 0; i < points.size(); i++)
                {
                    f32 speed = speeds[i];

                    f32 rangeMin = minSpeed;
                    f32 rangeMax = maxSpeed;

                    if (_useReference)
                    {
                        rangeMin = _referenceAcceleration - (_referenceAcceleration * _percentAcceleration);
                        rangeMax = _referenceAcceleration + (_referenceAcceleration * _percentAcceleration);
                    }

                    speed = std::min(speed, rangeMax);
                    speed = std::max(speed, rangeMin);

                    f32 tSpeed = Math::Map(speed, rangeMin, rangeMax, 0.0f, 1.0f);
                    vec3 resultColor = Spline::Interpolation::Linear::Lerp(tSpeed, _accelerationColorSlow, _accelerationColorHigh);

                    Color& accelerationColor = accelerations.emplace_back();
                    accelerationColor = Color(resultColor.x, resultColor.y, resultColor.z);
                }
            }

            std::vector<f32> rolls;
            std::vector<f32> fovs;
            if (isPosition)
            {
                if (roll.GetSize() == 1)
                {
                    rolls.emplace_back(roll.GetSpline2D().data[0].point);
                }
                else
                {
                    roll.Interpolate();
                    rolls = roll.GetInterpolatedStorage2D().Storage();
                }

                if (fov.GetSize() == 1)
                {
                    fovs.emplace_back(fov.GetSpline2D().data[0].point);
                }
                else
                {
                    fov.Interpolate();
                    fovs = fov.GetInterpolatedStorage2D().Storage();
                }
            }

            if (points.size() >= 2)
            {
                renderer->GenerateRibbon(cache, points, rolls, fovs, _pathWidth, color.pathColor, accelerations, true);

                const auto& spline = curve.GetSpline4D();

                for (i32 i = 0; i < spline.data.size(); i++)
                {
                    vec3 point  = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, spline.data[i].point);
                    vec3 in     = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, spline.controls[i].in);
                    vec3 out    = CoordinateSpaces::SplineSpaceToWorld(_currentCinematic.position, _currentCinematic.rotation, spline.controls[i].out);

                    if (curve.IsInterpolatedWithControl())
                    {
                        std::vector<vec3> empty;
                        if (i > 0)
                            renderer->GeneratePipe(cache, {point, in}, _sphereRadius / 10.0f, 0.0f, 4, Color::Black, empty);

                        if (i < spline.data.size() - 1)
                            renderer->GeneratePipe(cache, {point, out}, _sphereRadius / 10.0f, 0.0f, 4, Color::Black, empty);

                        if (i > 0)
                            renderer->GenerateSphere(cache, in, _sphereRadius, _sphereLongitude, _sphereLatitude, color.enterColor, true);

                        if (i < spline.data.size() - 1)
                            renderer->GenerateSphere(cache, out, _sphereRadius, _sphereLongitude, _sphereLatitude, color.exitColor, true);
                    }
                    renderer->GenerateSphere(cache, point, _sphereRadius, _sphereLongitude, _sphereLatitude, color.valueColor, true);
                }
            }

            _redraw++;
        }

        renderer->DrawVerticesSolid3D(cache);
    }
}