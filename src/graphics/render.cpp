//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/graphics/window_global.hpp"
#include "KalaWindow/include/graphics/window.hpp"
#include "KalaWindow/include/core/glm_global.hpp"
#include "KalaWindow/include/core/input.hpp"
#include "KalaWindow/include/ui/debug_ui.hpp"
#include "KalaWindow/include/graphics/opengl/opengl.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_functions_core.hpp"

#include "graphics/render.hpp"

using namespace KalaHeaders;

using namespace KalaWindow::Core;
using namespace KalaWindow::Graphics;
using namespace KalaWindow::Graphics::OpenGL;
using namespace KalaWindow::Graphics::OpenGLFunctions;
using namespace KalaWindow::UI;

using std::string;

static Window* window{};

static void Redraw();
static void ResizeProjectionMatrix();

namespace KalaServer::Graphics
{
	void Render::Initialize()
	{
		Window_Global::Initialize();

		window = Window::Initialize(
			"window 1",
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

		OpenGL_Global::Initialize();
		OpenGL_Context* context = OpenGL_Context::Initialize(windowID, 0);
		context->SetVSyncState(VSyncState::VSYNC_ON);

		window->SetRedrawCallback(Redraw);
		window->SetResizeCallback(ResizeProjectionMatrix);

		DebugUI* UI = DebugUI::Initialize(windowID);

		window->BringToFocus();
	}

	void Render::Run()
	{
		window->Update();

		Input* input = window->GetInput();
		if (input->IsKeyPressed(Key::Space))
		{
			window->Flash(
				FlashTarget::TARGET_WINDOW,
				FlashType::FLASH_TIMED,
				5);

			window->Flash(
				FlashTarget::TARGET_TASKBAR,
				FlashType::FLASH_TIMED,
				5);
		}

		if (!window->IsIdle()
			&& !window->IsResizing())
		{
			Redraw();
		}
	}

	void Render::Shutdown()
	{

	}
}

void Redraw()
{
	glClearColor(0.29f, 0.36f, 0.85f, 1.0f); //light blue
	glClear(
		GL_COLOR_BUFFER_BIT
		| GL_DEPTH_BUFFER_BIT);

	window->GetDebugUI()->Render();

	window->GetOpenGLContext()->SwapOpenGLBuffers();

	window->GetInput()->EndFrameUpdate();
}

void ResizeProjectionMatrix()
{

}