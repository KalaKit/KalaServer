//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/graphics/window.hpp"
#include "KalaWindow/include/core/glm_global.hpp"
#include "KalaWindow/include/core/input.hpp"
#include "KalaWindow/include/ui/debug_ui.hpp"
#include "KalaWindow/include/graphics/opengl/opengl.hpp"
#include "KalaWindow/include/graphics/opengl/opengl_functions_core.hpp"

#include "graphics/render.hpp"

using namespace KalaWindow::Core;
using namespace KalaWindow::Graphics;
using namespace KalaWindow::Graphics::OpenGL;
using namespace KalaWindow::Graphics::OpenGLFunctions;
using namespace KalaWindow::UI;

using std::string;

static Window* window{};
static Input* input{};
static OpenGL_Context* context{};
static DebugUI* UI{};

static void Redraw();
static void ResizeProjectionMatrix();

namespace KalaServer::Graphics
{
	void Render::Initialize()
	{
		window = Window::Initialize(
			"window 1",
			vec2(1280, 720),
			nullptr,
			WindowState::WINDOW_HIDE,
			DpiContext::DPI_SYSTEM_AWARE);

		u32 windowID = window->GetID();

		input = Input::Initialize(windowID);

		OpenGL_Global::Initialize();
		context = OpenGL_Context::Initialize(windowID, 0);

		window->SetRedrawCallback(Redraw);
		window->SetResizeCallback(ResizeProjectionMatrix);

		UI = DebugUI::Initialize(windowID);

		window->SetWindowState(WindowState::WINDOW_NORMAL);
	}

	void Render::Run()
	{
		if (!window->IsIdle()) Redraw();
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

	UI->Render(window->GetID());

	context->SwapOpenGLBuffers();

	input->EndFrameUpdate();
}

void ResizeProjectionMatrix()
{

}