#pragma once
#include "BaseEditor.h"

#include <Game/Rendering/Debug/DebugRenderer.h>

#include <FileFormat/Novus/Math/Spline.h>
#include <FileFormat/Novus/Model/ComplexModel.h>

namespace ECS::Components
{
    struct Transform;
    struct Camera;
}

class DebugRenderer;

namespace Editor
{
    struct SplineColor
    {
        Color pathColor   { };
        Color enterColor  { };
        Color valueColor  { };
        Color exitColor   { };
    };

    struct SplineRoll
    {
        std::vector<Spline::Spline2D> roll;
        std::vector<u32> timestamp;
    };

    struct SplineFOV
    {
        std::vector<Spline::Spline2D> fov;
        std::vector<u32> timestamp;
    };

    class CameraEditor : public BaseEditor
    {
    public:
        CameraEditor();

        virtual const char* GetName() override { return "Camera Editor"; }

        virtual void DrawImGui() override;
        virtual void Update(f32 deltaTime) override;

    public:
        void OpenFile(const std::filesystem::path& file);

    private:
        void ReceiveDrop();
        bool OpenCamera(const std::string& path, Model::ComplexModel& output);

        void DrawHeader();
        bool DrawGizmo(ECS::Components::Camera& camera);
        void DrawInViewport(DebugRenderer* renderer);
        void DrawCurve(std::vector<DebugRenderer::DebugVertexSolid3D>& cache, DebugRenderer* renderer, Spline::SplinePath* curve, SplineColor color, bool isPosition = false);

        void ConvertOldCameraToNovus();
        vec3 SplineSpaceToWorld(const vec3& dbPos, f32 dbFacing, const vec3& base, const vec3& splinePos);
        vec3 WorldSpaceToSpline(const vec3& dbPos, f32 dbFacing, const vec3& base, const vec3& worldPos);

    private:
        bool _drawPosition = true;
        bool _drawTarget = true;

        Spline::SplinePath* _curvePosition  = nullptr;
        Spline::SplinePath* _curveTarget    = nullptr;
        Spline::SplinePath* _curveRoll      = nullptr;
        Spline::SplinePath* _curveFOV       = nullptr;

        std::string _file;
        Model::ComplexModel _model;

        std::vector<DebugRenderer::DebugVertexSolid3D> _cacheSplinePosition { };
        std::vector<DebugRenderer::DebugVertexSolid3D> _cacheSplineTarget { };

        i32 _maxTimer = 0;
        i32 _currentTime = 0;
        i32 _redraw = 0;

        // cinematic
        bool _isStarting = false;
        f32 _cinematicTime = 0.0f;
        vec3 _currentTarget { };
        //

        f32 rotation_rad = 0.0f;
        f32 offset_x = 0.0f;
        f32 offset_y = 0.0f;
        f32 offset_z = 0.0f;

        i32 _stepBetweenEachPoint = 50;
        i32 _currentPointSelected = -1;

        f32 _sphereRadius = 0.6f;
        i32 _sphereLongitude = 8;
        i32 _sphereLatitude = 8;

        f32 _pathWidth = 0.6f;

        f32 _rotationPerSegment = 0.0f;

        static const u8 _itemCount = 8;
        const char* const _items[_itemCount] = {
            "Linear",
            "Bezier",
            "Hermite",
            "BSpline",
            "CatmullRom",
            "CatmullRom_Uniform",
            "CatmullRom_Centripetal",
            "CatmullRom_Chordal"
        };
        i32 _selectedItem = 2;
    };
}