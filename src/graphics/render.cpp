//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/core/crash.hpp"
#include "KalaWindow/include/core/containers.hpp"
#include "KalaWindow/include/graphics/window_global.hpp"
#include "KalaWindow/include/graphics/window.hpp"
#include "KalaWindow/include/core/glm_global.hpp"
#include "KalaWindow/include/core/input.hpp"
#include "KalaWindow/include/graphics/opengl/opengl.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_functions_core.hpp"

#include "graphics/render.hpp"

using namespace KalaHeaders;

using KalaWindow::Core::KalaWindowCore;
using KalaWindow::Core::CrashHandler;
using KalaWindow::Core::Input;
using KalaWindow::Core::WindowContent;
using KalaWindow::Core::runtimeWindows;
using KalaWindow::Core::windowContent;
using KalaWindow::Graphics::Window_Global;
using KalaWindow::Graphics::Window;
using KalaWindow::Graphics::WindowState;
using KalaWindow::Graphics::OpenGL::OpenGL_Global;
using KalaWindow::Graphics::OpenGL::OpenGL_Context;
using KalaWindow::Graphics::OpenGL::VSyncState;
using namespace KalaWindow::Graphics::OpenGLFunctions;

using std::string;

static void Redraw();
static void ResizeProjectionMatrix();

static Window* CreateNewWindow(const string& name);

namespace KalaServer::Graphics
{
	void Render::Initialize()
	{
		CrashHandler::Initialize(
			"KalaServer",
			Shutdown);
		KalaWindowCore::SetUserShutdownFunction(Shutdown);

		Window_Global::Initialize();
		Window_Global::SetVerboseLoggingState(true);

		OpenGL_Global::Initialize();

		Window* win1 = CreateNewWindow("test 1");
		Window* win2 = CreateNewWindow("test 2");
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

		WindowContent* content = windowContent[win].get();
		OpenGL_Context* context = content->glContext.get();

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

		WindowContent* content = windowContent[win].get();
		Input* input = content->input.get();

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

Window* CreateNewWindow(const string& name)
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

		return nullptr;
	}

	u32 windowID = window->GetID();

	Input* input = Input::Initialize(windowID);

	if (!input)
	{
		KalaWindowCore::ForceClose(
			"Initialization error",
			"Failed to set up input!");

		return nullptr;
	}

	OpenGL_Context* context = OpenGL_Context::Initialize(windowID, 0);
	context->SetVSyncState(VSyncState::VSYNC_ON);

	window->SetRedrawCallback(Redraw);
	window->SetResizeCallback(ResizeProjectionMatrix);

	window->BringToFocus();

	return window;
}