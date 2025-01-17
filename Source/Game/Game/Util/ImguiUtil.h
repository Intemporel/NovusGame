#pragma once
#include <imgui/imgui.h>

#include <Base/Util/Reflection.h>

struct ImGuiWindow;

namespace Util
{
	namespace Imgui
	{
		bool IsDockedToMain();
		bool IsDockedToMain(ImGuiWindow* window);

		void ItemRowsBackground(float lineHeight = -1.0f, const ImColor& color = ImColor(20, 20, 20, 64));
		
		bool BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0, 0));
		void EndGroupPanel();

		bool DrawColoredRectAndDragF32(const char* id, f32& value, ImVec4 color, f32 fractionOfWidth, f32 speed = 1.0f);
		void DrawColoredRectAndReadOnlyF32(const char* id, const f32& value, ImVec4 color, f32 fractionOfWidth);

		bool DrawColoredRectAndDragI32(const char* id, i32& value, ImVec4 color, f32 fractionOfWidth);
		void DrawColoredRectAndReadOnlyI32(const char* id, const i32& value, ImVec4 color, f32 fractionOfWidth);

		bool DrawColoredRectAndDragU32(const char* id, u32& value, ImVec4 color, f32 fractionOfWidth);
		void DrawColoredRectAndReadOnlyU32(const char* id, const u32& value, ImVec4 color, f32 fractionOfWidth);
		
		// Float types
		bool Inspect(const char* name, f32& value, f32 speed);
		bool Inspect(const char* name, const f32& value, f32 speed);
		bool Inspect(const char* name, vec2& value, f32 speed);
		bool Inspect(const char* name, const vec2& value, f32 speed);
		bool Inspect(const char* name, vec3& value, f32 speed);
		bool Inspect(const char* name, const vec3& value, f32 speed);
		bool Inspect(const char* name, vec4& value, f32 speed);
		bool Inspect(const char* name, const vec4& value, f32 speed);
		bool Inspect(const char* name, quat& value, f32 speed);
		bool Inspect(const char* name, const quat& value, f32 speed);

		// Int types
		bool Inspect(const char* name, i32& value, f32 speed);
		bool Inspect(const char* name, const i32& value, f32 speed);
		bool Inspect(const char* name, ivec2& value, f32 speed);
		bool Inspect(const char* name, const ivec2& value, f32 speed);
		bool Inspect(const char* name, ivec3& value, f32 speed);
		bool Inspect(const char* name, const ivec3& value, f32 speed);
		bool Inspect(const char* name, ivec4& value, f32 speed);
		bool Inspect(const char* name, const ivec4& value, f32 speed);

		// Unsigned int types
		bool Inspect(const char* name, u32& value, f32 speed);
		bool Inspect(const char* name, const u32& value, f32 speed);
		bool Inspect(const char* name, uvec2& value, f32 speed);
		bool Inspect(const char* name, const uvec2& value, f32 speed);
		bool Inspect(const char* name, uvec3& value, f32 speed);
		bool Inspect(const char* name, const uvec3& value, f32 speed);
		bool Inspect(const char* name, uvec4& value, f32 speed);
		bool Inspect(const char* name, const uvec4& value, f32 speed);

		// Strings
		bool Inspect(const char* name, std::string& value, f32 speed);
		bool Inspect(const char* name, const std::string& value, f32 speed);


		template <typename ComponentType>
		bool Inspect(ComponentType& component) 
		{
			static_assert(!std::is_pointer_v<ComponentType>, "Inspect<T>() does not support pointers, please dereference the pointer before calling Inspect<T>()");

			bool isDirty = false;
			auto typeDescriptor = refl::reflect<ComponentType>();
			refl::const_string componentName = refl::descriptor::get_simple_name(typeDescriptor);

			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1));
			if (BeginGroupPanel(componentName.c_str()))
			{
				// Loop over each field in the component
				refl::util::for_each(typeDescriptor.members, [&](auto field)
				{
					if constexpr (refl::descriptor::has_attribute<Reflection::Hidden>(field))
					{
						return;
					}

					// Get the name of the field
					std::string name = refl::descriptor::get_display_name(field);

					f32 speed = 1.0f;

					if constexpr (refl::descriptor::has_attribute<Reflection::DragSpeed>(field))
					{
						constexpr auto dragSpeed = refl::descriptor::get_attribute<Reflection::DragSpeed>(field);

						speed = dragSpeed.speed;
					}

					if constexpr (refl::descriptor::has_attribute<Reflection::ReadOnly>(field) || !is_readable(field))
					{
						const auto& fieldRef = field.get(component);
						Inspect(name.c_str(), fieldRef, speed);
					}
					else
					{
						auto& fieldRef = field.get(component);
						isDirty |= Inspect(name.c_str(), fieldRef, speed);
					}
				});
			}
			EndGroupPanel();
			ImGui::PopStyleColor();

			return isDirty;
		}
	}
}