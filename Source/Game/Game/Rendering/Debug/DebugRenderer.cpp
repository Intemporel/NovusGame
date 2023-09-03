#include "DebugRenderer.h"
#include "Game/Rendering/RenderResources.h"

#include <Base/CVarSystem/CVarSystem.h>

#include <Renderer/Renderer.h>
#include <Renderer/RenderGraph.h>
#include <Renderer/Descriptors/ImageDesc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

AutoCVar_Int CVAR_DebugRendererNumGPUVertices("debugRenderer.numGPUVertices", "number of GPU vertices to allocate for", 32000000);
AutoCVar_ShowFlag CVAR_DebugRendererAlwaysOnTop("debugRenderer.alwaysOnTop", "always show debug renderer on top", ShowFlag::DISABLED);

DebugRenderer::DebugRenderer(Renderer::Renderer* renderer)
{
	_renderer = renderer;

	_debugVertices2D.SetDebugName("DebugVertices2D");
	_debugVertices2D.SetUsage(Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::STORAGE_BUFFER);
	_debugVertices2D.SyncToGPU(_renderer);
	_draw2DDescriptorSet.Bind("_vertices", _debugVertices2D.GetBuffer());

	_debugVertices3D.SetDebugName("DebugVertices3D");
	_debugVertices3D.SetUsage(Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::STORAGE_BUFFER);
	_debugVertices3D.SyncToGPU(_renderer);
	_draw3DDescriptorSet.Bind("_vertices", _debugVertices3D.GetBuffer());

	_debugVerticesSolid2D.SetDebugName("DebugVerticesSolid2D");
	_debugVerticesSolid2D.SetUsage(Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::STORAGE_BUFFER);
	_debugVerticesSolid2D.SyncToGPU(_renderer);
	_drawSolid2DDescriptorSet.Bind("_vertices", _debugVerticesSolid2D.GetBuffer());

	_debugVerticesSolid3D.SetDebugName("DebugVerticesSolid3D");
	_debugVerticesSolid3D.SetUsage(Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::STORAGE_BUFFER);
	_debugVerticesSolid3D.SyncToGPU(_renderer);
	_drawSolid3DDescriptorSet.Bind("_vertices", _debugVerticesSolid3D.GetBuffer());

	// Create indirect buffers for GPU-side debugging
	u32 numGPUVertices = CVAR_DebugRendererNumGPUVertices.Get();
	{
		Renderer::BufferDesc desc;
		desc.name = "DebugVertices2D";
		desc.size = sizeof(DebugVertex2D) * numGPUVertices;
		desc.usage = Renderer::BufferUsage::STORAGE_BUFFER | Renderer::BufferUsage::TRANSFER_DESTINATION;

		_gpuDebugVertices2D = _renderer->CreateBuffer(_gpuDebugVertices2D, desc);
		_draw2DIndirectDescriptorSet.Bind("_vertices", _gpuDebugVertices2D);
		_debugDescriptorSet.Bind("_debugVertices2D", _gpuDebugVertices2D);
	}

	{
		Renderer::BufferDesc desc;
		desc.name = "DebugVertices3D";
		desc.size = sizeof(DebugVertex3D) * numGPUVertices;
		desc.usage = Renderer::BufferUsage::STORAGE_BUFFER | Renderer::BufferUsage::TRANSFER_DESTINATION;

		_gpuDebugVertices3D = _renderer->CreateBuffer(_gpuDebugVertices3D, desc);
		_draw3DIndirectDescriptorSet.Bind("_vertices", _gpuDebugVertices3D);
		_debugDescriptorSet.Bind("_debugVertices3D", _gpuDebugVertices3D);
	}

	// Create indirect argument buffers for GPU-side debugging
	{
		Renderer::BufferDesc desc;
		desc.name = "DebugVertices2DArgument";
		desc.size = sizeof(Renderer::IndirectDraw);
		desc.usage = Renderer::BufferUsage::STORAGE_BUFFER | Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::INDIRECT_ARGUMENT_BUFFER;

		_gpuDebugVertices2DArgumentBuffer = _renderer->CreateAndFillBuffer(_gpuDebugVertices2DArgumentBuffer, desc, [](void* mappedMemory, size_t size)
		{
			Renderer::IndirectDraw* indirectDraw = static_cast<Renderer::IndirectDraw*>(mappedMemory);
			indirectDraw->instanceCount = 1;
		});
		_debugDescriptorSet.Bind("_debugVertices2DCount", _gpuDebugVertices2DArgumentBuffer);
	}

	{
		Renderer::BufferDesc desc;
		desc.name = "DebugVertices3DArgument";
		desc.size = sizeof(Renderer::IndirectDraw);
		desc.usage = Renderer::BufferUsage::STORAGE_BUFFER | Renderer::BufferUsage::TRANSFER_DESTINATION | Renderer::BufferUsage::INDIRECT_ARGUMENT_BUFFER;

		_gpuDebugVertices3DArgumentBuffer = _renderer->CreateAndFillBuffer(_gpuDebugVertices3DArgumentBuffer, desc, [](void* mappedMemory, size_t size)
		{
			Renderer::IndirectDraw* indirectDraw = static_cast<Renderer::IndirectDraw*>(mappedMemory);
			indirectDraw->instanceCount = 1;
		});
		_debugDescriptorSet.Bind("_debugVertices3DCount", _gpuDebugVertices3DArgumentBuffer);
	}
}

void DebugRenderer::Update(f32 deltaTime)
{
	// Draw world axises
	DrawLine3D(vec3(0.0f, 0.0f, 0.0f), vec3(100.0f, 0.0f, 0.0f), Color::Red);
	DrawLine3D(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 100.0f, 0.0f), Color::Green);
	DrawLine3D(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 100.0f), Color::Blue);
}

void DebugRenderer::AddStartFramePass(Renderer::RenderGraph* renderGraph, RenderResources& resources, u8 frameIndex)
{
	struct Data
	{
		Renderer::BufferMutableResource gpuDebugVertices2DArgumentBuffer;
		Renderer::BufferMutableResource gpuDebugVertices3DArgumentBuffer;
	};

	renderGraph->AddPass<Data>("DebugRenderReset",
		[=, &resources](Data& data, Renderer::RenderGraphBuilder& builder) // Setup
		{
			using BufferUsage = Renderer::BufferPassUsage;

			data.gpuDebugVertices2DArgumentBuffer = builder.Write(_gpuDebugVertices2DArgumentBuffer, BufferUsage::TRANSFER);
			data.gpuDebugVertices3DArgumentBuffer = builder.Write(_gpuDebugVertices3DArgumentBuffer, BufferUsage::TRANSFER);

			return true;// Return true from setup to enable this pass, return false to disable it
		},
		[=](Data& data, Renderer::RenderGraphResources& graphResources, Renderer::CommandList& commandList) // Execute
		{
			GPU_SCOPED_PROFILER_ZONE(commandList, DebugRenderReset);

			commandList.FillBuffer(data.gpuDebugVertices2DArgumentBuffer, 0, 4, 0); // Reset vertexCount to 0
			commandList.FillBuffer(data.gpuDebugVertices3DArgumentBuffer, 0, 4, 0); // Reset vertexCount to 0
		});
}

void DebugRenderer::Add2DPass(Renderer::RenderGraph* renderGraph, RenderResources& resources, u8 frameIndex)
{
	// Sync to GPU
	if (_debugVertices2D.SyncToGPU(_renderer))
	{
		_draw2DDescriptorSet.Bind("_vertices", _debugVertices2D.GetBuffer());
	}
	if (_debugVerticesSolid2D.SyncToGPU(_renderer))
	{
		_drawSolid2DDescriptorSet.Bind("_vertices", _debugVerticesSolid2D.GetBuffer());
	}

	struct Data
	{
		Renderer::ImageMutableResource color;

		Renderer::BufferResource gpuDebugVertices2D;
		Renderer::BufferResource gpuDebugVertices2DArgumentBuffer;

		Renderer::DescriptorSetResource globalSet;
		Renderer::DescriptorSetResource draw2DSet;
		Renderer::DescriptorSetResource draw2DIndirectSet;
		Renderer::DescriptorSetResource drawSolid2DSet;
	};
	renderGraph->AddPass<Data>("DebugRender2D",
		[=, &resources](Data& data, Renderer::RenderGraphBuilder& builder) // Setup
		{
			using BufferUsage = Renderer::BufferPassUsage;

			data.color = builder.Write(resources.sceneColor, Renderer::PipelineType::GRAPHICS, Renderer::LoadMode::LOAD);

			data.gpuDebugVertices2D = builder.Read(_gpuDebugVertices2D, BufferUsage::GRAPHICS);
			data.gpuDebugVertices2DArgumentBuffer = builder.Read(_gpuDebugVertices2DArgumentBuffer, BufferUsage::GRAPHICS);
			builder.Read(_debugVertices2D.GetBuffer(), BufferUsage::GRAPHICS);
			builder.Read(_debugVerticesSolid2D.GetBuffer(), BufferUsage::GRAPHICS);

			data.globalSet = builder.Use(resources.globalDescriptorSet);
			data.draw2DSet = builder.Use(_draw2DDescriptorSet);
			data.draw2DIndirectSet = builder.Use(_draw2DIndirectDescriptorSet);
			data.drawSolid2DSet = builder.Use(_drawSolid2DDescriptorSet);

			return true;// Return true from setup to enable this pass, return false to disable it
		},
		[=](Data& data, Renderer::RenderGraphResources& graphResources, Renderer::CommandList& commandList) // Execute
		{
			GPU_SCOPED_PROFILER_ZONE(commandList, DebugRender2D);

			Renderer::GraphicsPipelineDesc pipelineDesc;
			graphResources.InitializePipelineDesc(pipelineDesc);

			// Rasterizer state
			pipelineDesc.states.rasterizerState.cullMode = Renderer::CullMode::BACK;

			// Render targets.
			pipelineDesc.renderTargets[0] = data.color;

			// Shader
			Renderer::VertexShaderDesc vertexShaderDesc;
			vertexShaderDesc.path = "Debug/Debug2D.vs.hlsl";

			Renderer::PixelShaderDesc pixelShaderDesc;
			pixelShaderDesc.path = "Debug/Debug2D.ps.hlsl";

			pipelineDesc.states.vertexShader = _renderer->LoadShader(vertexShaderDesc);
			pipelineDesc.states.pixelShader = _renderer->LoadShader(pixelShaderDesc);

			// Solid
			{
				pipelineDesc.states.primitiveTopology = Renderer::PrimitiveTopology::Triangles;

				Renderer::GraphicsPipelineID pipeline = _renderer->CreatePipeline(pipelineDesc);
				// CPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.drawSolid2DSet, frameIndex);

					// Draw
					commandList.Draw(static_cast<u32>(_debugVerticesSolid2D.Size()), 1, 0, 0);

					commandList.EndPipeline(pipeline);
				}
				_debugVerticesSolid2D.Clear(false);
			}

			// Wireframe
			{
				pipelineDesc.states.primitiveTopology = Renderer::PrimitiveTopology::Lines;

				Renderer::GraphicsPipelineID pipeline = _renderer->CreatePipeline(pipelineDesc);
				// CPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.draw2DSet, frameIndex);

					// Draw
					commandList.Draw(static_cast<u32>(_debugVertices2D.Size()), 1, 0, 0);

					commandList.EndPipeline(pipeline);
				}
				_debugVertices2D.Clear(false);

				// GPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.draw2DIndirectSet, frameIndex);

					// Draw
					commandList.DrawIndirect(data.gpuDebugVertices2DArgumentBuffer, 0, 1);

					commandList.EndPipeline(pipeline);
				}
			}
		});
}

void DebugRenderer::Add3DPass(Renderer::RenderGraph* renderGraph, RenderResources& resources, u8 frameIndex)
{
	// Sync to GPU
	if (_debugVertices3D.SyncToGPU(_renderer))
	{
		_draw3DDescriptorSet.Bind("_vertices", _debugVertices3D.GetBuffer());
	}
	if (_debugVerticesSolid3D.SyncToGPU(_renderer))
	{
		_drawSolid3DDescriptorSet.Bind("_vertices", _debugVerticesSolid3D.GetBuffer());
	}

	struct Data
	{
		Renderer::ImageMutableResource color;
		Renderer::DepthImageMutableResource depth;

		Renderer::BufferResource gpuDebugVertices3D;
		Renderer::BufferResource gpuDebugVertices3DArgumentBuffer;

		Renderer::DescriptorSetResource globalSet;
		Renderer::DescriptorSetResource draw3DSet;
		Renderer::DescriptorSetResource draw3DIndirectSet;
		Renderer::DescriptorSetResource drawSolid3DSet;
	};
	renderGraph->AddPass<Data>("DebugRender3D",
		[=, &resources](Data& data, Renderer::RenderGraphBuilder& builder) // Setup
		{
			using BufferUsage = Renderer::BufferPassUsage;

			data.color = builder.Write(resources.sceneColor, Renderer::PipelineType::GRAPHICS, Renderer::LoadMode::LOAD);

			if (CVAR_DebugRendererAlwaysOnTop.Get() == ShowFlag::ENABLED)
			{
				data.depth = builder.Write(resources.debugRendererDepth, Renderer::PipelineType::GRAPHICS, Renderer::LoadMode::CLEAR);
			}
			else
			{
				data.depth = builder.Write(resources.depth, Renderer::PipelineType::GRAPHICS, Renderer::LoadMode::LOAD);
			}

			data.gpuDebugVertices3D = builder.Read(_gpuDebugVertices3D, BufferUsage::GRAPHICS);
			data.gpuDebugVertices3DArgumentBuffer = builder.Read(_gpuDebugVertices3DArgumentBuffer, BufferUsage::GRAPHICS);
			builder.Read(resources.cameras.GetBuffer(), BufferUsage::GRAPHICS);
			builder.Read(_debugVertices3D.GetBuffer(), BufferUsage::GRAPHICS);
			builder.Read(_debugVerticesSolid3D.GetBuffer(), BufferUsage::GRAPHICS);

			data.globalSet = builder.Use(resources.globalDescriptorSet);
			data.draw3DSet = builder.Use(_draw3DDescriptorSet);
			data.draw3DIndirectSet = builder.Use(_draw3DIndirectDescriptorSet);
			data.drawSolid3DSet = builder.Use(_drawSolid3DDescriptorSet);

			return true;// Return true from setup to enable this pass, return false to disable it
		},
		[=](Data& data, Renderer::RenderGraphResources& graphResources, Renderer::CommandList& commandList) // Execute
		{
			GPU_SCOPED_PROFILER_ZONE(commandList, DebugRender3D);

			Renderer::GraphicsPipelineDesc pipelineDesc;
			graphResources.InitializePipelineDesc(pipelineDesc);

			// Shader
			Renderer::VertexShaderDesc vertexShaderDesc;
			vertexShaderDesc.path = "Debug/DebugSolid3D.vs.hlsl";

			Renderer::PixelShaderDesc pixelShaderDesc;
			pixelShaderDesc.path = "Debug/DebugSolid3D.ps.hlsl";

			pipelineDesc.states.vertexShader = _renderer->LoadShader(vertexShaderDesc);
			pipelineDesc.states.pixelShader = _renderer->LoadShader(pixelShaderDesc);

			// Depth state
			pipelineDesc.states.depthStencilState.depthEnable = true;
			pipelineDesc.states.depthStencilState.depthWriteEnable = true;
			pipelineDesc.states.depthStencilState.depthFunc = Renderer::ComparisonFunc::GREATER;

			// Rasterizer state
			pipelineDesc.states.rasterizerState.cullMode = Renderer::CullMode::FRONT;
			pipelineDesc.states.rasterizerState.frontFaceMode = Renderer::Settings::FRONT_FACE_STATE;

			pipelineDesc.renderTargets[0] = data.color;

			pipelineDesc.depthStencil = data.depth;

			// Solid
			{
				pipelineDesc.states.primitiveTopology = Renderer::PrimitiveTopology::Triangles;

				Renderer::GraphicsPipelineID pipeline = _renderer->CreatePipeline(pipelineDesc);

				// CPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.drawSolid3DSet, frameIndex);

					// Draw
					commandList.Draw(static_cast<u32>(_debugVerticesSolid3D.Size()), 1, 0, 0);

					commandList.EndPipeline(pipeline);
				}
				_debugVerticesSolid3D.Clear(false);
			}

			// Wireframe
			{
				// Shader
				Renderer::VertexShaderDesc vertexShaderDesc;
				vertexShaderDesc.path = "Debug/Debug3D.vs.hlsl";

				Renderer::PixelShaderDesc pixelShaderDesc;
				pixelShaderDesc.path = "Debug/Debug3D.ps.hlsl";

				pipelineDesc.states.vertexShader = _renderer->LoadShader(vertexShaderDesc);
				pipelineDesc.states.pixelShader = _renderer->LoadShader(pixelShaderDesc);

				pipelineDesc.states.depthStencilState.depthWriteEnable = false;
				pipelineDesc.states.primitiveTopology = Renderer::PrimitiveTopology::Lines;

				Renderer::GraphicsPipelineID pipeline = _renderer->CreatePipeline(pipelineDesc);

				// CPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.draw3DSet, frameIndex);

					// Draw
					commandList.Draw(static_cast<u32>(_debugVertices3D.Size()), 1, 0, 0);

					commandList.EndPipeline(pipeline);
				}
				_debugVertices3D.Clear(false);

				// GPU side debug rendering
				{
					commandList.BeginPipeline(pipeline);

					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::GLOBAL, data.globalSet, frameIndex);
					commandList.BindDescriptorSet(Renderer::DescriptorSetSlot::PER_PASS, data.draw3DIndirectSet, frameIndex);

					// Draw
					commandList.DrawIndirect(data.gpuDebugVertices3DArgumentBuffer, 0, 1);

					commandList.EndPipeline(pipeline);
				}
			}
		});
}

void DebugRenderer::DrawLine2D(const glm::vec2& from, const glm::vec2& to, Color color)
{
	auto& vertices = _debugVertices2D.Get();

	u32 colorInt = color.ToU32();

	vertices.push_back({ from, colorInt });
	vertices.push_back({ to, colorInt });
}

void DebugRenderer::DrawLine3D(const glm::vec3& from, const glm::vec3& to, Color color)
{
	auto& vertices = _debugVertices3D.Get();

	u32 colorInt = color.ToU32();

	vertices.push_back({ from, colorInt });
	vertices.push_back({ to, colorInt });
}

void DebugRenderer::DrawAABB3D(const vec3& center, const vec3& extents, Color color)
{
	auto& vertices = _debugVertices3D.Get();

	vec3 v0 = center - extents;
	vec3 v1 = center + extents;

	u32 colorInt = color.ToU32();

	// Bottom
	vertices.push_back({ { v0.x, v0.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v0.y, v0.z }, colorInt });

	// Top
	vertices.push_back({ { v0.x, v1.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v1.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v1.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v1.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v1.y, v0.z }, colorInt });

	// Vertical edges
	vertices.push_back({ { v0.x, v0.y, v0.z }, colorInt });
	vertices.push_back({ { v0.x, v1.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v0.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v0.z }, colorInt });
	vertices.push_back({ { v0.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v0.x, v1.y, v1.z }, colorInt });
	vertices.push_back({ { v1.x, v0.y, v1.z }, colorInt });
	vertices.push_back({ { v1.x, v1.y, v1.z }, colorInt });
}

void DebugRenderer::DrawOBB3D(const vec3& center, const vec3& extents, const quat& rotation, Color color)
{
	auto& vertices = _debugVertices3D.Get();

	u32 colorInt = color.ToU32();

	vec3 corners[8] = {
		center + rotation * glm::vec3(-extents.x, -extents.y, -extents.z),
		center + rotation * glm::vec3( extents.x, -extents.y, -extents.z),
		center + rotation * glm::vec3( extents.x, -extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x, -extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x,  extents.y, -extents.z),
		center + rotation * glm::vec3( extents.x,  extents.y, -extents.z),
		center + rotation * glm::vec3( extents.x,  extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x,  extents.y,  extents.z)
	};

	// Bottom
	vertices.push_back({ corners[0], colorInt });
	vertices.push_back({ corners[1], colorInt });
	vertices.push_back({ corners[1], colorInt });
	vertices.push_back({ corners[2], colorInt });
	vertices.push_back({ corners[2], colorInt });
	vertices.push_back({ corners[3], colorInt });
	vertices.push_back({ corners[3], colorInt });
	vertices.push_back({ corners[0], colorInt });

	// Top
	vertices.push_back({ corners[4], colorInt });
	vertices.push_back({ corners[5], colorInt });
	vertices.push_back({ corners[5], colorInt });
	vertices.push_back({ corners[6], colorInt });
	vertices.push_back({ corners[6], colorInt });
	vertices.push_back({ corners[7], colorInt });
	vertices.push_back({ corners[7], colorInt });
	vertices.push_back({ corners[4], colorInt });

	// Vertical edges
	vertices.push_back({ corners[0], colorInt });
	vertices.push_back({ corners[4], colorInt });
	vertices.push_back({ corners[1], colorInt });
	vertices.push_back({ corners[5], colorInt });
	vertices.push_back({ corners[2], colorInt });
	vertices.push_back({ corners[6], colorInt });
	vertices.push_back({ corners[3], colorInt });
	vertices.push_back({ corners[7], colorInt });
}

void DebugRenderer::DrawTriangle2D(const glm::vec2& v0, const glm::vec2& v1, const glm::vec2& v2, Color color)
{
	DrawLine2D(v0, v1, color);
	DrawLine2D(v1, v2, color);
	DrawLine2D(v2, v0, color);
}

void DebugRenderer::DrawTriangle3D(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, Color color)
{
	DrawLine3D(v0, v1, color);
	DrawLine3D(v1, v2, color);
	DrawLine3D(v2, v0, color);
}

void DebugRenderer::DrawCircle3D(const vec3& center, f32 radius, i32 resolution, Color color)
{
	auto& vertices = _debugVertices3D.Get();

	u32 colorInt = color.ToU32();

	constexpr f32 PI = glm::pi<f32>();
	constexpr f32 TAU = PI * 2.0f;

	f32 increment = TAU / resolution;
	for (f32 currentAngle = 0.0f; currentAngle <= TAU; currentAngle += increment)
	{
		vec3 pos = vec3(radius * glm::cos(currentAngle) + center.x, radius * glm::sin(currentAngle) + center.y, center.z);
		vertices.push_back({ vec3(pos.x, pos.y, pos.z), colorInt });
	}
}

vec3 DebugRenderer::UnProject(const vec3& point, const mat4x4& m)
{
	vec4 obj = m * vec4(point, 1.0f);
	obj /= obj.w;
	return vec3(obj);
}

void DebugRenderer::DrawFrustum(const mat4x4& viewProjectionMatrix, Color color)
{
	const mat4x4 m = glm::inverse(viewProjectionMatrix);

	const vec3 near0 = UnProject(vec3(-1.0f, -1.0f, 0.0f), m);
	const vec3 near1 = UnProject(vec3(+1.0f, -1.0f, 0.0f), m);
	const vec3 near2 = UnProject(vec3(+1.0f, +1.0f, 0.0f), m);
	const vec3 near3 = UnProject(vec3(-1.0f, +1.0f, 0.0f), m);

	const vec3 far0 = UnProject(vec3(-1.0f, -1.0f, 1.0f), m);
	const vec3 far1 = UnProject(vec3(+1.0f, -1.0f, 1.0f), m);
	const vec3 far2 = UnProject(vec3(+1.0f, +1.0f, 1.0f), m);
	const vec3 far3 = UnProject(vec3(-1.0f, +1.0f, 1.0f), m);

	// Near plane
	DrawLine3D(near0, near1, color);
	DrawLine3D(near1, near2, color);
	DrawLine3D(near2, near3, color);
	DrawLine3D(near3, near0, color);

	// Far plane
	DrawLine3D(far0, far1, color);
	DrawLine3D(far1, far2, color);
	DrawLine3D(far2, far3, color);
	DrawLine3D(far3, far0, color);

	// Edges
	DrawLine3D(near0, far0, color);
	DrawLine3D(near1, far1, color);
	DrawLine3D(near2, far2, color);
	DrawLine3D(near3, far3, color);
}

void DebugRenderer::DrawMatrix(const mat4x4& matrix, f32 scale)
{
	const vec3 origin = vec3(matrix[3].x, matrix[3].y, matrix[3].z);

	DrawLine3D(origin, origin + (vec3(matrix[0].x, matrix[0].y, matrix[0].z) * scale), Color::Red);
	DrawLine3D(origin, origin + (vec3(matrix[1].x, matrix[1].y, matrix[1].z) * scale), Color::Green);
	DrawLine3D(origin, origin + (vec3(matrix[2].x, matrix[2].y, matrix[2].z) * scale), Color::Blue);
}

void DebugRenderer::DrawVerticesSolid3D(const std::vector<DebugVertexSolid3D>& data)
{
    auto& vertices = _debugVerticesSolid3D.Get();
    vertices.insert(vertices.end(), data.begin(), data.end());
}

void DebugRenderer::DrawLineSolid2D(const vec2& from, const vec2& to, Color color, bool shaded)
{
	color.a = static_cast<f32>(shaded);
	u32 colorInt = color.ToU32();

	auto& vertices = _debugVerticesSolid2D.Get();

	vertices.push_back({ from, colorInt });
	vertices.push_back({ to, colorInt });
}

void DebugRenderer::DrawAABBSolid3D(const vec3& center, const vec3& extents, Color color, bool shaded)
{
	auto& vertices = _debugVerticesSolid3D.Get();

	vec3 v0 = center - extents;
	vec3 v1 = center + extents;

	// Normals
	vec3 bottomNormal = { 0, -1, 0 };
	vec3 topNormal = { 0, 1, 0 };
	vec3 frontNormal = { 0, 0, -1 };
	vec3 backNormal = { 0, 0, 1 };
	vec3 leftNormal = { -1, 0, 0 };
	vec3 rightNormal = { 1, 0, 0 };

	color.a = static_cast<f32>(shaded);
	u32 colorInt = color.ToU32();
	f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);

	// Bottom
	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v1.z, 0 ), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v0.z, 0 ), vec4(bottomNormal, colorFloat) });

	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v0.y, v1.z, 0 ), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v1.z, 0 ), vec4(bottomNormal, colorFloat) });

	// Top
	vertices.push_back({ vec4(v0.x, v1.y, v0.z, 0 ), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v0.z, 0 ), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(topNormal, colorFloat) });

	vertices.push_back({ vec4(v0.x, v1.y, v0.z, 0 ), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v1.z, 0 ), vec4(topNormal, colorFloat) });

	// Front
	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });

	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v0.z, 0 ), vec4(frontNormal, colorFloat) });

	// Back
	vertices.push_back({ vec4(v0.x, v0.y, v1.z, 0 ), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v1.z, 0 ), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(backNormal, colorFloat) });

	vertices.push_back({ vec4(v0.x, v0.y, v1.z, 0 ), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v1.z, 0 ), vec4(backNormal, colorFloat) });

	// Left
	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v1.z, 0 ), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v0.y, v1.z, 0 ), vec4(leftNormal, colorFloat) });

	vertices.push_back({ vec4(v0.x, v0.y, v0.z, 0 ), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v0.z, 0 ), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(v0.x, v1.y, v1.z, 0 ), vec4(leftNormal, colorFloat) });

	// Right
	vertices.push_back({ vec4(v1.x, v0.y, v0.z, 0 ), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v0.y, v1.z, 0 ), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(rightNormal, colorFloat) });

	vertices.push_back({ vec4(v1.x, v0.y, v0.z, 0 ), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v1.z, 0 ), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(v1.x, v1.y, v0.z, 0 ), vec4(rightNormal, colorFloat) });
}

void DebugRenderer::DrawOBBSolid3D(const vec3& center, const vec3& extents, const quat& rotation, Color color, bool shaded)
{
	auto& vertices = _debugVerticesSolid3D.Get();

	vec3 corners[8] = {
		center + rotation * glm::vec3(-extents.x, -extents.y, -extents.z),
		center + rotation * glm::vec3(extents.x, -extents.y, -extents.z),
		center + rotation * glm::vec3(extents.x, -extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x, -extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x,  extents.y, -extents.z),
		center + rotation * glm::vec3(extents.x,  extents.y, -extents.z),
		center + rotation * glm::vec3(extents.x,  extents.y,  extents.z),
		center + rotation * glm::vec3(-extents.x,  extents.y,  extents.z)
	};

	// Normals for each face of the box (oriented with the rotation)
	vec3 bottomNormal = rotation * vec3(0, -1, 0);
	vec3 topNormal = rotation * vec3(0, 1, 0);
	vec3 frontNormal = rotation * vec3(0, 0, -1);
	vec3 backNormal = rotation * vec3(0, 0, 1);
	vec3 leftNormal = rotation * vec3(-1, 0, 0);
	vec3 rightNormal = rotation * vec3(1, 0, 0);

	color.a = static_cast<f32>(shaded);
	u32 colorInt = color.ToU32();
	f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);

	// Bottom
	vertices.push_back({ vec4(corners[0], 0.0f), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(corners[2], 0.0f), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(corners[1], 0.0f), vec4(bottomNormal, colorFloat) });

	vertices.push_back({ vec4(corners[0], 0.0f), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(corners[3], 0.0f), vec4(bottomNormal, colorFloat) });
	vertices.push_back({ vec4(corners[2], 0.0f), vec4(bottomNormal, colorFloat) });

	// Top
	vertices.push_back({ vec4(corners[4], 0.0f), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(corners[5], 0.0f), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(corners[6], 0.0f), vec4(topNormal, colorFloat) });

	vertices.push_back({ vec4(corners[4], 0.0f), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(corners[6], 0.0f), vec4(topNormal, colorFloat) });
	vertices.push_back({ vec4(corners[7], 0.0f), vec4(topNormal, colorFloat) });

	// Front
	vertices.push_back({ vec4(corners[0], 0.0f), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(corners[1], 0.0f), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(corners[5], 0.0f), vec4(frontNormal, colorFloat) });

	vertices.push_back({ vec4(corners[0], 0.0f), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(corners[5], 0.0f), vec4(frontNormal, colorFloat) });
	vertices.push_back({ vec4(corners[4], 0.0f), vec4(frontNormal, colorFloat) });

	// Back
	vertices.push_back({ vec4(corners[2], 0.0f), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(corners[3], 0.0f), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(corners[7], 0.0f), vec4(backNormal, colorFloat) });

	vertices.push_back({ vec4(corners[2], 0.0f), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(corners[7], 0.0f), vec4(backNormal, colorFloat) });
	vertices.push_back({ vec4(corners[6], 0.0f), vec4(backNormal, colorFloat) });

	// Left
	vertices.push_back({ vec4(corners[0], 0.0f), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(corners[7], 0.0f), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(corners[3], 0.0f), vec4(leftNormal, colorFloat) });

	vertices.push_back({ vec4(corners[0], 0.0f), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(corners[4], 0.0f), vec4(leftNormal, colorFloat) });
	vertices.push_back({ vec4(corners[7], 0.0f), vec4(leftNormal, colorFloat) });

	// Right
	vertices.push_back({ vec4(corners[1], 0.0f), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(corners[2], 0.0f), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(corners[6], 0.0f), vec4(rightNormal, colorFloat) });

	vertices.push_back({ vec4(corners[1], 0.0f), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(corners[6], 0.0f), vec4(rightNormal, colorFloat) });
	vertices.push_back({ vec4(corners[5], 0.0f), vec4(rightNormal, colorFloat) });
}

void DebugRenderer::DrawTriangleSolid2D(const vec2& v0, const vec2& v1, const vec2& v2, Color color, bool shaded)
{
	color.a = static_cast<f32>(shaded);

	DrawLineSolid2D(v0, v1, color, shaded);
	DrawLineSolid2D(v1, v2, color, shaded);
	DrawLineSolid2D(v2, v0, color, shaded);
}

void DebugRenderer::DrawTriangleSolid3D(const vec3& v0, const vec3& v1, const vec3& v2, Color color, bool shaded)
{
	auto& vertices = _debugVerticesSolid3D.Get();

	vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

	color.a = static_cast<f32>(shaded);
	u32 colorInt = color.ToU32();
	f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);
	
	vertices.push_back({ vec4(v0, 0.0f), vec4(normal, colorFloat ) });
	vertices.push_back({ vec4(v1, 0.0f), vec4(normal, colorFloat ) });
	vertices.push_back({ vec4(v2, 0.0f), vec4(normal, colorFloat ) });
}

void DebugRenderer::GenerateSphere(std::vector<DebugVertexSolid3D>& output, const vec3& center, f32 radius, i32 longitude, i32 latitude, Color color, bool shaded)
{
    color.a = static_cast<f32>(shaded);
    u32 colorInt = color.ToU32();
    f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);

    f32 x, y, z, a, b, da, db;
    i32 ia, ib, iy;
    da = glm::two_pi<f32>() / static_cast<f32>(longitude);
    db = glm::pi<f32>() / static_cast<f32>(latitude - 1);

    std::vector<vec3> vertices;
    std::vector<vec3> normals;
    std::vector<u16> indices;

    // vertices
    for (b = -glm::half_pi<float>(), ib = 0; ib < latitude; ib++, b += db)
    {
        for (a = 0.f, ia = 0; ia < longitude; ia++, a += da)
        {
            x = cos(b) * cos(a);
            z = cos(b) * sin(a);
            y = sin(b);

            vec3 point(x, y, z);
            point *= radius;
            point += center;

            vertices.emplace_back(point);
            normals.emplace_back(glm::normalize(point - center));
        }
    }

    // indices
    for (iy = 0, ib = 1; ib < latitude; ib++)
    {
        for (ia = 1; ia < longitude; ia++, iy++)
        {
            indices.push_back(iy);
            indices.push_back(iy + longitude);
            indices.push_back(iy + 1);

            indices.push_back(iy + longitude);
            indices.push_back(iy + longitude + 1);
            indices.push_back(iy + 1);
        }

        indices.push_back(iy);
        indices.push_back(iy + longitude);
        indices.push_back(iy + 1 - longitude);

        indices.push_back(iy + longitude);
        indices.push_back(iy + 1);
        indices.push_back(iy - longitude + 1);
        iy++;
    }

    for (auto index : indices)
    {
        if (index >= 0 && index < vertices.size())
        {
            output.push_back({vec4{vertices[index], 0.0f}, vec4{normals[index], colorFloat}});
        }
    }
}

void DebugRenderer::GeneratePipe(std::vector<DebugVertexSolid3D>& output, const std::vector<vec3>& path, f32 radius, f32 rotationPerSegment, i32 segment, Color color, std::vector<vec3>& outVerticesDebug, bool shaded)
{
    color.a = static_cast<f32>(shaded);
    u32 colorInt = color.ToU32();
    f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<u16> indices;

    // vertices
    f32 rotationStep = 0.0f;
    for (size_t i = 0; i < path.size(); ++i)
    {
        vec3 currentPoint = path[i];
        vec3 nextPoint = (i + 1 < path.size()) ? path[i + 1]: path[i - 1];

        vec3 direction;
        if (i + 1 < path.size())
        {
            direction = glm::normalize(nextPoint - currentPoint);
        }
        else
        {
            // here the nextPoint is the previous
            direction = glm::normalize(currentPoint - nextPoint);
        }

        vec3 up = vec3(0.0f, 1.0f, 0.0f);
        vec3 side = glm::normalize(glm::cross(up, direction));
        up = glm::normalize(glm::cross(direction, side));

        rotationStep += rotationPerSegment;
        for (i32 j = 0; j <= segment; ++j)
        {
            f32 theta = static_cast<f32>(j) / static_cast<f32>(segment) * glm::two_pi<f32>() + rotationStep;
            vec3 vertex = currentPoint + radius * (side * cos(theta) + up * sin(theta));
            vertices.emplace_back(vertex);
            normals.emplace_back(glm::normalize(glm::cross(vertex, currentPoint)));
        }
    }

    // indices
    for (i32 i = 0; i < path.size() - 1; ++i)
    {
        i32 base = i * (segment + 1);
        i32 nextBase = (i + 1) * (segment + 1);

        for (i32 j = 0; j < segment; ++j)
        {
            indices.push_back(base + j);
            indices.push_back(nextBase + j);
            indices.push_back(nextBase + j + 1);

            indices.push_back(base + j);
            indices.push_back(nextBase + j + 1);
            indices.push_back(base + j + 1);
        }
    }

    outVerticesDebug = vertices;

    for (auto index : indices)
    {
        if (index >= 0 && index < vertices.size())
        {
            output.push_back({vec4{vertices[index], 0.0f}, vec4{normals[index], colorFloat}});
        }
    }
}

void DebugRenderer::GenerateRibbon(std::vector<DebugVertexSolid3D>& output, const std::vector<vec3>& path, const std::vector<f32>& roll, const std::vector<f32>& fov, f32 radius, Color color, const std::vector<Color>& acceleration, bool shaded)
{
    if (path.size() < 2)
        return;

    bool useAccelerationColor = path.size() == acceleration.size();

    color.a = static_cast<f32>(shaded);
    u32 colorInt = color.ToU32();
    f32 colorFloat = *reinterpret_cast<f32*>(&colorInt);

    std::vector<vec3> vertices;
    std::vector<vec3> normals;
    std::vector<u32> indices;
    std::vector<f32> colors;

    vertices.reserve(path.size() * 2);
    normals.reserve(path.size() * 2);
    indices.reserve((path.size() - 1) * 12);
    colors.reserve(path.size() * 2);

    for (u32 i = 0; i < path.size(); ++i)
    {
        vec3 up(0.0f, 1.0f, 0.0f);

        vec3 currentPoint = path[i];
        vec3 nextPoint = (i + 1 < path.size()) ? path[i + 1]: path[i - 1];

        vec3 direction;
        if (i + 1 < path.size())
        {
            direction = glm::normalize(nextPoint - currentPoint);
        }
        else
        {
            // here the nextPoint is the previous
            direction = glm::normalize(currentPoint - nextPoint);
        }

        vec3 side = glm::normalize(glm::cross(up, direction));
        up = glm::normalize(glm::cross(direction, side));

        f32 theta = 0.0f;
        if (!roll.empty())
        {
            u32 rollIndex = static_cast<u32>(static_cast<f32>(i) / static_cast<f32>(path.size()) * static_cast<f32>(roll.size()));
            theta = roll[rollIndex];
        }

        f32 width = 0.5f;
        if (!fov.empty())
        {
            u32 fovIndex = static_cast<u32>(static_cast<f32>(i) / static_cast<f32>(path.size()) * static_cast<f32>(fov.size()));
            width = glm::degrees(fov[fovIndex]) * width / 75.0f;
            width *= 1.25f;
        }

        vec3 offset = radius * width * (side * cos(theta) + up * sin(theta));

        vec3 left = path[i] - offset;
        vec3 right = path[i] + offset;

        vertices.push_back(left);
        vertices.push_back(right);

        if (useAccelerationColor)
        {
            Color accelerationColor = acceleration[i];

            accelerationColor.a = static_cast<f32>(shaded);
            u32 accelerationColorInt = accelerationColor.ToU32();
            f32 accelerationColorFloat = *reinterpret_cast<f32*>(&accelerationColorInt);

            colors.push_back(accelerationColorFloat);
            colors.push_back(accelerationColorFloat);
        }

        up = glm::normalize(glm::cross(direction, glm::normalize(offset)));
        normals.push_back(up);
        normals.push_back(up);

        if (i < path.size() - 1)
        {
            /*
             * (( i + 1 ) * 2)  O -----O  (( i + 1 ) * 2 + 1)
             *                  | \    |
             *                  |  \   |
             *                  |   \  |
             *                  |    \ |
             *       ( i * 2 )  O------O  ( i * 2 + 1 )
             */

            indices.push_back(i * 2);
            indices.push_back((i + 1) * 2);
            indices.push_back(i * 2 + 1);

            indices.push_back(i * 2 + 1);
            indices.push_back((i + 1) * 2);
            indices.push_back((i + 1) * 2 + 1);

            // other side
            indices.push_back(i * 2);
            indices.push_back(i * 2 + 1);
            indices.push_back((i + 1) * 2);

            indices.push_back((i + 1) * 2);
            indices.push_back(i * 2 + 1);
            indices.push_back((i + 1) * 2 + 1);
        }
    }

    u32 count = 0;
    for (auto index : indices)
    {
        if (count == 12)
            count = 0;

        vec3 normal = normals[index];
        if (count >= 6 && count < 12)
            normal = -normal;

        if (index < vertices.size())
        {
            f32 vertexColor = 0.0f;
            if (useAccelerationColor)
            {
                vertexColor = colors[index];
            }
            else
            {
                vertexColor = colorFloat;
            }

            output.push_back({vec4{vertices[index], 0.0f}, vec4{normal, vertexColor}});
        }

        count++;
    }
}

void DebugRenderer::RegisterCullingPassBufferUsage(Renderer::RenderGraphBuilder& builder)
{
	using BufferUsage = Renderer::BufferPassUsage;
	builder.Write(_gpuDebugVertices2D, BufferUsage::COMPUTE);
	builder.Write(_gpuDebugVertices2DArgumentBuffer, BufferUsage::COMPUTE);

	builder.Write(_gpuDebugVertices3D, BufferUsage::COMPUTE);
	builder.Write(_gpuDebugVertices3DArgumentBuffer, BufferUsage::COMPUTE);
}