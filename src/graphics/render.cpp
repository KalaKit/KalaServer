//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/core/containers.hpp"
#include "KalaWindow/include/graphics/window_global.hpp"
#include "KalaWindow/include/graphics/window.hpp"
#include "KalaWindow/include/core/glm_global.hpp"
#include "KalaWindow/include/core/input.hpp"
#include "KalaWindow/include/graphics/opengl/opengl.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_functions_core.hpp"

#include "graphics/render.hpp"

using namespace KalaHeaders;

using namespace KalaWindow::Core;
using namespace KalaWindow::Graphics;
using namespace KalaWindow::Graphics::OpenGL;
using namespace KalaWindow::Graphics::OpenGLFunctions;

using std::string;

static void Redraw();
static void ResizeProjectionMatrix();

static void CreateNewWindow(const string& name);

namespace KalaServer::Graphics
{
	void Render::Initialize()
	{
		Window_Global::Initialize();
		Window_Global::SetVerboseLoggingState(true);

		OpenGL_Global::Initialize();

		CreateNewWindow("test 1");
		CreateNewWindow("test 2");
	}


	void Render::Run()
	{
		for (const auto& win : runtimeWindows)
		{
			win->Update();

			if (!win->IsIdle()
				&& !win->IsResizing())
			{
				Redraw();
			}
		}
	}

	void Render::Shutdown()
	{

	}
}

void Redraw()
{
	for (const auto& win : runtimeWindows)
	{
		if (!win
			|| (win
			&& !win->IsInitialized()))
		{
			continue;
		}

		OpenGL_Context* context = win->GetOpenGLContext();

		if (context
			&& context->IsInitialized())
		{
			context->MakeContextCurrent();

			glClearColor(0.29f, 0.36f, 0.85f, 1.0f); //light blue
			glClear(
				GL_COLOR_BUFFER_BIT
				| GL_DEPTH_BUFFER_BIT);

			context->SwapOpenGLBuffers();
		}

		
	}

	for (const auto& win : runtimeWindows)
	{
		if (!win
			|| (win
			&& !win->IsInitialized()))
		{
			continue;
		}

		Input* input = win->GetInput();

		if (input
			&& input->IsInitialized())
		{
			input->EndFrameUpdate();
		}
	}
}

void ResizeProjectionMatrix()
{

}

void CreateNewWindow(const string& name)
{
	Window* window = Window::Initialize(
		name,
		vec2(1280, 720),
		nullptr,
		WindowState::WINDOW_HIDE);

	if (!window)
	{
		KalaWindowCore::ForceClose(
			"Initialization error",
			"Failed to create a window!");

		return;
	}

	u32 windowID = window->GetID();

	Input* input = Input::Initialize(windowID);

	if (!input)
	{
		KalaWindowCore::ForceClose(
			"Initialization error",
			"Failed to set up input!");

		return;
	}

	OpenGL_Context* context = OpenGL_Context::Initialize(windowID, 0);
	context->SetVSyncState(VSyncState::VSYNC_ON);

	window->SetRedrawCallback(Redraw);
	window->SetResizeCallback(ResizeProjectionMatrix);

	window->BringToFocus();
}