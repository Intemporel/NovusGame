#include "CameraEditor.h"

#include <Base/Math/Color.h>
#include <Base/Memory/FileReader.h>

#include <Game/Util/ImguiUtil.h>
#include <Game/Util/ServiceLocator.h>
#include <Game/Application/EnttRegistries.h>
#include <Game/Rendering/GameRenderer.h>
#include <Game/ECS/Util/CameraUtil.h>
#include <Game/ECS/Singletons/ActiveCamera.h>
#include <Game/ECS/Components/Transform.h>
#include <Game/ECS/Components/Camera.h>

#include <entt/entt.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imguizmo/ImGuizmo.h>

#include <filesystem>

namespace Editor
{
    CameraEditor::CameraEditor()
        : BaseEditor(GetName(), false)
    {
        Spline::SplineArguments arguments4D;
        arguments4D.splineType = Spline::SplineType::SplineType_4D;
        arguments4D.interpolationType = Spline::InterpolationType::Bezier;

        Spline::SplineArguments arguments2D;
        arguments2D.splineType = Spline::SplineType::SplineType_2D;
        arguments2D.interpolationType = Spline::InterpolationType::Bezier;

        _curvePosition = new Spline::SplinePath(arguments4D);
        _curveTarget = new Spline::SplinePath(arguments4D);
        _curveRoll = new Spline::SplinePath(arguments2D);
        _curveFOV = new Spline::SplinePath(arguments2D);
    }

    void CameraEditor::DrawImGui()
    {
        EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
        entt::registry& registry = *registries->gameRegistry;
        entt::registry::context& ctx = registry.ctx();

        GameRenderer* gameRenderer = ServiceLocator::GetGameRenderer();
        DebugRenderer* debugRenderer = gameRenderer->GetDebugRenderer();
        auto& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
        ECS::Components::Transform& cameraTransform = registry.get<ECS::Components::Transform>(activeCamera.entity);
        ECS::Components::Camera& camera = registry.get<ECS::Components::Camera>(activeCamera.entity);

        if (ImGui::Begin(GetName(), &IsVisible()))
        {
            ReceiveDrop();
            DrawHeader();

            if (_currentPointSelected >= 0 && _currentPointSelected < _curvePosition->GetSize())
            {
                DrawGizmo(camera);
            }

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
                    OpenFile(file->string());
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void CameraEditor::ConvertOldCameraToNovus()
    {
        if (!_model.cameras.empty())
        {
            Spline::Spline4D pointsPosition = { };
            Spline::Spline4D pointsTarget   = { };
            Spline::Spline2D pointsRoll     = { };
            Spline::Spline2D pointsFOV      = { };

            Spline::SplineArguments arguments4D;
            arguments4D.splineType = Spline::SplineType::SplineType_4D;
            arguments4D.interpolationType = Spline::InterpolationType::Bezier;

            Spline::SplineArguments arguments2D;
            arguments2D.splineType = Spline::SplineType::SplineType_2D;
            arguments2D.interpolationType = Spline::InterpolationType::Bezier;

            const auto& camera = _model.cameras[0];
            if (!camera.positions.tracks.empty())
            {
                const auto& positionTrack = camera.positions.tracks[0];
                const auto& targetTrack = camera.targetPositions.tracks[0];
                const auto& rollTrack = camera.roll.tracks[0];
                const auto& fovTrack = camera.fov.tracks[0];

                vec3 camPositionBase = camera.positionBase;
                vec3 camTargetBase = camera.targetPositionBase;

                if (positionTrack.values.size() != positionTrack.timestamps.size() ||
                    targetTrack.values.size() != targetTrack.timestamps.size() ||
                    rollTrack.values.size() != rollTrack.timestamps.size() ||
                    fovTrack.values.size() != fovTrack.timestamps.size())
                {
                    ImGui::EndDragDropTarget();
                    return;
                }

                u32 positionMaxTimestamp = positionTrack.timestamps.back();
                u32 targetMaxTimestamp = positionTrack.timestamps.back();

                // Max timestamp need to be the same for both path
                if (positionMaxTimestamp != targetMaxTimestamp)
                {
                    ImGui::EndDragDropTarget();
                    return;
                }

                _maxTimer = static_cast<i32>(positionMaxTimestamp);

                vec3 dbcPosition(-offset_x, offset_y, offset_z);
                f32 dbcFacing = rotation_rad + glm::pi<f32>();

                pointsPosition.Clear();
                for (i32 i = 0; i < positionTrack.values.size(); i++)
                {
                    Spline::DataSpline<vec3> point;
                    point.point = SplineSpaceToWorld(dbcPosition, dbcFacing, camPositionBase, positionTrack.values[i].value);
                    point.timestamp = static_cast<f32>(positionTrack.timestamps[i]) / static_cast<f32>(_maxTimer);

                    Spline::ControlSpline<vec3> control;
                    control.in = SplineSpaceToWorld(dbcPosition, dbcFacing, camPositionBase, positionTrack.values[i].tanIn);
                    control.out = SplineSpaceToWorld(dbcPosition, dbcFacing, camPositionBase, positionTrack.values[i].tanOut);

                    pointsPosition.data.push_back(point);
                    pointsPosition.controls.push_back(control);
                }

                pointsTarget.Clear();
                for (i32 i = 0; i < targetTrack.values.size(); i++)
                {
                    Spline::DataSpline<vec3> point;
                    point.point = SplineSpaceToWorld(dbcPosition, dbcFacing, camTargetBase, targetTrack.values[i].value);
                    point.timestamp = static_cast<f32>(targetTrack.timestamps[i]) / static_cast<f32>(_maxTimer);

                    Spline::ControlSpline<vec3> control;
                    control.in = SplineSpaceToWorld(dbcPosition, dbcFacing, camTargetBase, targetTrack.values[i].tanIn);
                    control.out = SplineSpaceToWorld(dbcPosition, dbcFacing, camTargetBase, targetTrack.values[i].tanOut);

                    pointsTarget.data.push_back(point);
                    pointsTarget.controls.push_back(control);
                }

                for (i32 i = 0; i < rollTrack.values.size(); i++)
                {
                    Spline::DataSpline<f32> point;
                    point.point = rollTrack.values[i].value;
                    point.timestamp = static_cast<f32>(rollTrack.timestamps[i]) / static_cast<f32>(_maxTimer);

                    Spline::ControlSpline<f32> control;
                    control.in = rollTrack.values[i].tanIn;
                    control.out = rollTrack.values[i].tanOut;

                    pointsRoll.data.push_back(point);
                    pointsRoll.controls.push_back(control);
                }

                pointsFOV.Clear();
                for (i32 i = 0; i < fovTrack.values.size(); i++)
                {
                    Spline::DataSpline<f32> point;
                    point.point = fovTrack.values[i].value;
                    point.timestamp = static_cast<f32>(fovTrack.timestamps[i]) / static_cast<f32>(_maxTimer);

                    Spline::ControlSpline<f32> control;
                    control.in = fovTrack.values[i].tanIn;
                    control.out = fovTrack.values[i].tanOut;

                    pointsFOV.data.push_back(point);
                    pointsFOV.controls.push_back(control);
                }

                if (!pointsPosition.data.empty())
                {
                    _curvePosition = new Spline::SplinePath(arguments4D, pointsPosition);
                }

                if (!pointsTarget.data.empty())
                {
                    _curveTarget = new Spline::SplinePath(arguments4D, pointsTarget);
                }

                if (!pointsRoll.data.empty())
                {
                    _curveRoll = new Spline::SplinePath(arguments2D, pointsRoll);
                }

                if (!pointsFOV.data.empty())
                {
                    _curveFOV = new Spline::SplinePath(arguments2D, pointsFOV);
                }
            }
        }
    }

    // need to include the base directly to the spline data
    vec3 CameraEditor::SplineSpaceToWorld(const vec3& dbPos, f32 dbFacing, const vec3& base, const vec3& splinePos)
    {
        vec3 offset = base + splinePos;
        const f32 distance = glm::sqrt((offset.x * offset.x) + (offset.z * offset.z));
        f32 angle = glm::atan(offset.x, offset.z) - dbFacing;

        if (angle < 0)
            angle += glm::two_pi<f32>();

        vec3 result = dbPos;
        result.x += distance * sin(angle);
        result.y += offset.y;
        result.z += distance * cos(angle);
        return result;
    }

    // after that, we can remove the -base vector at the end
    vec3 CameraEditor::WorldSpaceToSpline(const vec3& dbPos, f32 dbFacing, const vec3& base, const vec3& worldPos)
    {
        vec3 offset = worldPos - dbPos;
        f32 distance = glm::length(glm::vec2(offset.x, offset.z));
        f32 angle = glm::atan(offset.x, offset.z) + dbFacing;

        if (angle < 0)
            angle += glm::two_pi<f32>();

        vec3 result;
        result.x = distance * glm::sin(angle) - base.x;
        result.y = worldPos.y - base.y;
        result.z = distance * glm::cos(angle) - base.z;
        return result;
    }

    void CameraEditor::OpenFile(const std::filesystem::path& file)
    {
        _file = file.filename().string();
        if (OpenCamera(file.string(), _model))
        {
            DebugHandler::Print("Is opened");
            ConvertOldCameraToNovus();
        }
    }

    bool CameraEditor::OpenCamera(const std::string& path, Model::ComplexModel& output)
    {
        FileReader file(path);
        if (!file.Open())
        {
            DebugHandler::PrintError("ModelLoader : Failed to open CModel file: {0}", path);
            return false;
        }

        size_t fileSize = file.Length();
        std::shared_ptr<Bytebuffer> fileBuffer = Bytebuffer::BorrowRuntime(fileSize);

        file.Read(fileBuffer.get(), fileSize);
        file.Close();

        bool result = Model::ComplexModel::Read(fileBuffer, output);
        return result;
    }

    void CameraEditor::Update(f32 deltaTime)
    {
        if (_isStarting)
        {
            Spline::Spline4D splinePosition = _curvePosition->GetSpline4D();
            Spline::Spline4D splineTarget = _curveTarget->GetSpline4D();
            Spline::Spline2D splineRoll = _curveRoll->GetSpline2D();
            Spline::Spline2D splineFOV = _curveFOV->GetSpline2D();

            if (splinePosition.data.empty() || splineTarget.data.empty())
            {
                _isStarting = false;
                return;
            }

            _cinematicTime += deltaTime;
            _currentTime = static_cast<i32>(_cinematicTime * 1000.0f);

            i32 rowPosition = 0;
            i32 rowTarget = 0;
            i32 rowRoll = 0;
            i32 rowFOV = 0;

            u32 lastPositionTimestamp = 0;
            u32 lastTargetTimestamp = 0;
            u32 lastRollTimestamp = 0;
            u32 lastFOVTimestamp = 0;

            f32 tPosition = 0.0f;
            f32 tTarget = 0.0f;
            f32 tRoll;
            f32 tFOV;

            for (i32 i = 0; i < splinePosition.data.size(); i++)
            {
                u32 timestamp = static_cast<u32>(splinePosition.data[i].timestamp * static_cast<f32>(_maxTimer));
                if (timestamp <= _currentTime)
                {
                    rowPosition = i;
                    lastPositionTimestamp = timestamp;
                }
                else
                {
                    break;
                }
            }

            for (i32 i = 0; i < splineTarget.data.size(); i++)
            {
                u32 timestamp = static_cast<u32>(splineTarget.data[i].timestamp * static_cast<f32>(_maxTimer));
                if (timestamp <= _currentTime)
                {
                    rowTarget = i;
                    lastTargetTimestamp = timestamp;
                }
                else
                {
                    break;
                }
            }

            for (i32 i = 0; i < splineRoll.data.size(); i++)
            {
                u32 timestamp = static_cast<u32>(splineRoll.data[i].timestamp * static_cast<f32>(_maxTimer));
                if (timestamp <= _currentTime)
                {
                    rowRoll = i;
                    lastRollTimestamp = timestamp;
                }
                else
                {
                    break;
                }
            }

            for (i32 i = 0; i < splineFOV.data.size(); i++)
            {
                u32 timestamp = static_cast<u32>(splineFOV.data[i].timestamp * static_cast<f32>(_maxTimer));
                if (timestamp <= _currentTime)
                {
                    rowFOV = i;
                    lastFOVTimestamp = timestamp;
                }
                else
                {
                    break;
                }
            }

            if (rowPosition < splinePosition.data.size() - 1)
            {
                i32 diffTimestamp = _currentTime - static_cast<i32>(lastPositionTimestamp);
                tPosition = static_cast<f32>(diffTimestamp) / static_cast<f32>(splinePosition.data[rowPosition + 1].timestamp * _maxTimer - lastPositionTimestamp);
            }

            if (rowTarget < splineTarget.data.size() - 1)
            {
                i32 diffTimestamp = _currentTime - static_cast<i32>(lastTargetTimestamp);
                tTarget = static_cast<f32>(diffTimestamp) / static_cast<f32>(splineTarget.data[rowTarget + 1].timestamp * _maxTimer - lastTargetTimestamp);
            }

            if (rowRoll < splineRoll.data.size() - 1)
            {
                i32 diffTimestamp = _currentTime - static_cast<i32>(lastRollTimestamp);
                tRoll = static_cast<f32>(diffTimestamp) / static_cast<f32>(splineRoll.data[rowRoll + 1].timestamp * _maxTimer - lastRollTimestamp);
            }

            if (rowFOV < splineFOV.data.size() - 1)
            {
                i32 diffTimestamp = _currentTime - static_cast<i32>(lastFOVTimestamp);
                tFOV = static_cast<f32>(diffTimestamp) / static_cast<f32>(splineFOV.data[rowFOV + 1].timestamp * _maxTimer - lastFOVTimestamp);
            }

            // CAMERA POSITION CALCUL TEST
            {
                EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
                entt::registry& registry = *registries->gameRegistry;
                entt::registry::context& ctx = registry.ctx();

                ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
                ECS::Components::Transform& cameraTransform = registry.get<ECS::Components::Transform>(activeCamera.entity);
                ECS::Components::Camera& camera = registry.get<ECS::Components::Camera>(activeCamera.entity);

                vec3 cameraPos(0.0f);
                vec3 targetPos(0.0f);
                f32 roll = camera.roll;
                f32 fov = camera.fov;

                if (rowPosition < splinePosition.data.size() - 1)
                {
                    cameraPos = _curvePosition->Interpolation4D(tPosition, rowPosition);
                }

                if (rowTarget < splineTarget.data.size() - 1)
                {
                    targetPos = _curveTarget->Interpolation4D(tTarget, rowTarget);
                }

                if (splineRoll.data.size() == 1)
                {
                    roll = glm::degrees(splineRoll.data[0].point);
                }
                else
                {
                    if (rowRoll < splineRoll.data.size() - 1)
                    {
                        roll = glm::degrees(_curveRoll->Interpolation2D(tRoll, rowRoll));
                    }
                }

                if (splineFOV.data.size() == 1)
                {
                    fov = glm::degrees(splineFOV.data[0].point);
                }
                else
                {
                    if (rowFOV < splineFOV.data.size() - 1)
                    {
                        fov = glm::degrees(_curveFOV->Interpolation2D(tFOV, rowFOV));
                    }
                }

                cameraTransform.position = cameraPos;
                ECS::Util::CameraUtil::LookAt(targetPos);

                if (_currentTime >= _maxTimer)
                {
                    cameraTransform.position = splinePosition.data[splinePosition.data.size() - 1].point;
                    //cameraTransform.position.y += 1.0f;
                    ECS::Util::CameraUtil::LookAt(splineTarget.data[splineTarget.data.size() - 1].point);

                    roll = 0.0f; // default;
                    fov = 75.0f; // default

                    _isStarting = false;
                    _currentTime = 0.0f;

                    // THIS IS A TESSSSSSSSST
                    vec3 dbcPosition(-offset_x, offset_y, offset_z);
                    f32 dbcFacing = rotation_rad + glm::pi<f32>();
                    auto base = _model.cameras[0].positionBase;
                    auto track = _model.cameras[0].positions.tracks[0];
                    auto lastPos = track.values[track.values.size() - 1].value;
                    const auto& testPosition = WorldSpaceToSpline(dbcPosition, dbcFacing, base, cameraTransform.position);

                    DebugHandler::Print("TEST X:{0}; Y{1}; Z{2}", testPosition.x, testPosition.y, testPosition.z);
                    DebugHandler::Print("VALL X:{0}; Y{1}; Z{2}", lastPos.x, lastPos.y, lastPos.z);
                }

                camera.roll = roll;
                camera.fov = fov; // NEED TO RESET
                camera.dirtyView = true;

                _currentTarget = targetPos;
            }
        }
    }

    void CameraEditor::DrawHeader()
    {
        ImGui::Text("Current file : %s", _file.data());
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        if (Util::Imgui::BeginGroupPanel("Cinematic"))
        {
            f32 width = ImGui::GetContentRegionAvail().x;

            ImGui::PushItemWidth(width / 2.0f);
            if (ImGui::Button("Play"))
            {
                _cinematicTime = 0.0f;
                _isStarting = true;
            }
            ImGui::SameLine();

            ImGui::BeginDisabled(_cinematicTime == 0.0f);
            std::string secondaryButton = _isStarting ? "Pause" : "Resume";
            if (ImGui::Button(secondaryButton.c_str()))
            {
                _isStarting = !_isStarting;
            }
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            std::string timerStr = "Timer";
            std::string currentTimeStr = "Current Time";
            f32 size = 0.0f;
            size = std::max(size, ImGui::CalcTextSize(timerStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(currentTimeStr.c_str()).x);
            size += 8.0f;

            ImGui::Text("%s", timerStr.c_str()); ImGui::SameLine(size);
            Util::Imgui::DrawColoredRectAndDragI32("##cinematicTimer", _maxTimer, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);
            ImGui::Text("%s", currentTimeStr.c_str()); ImGui::SameLine(size);
            Util::Imgui::DrawColoredRectAndDragI32("##currentTime", _currentTime, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);
            //ImGui::SliderInt("Current Time", &_currentTime, 0, _maxTimer, "%d", ImGuiSliderFlags_AlwaysClamp);

            if (Util::Imgui::BeginGroupPanel("Placement"))
            {
                f32 internalWidth = ImGui::GetContentRegionAvail().x;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Rotation");
                Util::Imgui::DrawColoredRectAndDragF32("##Rotation", rotation_rad, ImVec4(0.686f, 0.478f, 0.773f, 1.0f), internalWidth, 1.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Offset");
                Util::Imgui::DrawColoredRectAndDragF32("##Offset X", offset_x, ImVec4(1.0f, 0.0f, 0.0f, 1.0f), internalWidth, 1.0f);
                Util::Imgui::DrawColoredRectAndDragF32("##Offset Y", offset_y, ImVec4(0.0f, 1.0f, 0.0f, 1.0f), internalWidth, 1.0f);
                Util::Imgui::DrawColoredRectAndDragF32("##Offset Z", offset_z, ImVec4(0.0f, 0.0f, 1.0f, 1.0f), internalWidth, 1.0f);
            }
            Util::Imgui::EndGroupPanel();
        }
        Util::Imgui::EndGroupPanel();
        ImGui::PopStyleColor();

        /*ImGui::PushItemWidth(200);
        if (ImGui::BeginCombo("combo 1", _items[_selectedItem]))
        {
            for (int n = 0; n < _itemCount; n++)
            {
                const bool is_selected = (_selectedItem == n);
                if (ImGui::Selectable(_items[n], is_selected))
                {
                    _selectedItem = n;
                    _curvePosition->SetInterpolationType(static_cast<Spline::InterpolationType>(n + 1));
                    _curveTarget->SetInterpolationType(static_cast<Spline::InterpolationType>(n + 1));
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();*/

        /*if (_selectedItem == 4)
        {
            ImGui::SameLine(0.f, 32.f);
            ImGui::PushItemWidth(100);
            if (ImGui::DragFloat("alpha", &_catmullRomAlpha, 0.01f, 0.0f, 1.f))
            {
                //_curvePosition->SetAlpha(_catmullRomAlpha);
                //_curveTarget->SetAlpha(_catmullRomAlpha);
            }

            ImGui::PopItemWidth();
        }*/

        /*if (!_curvePosition->GetPoints().empty())
        {
            static char* selectedPoint = "";
            ImGui::PushItemWidth(120);
            if (ImGui::BeginCombo("Select a points", selectedPoint))
            {
                for (i32 i = 0; i < _curvePosition->GetPoints().size() * 3; i++)
                {
                    const bool is_selected = (_currentPointSelected == i);
                    std::string name = std::string("item" + std::to_string(i));
                    if (ImGui::Selectable(name.c_str(), is_selected))
                    {
                        _currentPointSelected = i;
                        selectedPoint = name.data();
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        }*/

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

            f32 size = 0.0f;
            size = std::max(size, ImGui::CalcTextSize(stepStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(widthStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(rotationStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(radiusStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(longitudeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(latitudeStr.c_str()).x);
            size = std::max(size, ImGui::CalcTextSize(segmentStr.c_str()).x);
            size += 8.0f; // padding

            bool isDirty = false;

            ImGui::Checkbox("Draw Position path", &_drawPosition);
            ImGui::Checkbox("Draw Target path", &_drawTarget);

            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", stepStr.c_str()); ImGui::SameLine(size);
            Util::Imgui::DrawColoredRectAndDragI32("##Step count", _stepBetweenEachPoint, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size);
            // only for debug
            ImGui::Text("%s", rotationStr.c_str()); ImGui::SameLine(size);
            isDirty |= Util::Imgui::DrawColoredRectAndDragF32("##pRotation", _rotationPerSegment, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), width - size, 0.0005f);

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

            if (_curvePosition->Step() != _stepBetweenEachPoint)
            {
                _curvePosition->SetStep(_stepBetweenEachPoint);
            }
            if (_curveTarget->Step() != _stepBetweenEachPoint)
            {
                _curveTarget->SetStep(_stepBetweenEachPoint);
            }
            if (_curveRoll->Step() != _stepBetweenEachPoint)
            {
                _curveRoll->SetStep(_stepBetweenEachPoint);
            }
            if (_curveFOV->Step() != _stepBetweenEachPoint)
            {
                _curveFOV->SetStep(_stepBetweenEachPoint);
            }

            if (isDirty)
            {
                _curvePosition->MarkAsDirty();
                _curveTarget->MarkAsDirty();
                _curveRoll->MarkAsDirty();
                _curveFOV->MarkAsDirty();
            }
        }
        Util::Imgui::EndGroupPanel();
        ImGui::PopStyleColor();

        if (ImGui::Button("Add position"))
        {
            /*Spline::SplinePoint point;
            point.control.point = cameraTransform.position;

            point.control.enter = point.control.point * (1.f + (f32)((rand() % 10)/100.f));
            point.control.exit  = point.control.point * (1.f - (f32)((rand() % 10)/100.f));

            _curvePosition->AddPoint(point);*/
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            //_curvePosition->Clear();
        }

        ImGui::Text("Redraw %i times", _redraw);
        //ImGui::SameLine(0.f, 32.f);
        //ImGui::Text("There are %zu/%zu points", _curvePosition->GetPoints().size(), _curvePosition->GetPoints().size() * 3);

        /*if (ImGui::Button("Press the button lol"))
        {
            std::vector<vec3> points;
            _curvePosition->GetPointsData(points);
            for (auto point : points)
            {
                DebugHandler::Print("{0}, {1}, {2}", point.x, point.y, point.z);
            }
        }*/
    }

    bool CameraEditor::DrawGizmo(ECS::Components::Camera& camera)
    {
        if (_currentPointSelected < 0 || _currentPointSelected >= _curvePosition->GetSpline4D().data.size())
            return false;

        vec3& position = _curvePosition->GetSpline4D().data[_currentPointSelected].point;
        vec3 rotation(0.0f);
        vec3 scale(1.0f);

        mat4x4& viewMatrix = camera.worldToView;
        mat4x4& projMatrix = camera.viewToClip;

        auto operation = ImGuizmo::OPERATION::TRANSLATE;
        auto mode = ImGuizmo::MODE::WORLD;

        mat4x4 matrix;
        f32* instanceMatrixPtr = glm::value_ptr(matrix);
        ImGuizmo::RecomposeMatrixFromComponents(&position.x, &rotation.x, &scale.x, instanceMatrixPtr);

        bool isDirty = ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), operation, mode, instanceMatrixPtr, nullptr);

        if (isDirty)
        {
            ImGuizmo::DecomposeMatrixToComponents(instanceMatrixPtr, glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));
            _curvePosition->MarkAsDirty();
            _curveTarget->MarkAsDirty();
        }

        return isDirty;
    }

    void CameraEditor::DrawInViewport(DebugRenderer* debugRenderer)
    {
        vec3 dbcPosition(-offset_x, offset_y, offset_z);
        f32 dbcFacing = rotation_rad + glm::pi<f32>();

        // Draw .cdb offset & rotation
        {
            vec3 offsetXMin = dbcPosition - vec3(20.0f, 0.0f, 0.0f);
            vec3 offsetXMax = dbcPosition + vec3(20.0f, 0.0f, 0.0f);
            debugRenderer->DrawLine3D(offsetXMin, offsetXMax, Color::Black);

            vec3 offsetYMin = dbcPosition - vec3(0.0f, 20.0f, 0.0f);
            vec3 offsetYMax = dbcPosition + vec3(0.0f, 20.0f, 0.0f);
            debugRenderer->DrawLine3D(offsetYMin, offsetYMax, Color::Black);

            vec3 offsetZMin = dbcPosition - vec3(0.0f, 0.0f, 20.0f);
            vec3 offsetZMax = dbcPosition + vec3(0.0f, 0.0f, 20.0f);
            debugRenderer->DrawLine3D(offsetZMin, offsetZMax, Color::Black);

            vec3 rotateDirection(20.0f, 0.0f, 0.0f);
            mat4x4 rotationMatrix = glm::rotate(mat4x4(1.0f), dbcFacing, vec3(0.0f, 1.0f, 0.0f));
            rotateDirection = rotationMatrix * vec4(rotateDirection, 1.0f);
            debugRenderer->DrawLine3D(dbcPosition, (dbcPosition + rotateDirection), Color::Red);
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

        if (!_curvePosition->Is2DSpline() && _drawPosition)
            DrawCurve(_cacheSplinePosition, debugRenderer, _curvePosition, colorPosition, true);

        if (!_curveTarget->Is2DSpline() && _drawTarget)
            DrawCurve(_cacheSplineTarget, debugRenderer, _curveTarget, colorTarget);
    }

    void CameraEditor::DrawCurve(std::vector<DebugRenderer::DebugVertexSolid3D>& cache, DebugRenderer* renderer, Spline::SplinePath* curve, SplineColor color, bool isPosition)
    {
        if (_isStarting)
        {
            // like that the position path is not in the middle of the camera
            vec3 yOffset(0.0f, 0.0f, 0.0f);
            if (isPosition)
                yOffset.y -= 1.0f;

            const auto& points = curve->GetInterpolatedStorage4D().Storage();
            for (auto ite = points.begin(); ite != points.end() - 1; ite++)
            {
                renderer->DrawLine3D(*ite + yOffset, *(ite + 1) + yOffset, color.pathColor);
            }

            if (!isPosition)
            {
                std::vector<DebugRenderer::DebugVertexSolid3D> targetSphere;
                renderer->GenerateSphere(targetSphere, _currentTarget, _sphereRadius, _sphereLongitude, _sphereLatitude, color.pathColor);
                renderer->DrawVerticesSolid3D(targetSphere);
            }
            return;
        }

        if (curve->Interpolate())
        {
            cache.clear();

            const auto& points = curve->GetInterpolatedStorage4D().Storage();
            std::vector<f32> rolls;
            if (isPosition)
            {
                _curveRoll->Interpolate();
                rolls = _curveRoll->GetInterpolatedStorage2D().Storage();
            }

            if (points.size() >= 2)
            {
                std::vector<vec3> cacheDebug;
                renderer->GenerateRibbon(cache, points, rolls, _pathWidth, color.pathColor, true);

                const auto& spline = curve->GetSpline4D();

                for (i32 i = 0; i < spline.data.size(); i++)
                {
                    if (curve->IsInterpolatedWithControl())
                    {
                        std::vector<vec3> empty;
                        renderer->GeneratePipe(cache, {spline.data[i].point, spline.controls[i].in}, _sphereRadius / 10.0f, 0.0f, 4, Color::Black, empty);
                        renderer->GeneratePipe(cache, {spline.data[i].point, spline.controls[i].out}, _sphereRadius / 10.0f, 0.0f, 4, Color::Black, empty);

                        renderer->GenerateSphere(cache, spline.controls[i].in, _sphereRadius, _sphereLongitude, _sphereLatitude, color.enterColor, true);
                        renderer->GenerateSphere(cache, spline.controls[i].out, _sphereRadius, _sphereLongitude, _sphereLatitude, color.exitColor, true);
                    }
                    renderer->GenerateSphere(cache, spline.data[i].point, _sphereRadius, _sphereLongitude, _sphereLatitude, color.valueColor, true);
                }

                Color debugColor = Color::Green;
                u32 c = 0;
                for (auto p : cacheDebug)
                {
                    if (c > _stepBetweenEachPoint)
                    {
                        debugColor.AddHue(0.1f);
                        c = 0;
                    }

                    renderer->GenerateSphere(cache, p, _sphereRadius + 0.5f, 5, 5, debugColor);
                    c++;
                }
            }

            _redraw++;
        }

        renderer->DrawVerticesSolid3D(cache);
    }
}