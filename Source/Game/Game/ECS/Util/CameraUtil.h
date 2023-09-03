#pragma once
#include <Base/Types.h>

namespace ECS::Util
{
	namespace CameraUtil
	{
		void SetCaptureMouse(bool capture);
		void CenterOnObject(const vec3& position, f32 radius);

        vec3 GetPosition();
        f32 GetRoll();
        f32 GetFov();

        void MoveTo(const vec3& position);
        void LookAt(const vec3& target);
        void SetRoll(f32 roll);
        void SetFov(f32 fov);
        void ResetRoll();
        void ResetFov();
        void MarkAsDirty();
	}
}