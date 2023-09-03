#pragma once
#include "BaseEditor.h"

#include <Game/Rendering/Debug/DebugRenderer.h>

#include <FileFormat/Novus/Math/Spline.h>
#include <FileFormat/Novus/Model/ComplexModel.h>
#include <FileFormat/Novus/ClientDB/Definitions.h>

#include <imgui/imguizmo/ImGuizmo.h>

namespace ECS
{
    namespace Components
    {
        struct Transform;
        struct Camera;
    }

    namespace Singletons
    {
        struct SplineDataDB;
    }
}

namespace Editor
{
    struct SplineColor
    {
        Color pathColor   { };
        Color enterColor  { };
        Color valueColor  { };
        Color exitColor   { };
    };

    struct SplineData
    {
        Spline::SplinePath Position;
        Spline::SplinePath Target;
        Spline::SplinePath Roll;
        Spline::SplinePath Fov;
    };

    struct SplineSelector
    {
        enum class SplineType
        {
            None, Position, Target, COUNT
        };

        enum class SplinePointType
        {
            Point, In, Out, COUNT
        };

        SplineType splineType               = SplineType::None;
        SplinePointType splinePointType     = SplinePointType::Point;
        i32 pointSelected                   = 0;
        ImGuizmo::OPERATION operation       = ImGuizmo::OPERATION::TRANSLATE;
    };

    enum class SplineControlMode
    {
        Aligned,
        Mirrored,
        Free,
        COUNT
    };

    static std::string SplineTypeName[static_cast<u32>(SplineSelector::SplineType::COUNT)] = {
        "None", "Position", "Target"
    };

    static std::string SplinePointTypeName[static_cast<u32>(SplineSelector::SplinePointType::COUNT)] = {
        "Point", "In", "Out"
    };

    static std::string SplineOperationName[3] = {
        "Translate", "Rolling", "Fov"
    };

    static std::string SplineControlModeName[static_cast<u32>(SplineControlMode::COUNT)] = {
        "Aligned", "Mirrored", "Free"
    };

    static std::vector<ImGuizmo::OPERATION> _selectorOperation ={
        ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::OPERATION::ROTATE_X, ImGuizmo::OPERATION::SCALE_X
    };

    class CameraEditor : public BaseEditor
    {
    public:
        CameraEditor();

        const char* GetName() override { return "Camera Editor"; }

        void DrawImGui() override;
        void Update(f32 deltaTime) override;

    private:
        void ReceiveDrop();
        void LoadCinematic();
        bool LoadSplineFile(const std::string& path, Spline::SplinePath& out);

        static void HandleCinematic(DB::Client::Definitions::Cinematic& cinematic, u32 time, std::vector<SplineData>& splineData, vec3& position, vec3& target, f32& roll, f32& fov);
        static u32 GetCurrentSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time);
        static f32 GetTimestampSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time);
        static u32 GetTimestampUntilSequence(DB::Client::Definitions::Cinematic& cinematic, u32 time);

        void DrawInterface();
        void DrawSelector();
        void DrawCinematic(ECS::Singletons::SplineDataDB& splineDataDB);
        void DrawDisplaySettings();
        void DrawDataInformation();
        void DrawComboBoxInterpolation(ImVec2 cursorPosition, f32 width, Spline::SplinePath& spline, const std::string& id);
        bool DrawGizmo(ECS::Components::Camera& camera);
        bool ApplyGizmo(ECS::Components::Camera& camera, Spline::SplinePath& splinePath);
        bool ComputeGizmo(ECS::Components::Camera& camera, Spline::SplinePath& spline, vec3& point, vec3& rotation);
        void DrawInViewport(DebugRenderer* renderer);
        void DrawCurve(std::vector<DebugRenderer::DebugVertexSolid3D>& cache, DebugRenderer* renderer, Spline::SplinePath& curve, Spline::SplinePath& roll, Spline::SplinePath& fov, SplineColor color, bool isPosition = false);

        void MarkAllSplineAsDirty();
        u32 GetTotalTimestamp();
        vec3 GetLastPosition();
        vec3 GetLastTarget();

    private:
        DB::Client::Definitions::Cinematic _currentCinematic;

        std::vector<SplineData> _splineData = { };
        std::vector<std::vector<DebugRenderer::DebugVertexSolid3D>> _cacheSplinePosition = { };
        std::vector<std::vector<DebugRenderer::DebugVertexSolid3D>> _cacheSplineTarget { };

        i32 _redraw = 0;

        // selector
        bool _drawLineOfView = false;
        bool _playCinematicForLineOfView = false;
        i32 _lineOfViewTime = 0;
        u32 _currentSequence = 0;
        u32 _currentOperation = 0;
        SplineSelector _currentSelection = { };
        SplineControlMode _currentControlMode = SplineControlMode::Aligned;
        //

        // cinematic
        bool _isStarting = false;
        u32 _cinematicTime = 0;
        vec3 _currentTarget { };
        //

        // display settings
        bool _drawAcceleration = false;
        bool _drawPosition = true;
        bool _drawTarget = true;
        i32 _stepBetweenEachPoint = 50;
        f32 _sphereRadius = 0.6f;
        i32 _sphereLongitude = 8;
        i32 _sphereLatitude = 8;
        f32 _pathWidth = 0.6f;
        bool _useReference = false;
        f32 _referenceAcceleration = 750.f;
        f32 _percentAcceleration = 0.5f;
        vec4 _savedAccelerationColorSlow = vec4{ 0.0f };
        vec4 _savedAccelerationColorHigh = vec4( 0.0f );
        vec4 _accelerationColorSlow = { 0.345f, 0.839f, 0.553f, 1.0f };
        vec4 _accelerationColorHigh = { 0.941f, 0.698f, 0.478f, 1.0f };
        //
    };
}