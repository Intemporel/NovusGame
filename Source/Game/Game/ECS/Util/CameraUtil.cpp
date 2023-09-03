#include "CameraUtil.h"

#include <Game/Util/ServiceLocator.h>
#include <Game/Rendering/GameRenderer.h>
#include <Game/ECS/Singletons/FreeflyingCameraSettings.h>
#include <Game/ECS/Singletons/ActiveCamera.h>
#include <Game/ECS/Components/Transform.h>
#include <Game/ECS/Components/Camera.h>

#include <Renderer/Window.h>

#include <imgui/imgui.h>
#include <entt/entt.hpp>
#include <GLFW/glfw3.h>

namespace ECS::Util
{
	namespace CameraUtil
	{
		void SetCaptureMouse(bool capture)
		{
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::FreeflyingCameraSettings& settings = ctx.at<ECS::Singletons::FreeflyingCameraSettings>();

            settings.captureMouse = capture;

            GameRenderer* gameRenderer = ServiceLocator::GetGameRenderer();
            Window* window = gameRenderer->GetWindow();

            if (capture)
            {
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
                glfwSetInputMode(window->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else
            {
                ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
                glfwSetInputMode(window->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
		}

        void CenterOnObject(const vec3& position, f32 radius)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.at<ECS::Singletons::ActiveCamera>();

            ECS::Components::Transform& transform = registry->get<ECS::Components::Transform>(activeCamera.entity);
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            f32 fovInRadians = glm::radians(camera.fov);

            // Compute the distance the camera should be to fit the entire bounding sphere
            f32 camDistance = (radius * 2.0f) / Math::Tan(fovInRadians / 2.0f);

            transform.position = position - (transform.forward * camDistance);
            transform.isDirty = true;

            camera.dirtyView = true;

            registry->get_or_emplace<ECS::Components::DirtyTransform> (activeCamera.entity);
        }

        vec3 GetPosition()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Transform& cameraTransform = registry->get<ECS::Components::Transform>(activeCamera.entity);

            return cameraTransform.position;
        }

        f32 GetRoll()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            return camera.roll;
        }

        f32 GetFov()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            return camera.fov;
        }

        void MoveTo(const vec3& position)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Transform& cameraTransform = registry->get<ECS::Components::Transform>(activeCamera.entity);

            cameraTransform.position = position;
        }

        void LookAt(const vec3& target)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Transform& cameraTransform = registry->get<ECS::Components::Transform>(activeCamera.entity);
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            vec3 direction = glm::normalize(target - cameraTransform.position);

            camera.yaw = glm::degrees(glm::atan(direction.x, direction.z));
            camera.pitch = -glm::degrees(glm::asin(direction.y));
        }

        void SetRoll(f32 roll)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            camera.roll = roll;
        }

        void SetFov(f32 fov)
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            camera.fov = fov;
        }

        void ResetRoll()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            camera.roll = 0.0f;
        }

        void ResetFov()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            camera.fov = 75.0f;
        }

        void MarkAsDirty()
        {
            entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
            entt::registry::context& ctx = registry->ctx();

            ECS::Singletons::ActiveCamera& activeCamera = ctx.emplace<ECS::Singletons::ActiveCamera>();
            ECS::Components::Camera& camera = registry->get<ECS::Components::Camera>(activeCamera.entity);

            camera.dirtyView = true;
        }
	}
}