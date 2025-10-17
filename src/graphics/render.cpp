//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>
#include <filesystem>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/registry.hpp"
#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/graphics/window_global.hpp"
#include "KalaWindow/include/graphics/window.hpp"
#include "KalaWindow/include/core/glm_global.hpp"
#include "KalaWindow/include/core/input.hpp"
#include "KalaWindow/include/graphics/opengl/opengl.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_texture.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_shader.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_functions_core.hpp"
#include "KalaWindow/include/graphics/opengl/shaders/shader_quad.hpp"

#include "graphics/render.hpp"

using namespace KalaHeaders;

using KalaWindow::Core::KalaWindowCore;
using KalaWindow::Core::Registry;
using KalaWindow::Core::Input;
using KalaWindow::Core::Key;
using KalaWindow::Graphics::Window_Global;
using KalaWindow::Graphics::Window;
using KalaWindow::Graphics::TargetType;
using KalaWindow::Graphics::WindowData;
using KalaWindow::Graphics::WindowState;
using KalaWindow::Graphics::OpenGL::OpenGL_Global;
using KalaWindow::Graphics::OpenGL::OpenGL_Context;
using KalaWindow::Graphics::OpenGL::OpenGL_Texture;
using KalaWindow::Graphics::TextureType;
using KalaWindow::Graphics::TextureFormat;
using KalaWindow::Graphics::OpenGL::OpenGL_Shader;
using KalaWindow::Graphics::OpenGL::ShaderData;
using KalaWindow::Graphics::OpenGL::ShaderType;
using KalaWindow::Graphics::OpenGL::VSyncState;
using KalaWindow::Graphics::OpenGL::Shader::shader_quad_vertex;
using KalaWindow::Graphics::OpenGL::Shader::shader_quad_fragment;
using namespace KalaWindow::Graphics::OpenGLFunctions;

using std::string;
using std::to_string;
using std::vector;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::current_path;

static void Redraw();
static void ResizeProjectionMatrix();

static Window* ownerWindow{};
static Window* CreateNewWindow(
	const string& name,
	Window* parentWindow = nullptr);

static vector<Window*> windows{};

//light blue background color
constexpr vec3 NORMALIZED_BACKGROUND_COLOR = vec3(0.29f, 0.36f, 0.85f);

namespace KalaServer::Graphics
{
	void Render::Initialize()
	{
		Window_Global::Initialize();
		OpenGL_Global::Initialize();

		Input::SetVerboseLoggingState(true);

		ownerWindow = CreateNewWindow("owner");

		/*
		Window* win1 = CreateNewWindow("test 1");
		Window* win2 = CreateNewWindow("test 2");

		WindowContent* content = windowContent[win1].get();
		OpenGL_Context* c1 = content->glContext
			? content->glContext.get()
			: content->parentGLContext;

		c1->MakeContextCurrent();

		string texPath = (current_path() / "files" / "UI" / "image1.png").string();

		OpenGL_Texture* tex = OpenGL_Texture::LoadTexture(
			win1->GetID(),
			"tex01",
			texPath,
			TextureType::Type_2D,
			TextureFormat::Format_RGBA8);

		OpenGL_Shader::CreateShader(
			win1->GetID(),
			"shader01",
			{ {
				{.shaderData = string(shader_quad_vertex), .type = ShaderType::SHADER_VERTEX },
				{.shaderData = string(shader_quad_fragment), .type = ShaderType::SHADER_FRAGMENT }
			} });
		*/
	}

	void Render::Run()
	{
		for (const auto& window : windows)
		{
			if (!window) continue;

			window->Update();
			u32 windowID = window->GetID();

			if (ownerWindow
				&& window == ownerWindow)
			{
				auto inputs = Input::registry.GetAllWindowContent(windowID);
				Input* input = inputs.empty() ? nullptr : inputs.front();

				static int globalIndex = 1;
				if (input->IsKeyPressed(Key::X))
				{
					for (int i = 1; i < 3; i++)
					{
						CreateNewWindow(
							"test " + to_string(globalIndex),
							ownerWindow);

						++globalIndex;
					}
				}
			}

			if (!window->IsIdle()
				&& !window->IsResizing())
			{
				Redraw();
			}
		}

		if (windows != Window::registry.runtimeContent) windows = Window::registry.runtimeContent;
	}

	void Render::Shutdown()
	{

	}
}

void Redraw()
{
	for (const auto& window : Window::registry.runtimeContent)
	{
		if (!window) continue;

		u32 windowID = window->GetID();

		auto contexts = OpenGL_Context::registry.GetAllWindowContent(windowID);
		OpenGL_Context* context = contexts.empty() ? nullptr : contexts.front();

		if (context)
		{
			context->MakeContextCurrent();

			glClearColor(
				NORMALIZED_BACKGROUND_COLOR.x,
				NORMALIZED_BACKGROUND_COLOR.y,
				NORMALIZED_BACKGROUND_COLOR.z,
				1.0f);
			glClear(
				GL_COLOR_BUFFER_BIT
				| GL_DEPTH_BUFFER_BIT);

			bool isChild = ownerWindow->IsChildWindow(window);
			if (!isChild)
			{

			}
			else
			{

			}

			context->SwapOpenGLBuffers();
		}

		auto inputs = Input::registry.GetAllWindowContent(windowID);
		Input* input = inputs.empty() ? nullptr : inputs.front();

		if (input) input->EndFrameUpdate();
	}
}

void ResizeProjectionMatrix()
{

}

Window* CreateNewWindow(
	const string& name,
	Window* parentWindow)
{
	Window* window = Window::Initialize(
		name,
		vec2(1280, 720),
		parentWindow,
		WindowState::WINDOW_HIDE);

	if (!window)
	{
		KalaWindowCore::ForceClose(
			"Initialization error",
			"Failed to create a window!");

		return nullptr;
	}

	u32 windowID = window->GetID();

	Input::Initialize(windowID);

	OpenGL_Context* context = OpenGL_Context::Initialize(windowID, 0);
	context->SetVSyncState(VSyncState::VSYNC_ON);

	window->SetRedrawCallback(Redraw);
	window->SetResizeCallback(ResizeProjectionMatrix);

	window->BringToFocus();

	return window;
	return nullptr;
}