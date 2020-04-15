// dear imgui: standalone example application for SDL2 + OpenGL
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_sdl_opengl3/ folder**
// See imgui_impl_sdl.cpp for details.

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <gl/GL.h>
#include <gl/GLU.h>

#include <QuadTree.h>
#include <Camera.h>
#include <set>
using namespace glm;

CCamera gCamera;
SDL_Window* window;

namespace
{
	enum class Face
	{
		right, //px
		top, //py
		back, //pz
		left, //nx
		botoom, //ny
		front  //nz
	};

	struct Quad
	{
		vec3 p1, p2, p3, p4;
		vec3 color;
		Quad(vec3 p1, vec3 p2, vec3 p3, vec3 p4, vec3 color) : p1(p1), p2(p2), p3(p3), p4(p4), color(color) {}
	};

	static const float radii = 1;

	Quad get_cube_face(Face f)
	{
		switch (f)
		{
			// White side - BACK
		case Face::back:
			return Quad({
			glm::vec3(radii, -radii, radii),
			glm::vec3(radii, radii, radii),
			glm::vec3(-radii, radii, radii),
			glm::vec3(-radii, -radii, radii),
			glm::vec3(   1.0,  1.0, 1.0 )
				});

			// Purple side - RIGHT
		case Face::right:
			return Quad({
				glm::vec3(radii, -radii, -radii),
				glm::vec3(radii, radii, -radii),
				glm::vec3(radii, radii, radii),
				glm::vec3(radii, -radii, radii),
				glm::vec3(  1.0,  0.0,  1.0 )
				});

			// Green side - LEFT
		case Face::left:
			return Quad({
			glm::vec3(-radii, -radii, radii),
			glm::vec3(-radii, radii, radii),
			glm::vec3(-radii, radii, -radii),
			glm::vec3(-radii, -radii, -radii),
			glm::vec3(   0.0,  1.0,  0.0 )
				});

			// Blue side - TOP
		case Face::top:
			return Quad({
			glm::vec3(radii, radii, radii),
			glm::vec3(radii, radii, -radii),
			glm::vec3(-radii, radii, -radii),
			glm::vec3(-radii, radii, radii),
			glm::vec3(   0.0,  0.0,  1.0 )
				});

			// Red side - BOTTOM
		case Face::botoom:
			return Quad({
			glm::vec3(radii, -radii, -radii),
			glm::vec3(radii, -radii, radii),
			glm::vec3(-radii, -radii, radii),
			glm::vec3(-radii, -radii, -radii),
			glm::vec3(   1.0,  0.0,  0.0 )
				});
		case Face::front:
			return Quad({
			 glm::vec3(radii, -radii, -radii),      // P1 is red
			 glm::vec3(radii, radii, -radii),      // P2 is green
			 glm::vec3(-radii, radii, -radii),      // P3 is blue
			 glm::vec3(-radii, -radii, -radii),      // P4 is 
			 glm::vec3(   0.0,  1.0, 1.0 )
				});
		default:
			assert(0);
			break;
		}
	}

	void render_quad(const Quad& quad)
	{
		glBegin(GL_POLYGON);
		auto& q = quad;
		auto& c = quad.color;
	 
		glColor3f(c.r, c.g, c.b);
		glVertex3f(q.p1.x, q.p1.y, q.p1.z );      // P1 is red
		glVertex3f(q.p2.x, q.p2.y, q.p2.z );      // P2 is red
		glVertex3f(q.p3.x, q.p3.y, q.p3.z );      // P3 is red
		glVertex3f(q.p4.x, q.p4.y, q.p4.z );      // P4 is red
	 
		glEnd();

	}
}

class CRender : public IQuadTreeRender {
public:
	CRender()
	{

	}
  void draw_plane(double ox, double oy, double size, color3 color) override {
		glTranslatef(ox, 0, oy);
		glScalef(size, size, size);
		render_quad(get_cube_face(Face::botoom));
		SDL_GL_SwapWindow(window);

		//m_Plane->moveTo(glm::vec3(ox, 0, oy));
		//m_Plane->scale(glm::vec3(size, size, size));
  }
};

class TreeRender : public ITreeVisitorCallback {
public:
  TreeRender(IQuadTreeRender *render) : render(render) {}
  // Inherited via ITreeVisitorCallback
  virtual void BeforVisit(QuadTree *qt) override {

  }
  virtual void OnLeaf(QuadTree *qt, bool is_last, int level) override {
    render->draw_plane(qt->m_x, qt->m_y, qt->m_size, qt->m_color);
  }

  IQuadTreeRender *render = nullptr;
};

/*
class TreeObject : public Object
{
	virtual void draw(void* camera) final
	{

	}
};
*/


int winW = 1280;
int winH = 720;

void glInit()
{
	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45, (GLfloat)winW / (GLfloat)winH, 0.1, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gCamera.updateCameraVectors();
}
float rotate_x = 0;
float rotate_y = 0;

void display()
{
	rotate_x += 0.5;
	rotate_y += 0.5;
  // Reset transformations
	glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
	auto pos = gCamera.getPosition();
	auto point = pos + glm::normalize(gCamera.Front);
	auto up = gCamera.Up;
	gluLookAt(
		pos.x, pos.y, pos.z,
		/*0,0,0,*/
		point.x, point.y, point.z,
		up.x, up.y, up.z
	);

	QuadTree quadTree = QuadTree(8, 2, 0, 0, color3(1, 0, 0));
	CRender render;
	TreeRender treeRender = TreeRender(&render);
	quadTree.split(0.5, 0.5, 1.5);
	quadTree.visit(&treeRender, 0, 0, 0);

  // Other Transformations
  // glTranslatef( 0.1, 0.0, 0.0 );      // Not included
  // glRotatef( 180, 0.0, 1.0, 0.0 );    // Not included

  // Rotate when user changes rotate_x and rotate_y
  //glRotatef( rotate_x, 1.0, 0.0, 0.0 );
#if 0
  glRotatef( rotate_y, 0.0, 1.0, 0.0 );

  // Other Transformations
  glScalef( 2.0, 2.0, 2.0 );          // Not included

  // Yellow side - FRONT
	render_quad(get_cube_face(Face::front));

  // White side - BACK
	render_quad(get_cube_face(Face::back));
 
  // Purple side - RIGHT
	render_quad(get_cube_face(Face::right));
 
  // Green side - LEFT
	render_quad(get_cube_face(Face::left));
 
  // Blue side - TOP
	render_quad(get_cube_face(Face::top));
 
  // Red side - BOTTOM
	render_quad(get_cube_face(Face::botoom));
#endif
}
#undef main

// Main code
int main(int, char**)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | /*SDL_WINDOW_RESIZABLE |*/ SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

		glInit();

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
		std::set<int> keycodes;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
						switch (event.type)
						{
						case SDL_KEYDOWN:
						{
							keycodes.insert(event.key.keysym.scancode);
							break;
						}
						case SDL_KEYUP:
						{
							keycodes.erase(event.key.keysym.scancode);
							break;
						}
						default:
							break;
						}
        }
				for (auto key : keycodes)
				{
					switch (key)
					{
					case SDL_SCANCODE_F1:
						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						break;
					case SDL_SCANCODE_F2:
						glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
						break;
					case SDL_SCANCODE_W:
						gCamera.ProcessKeyboard(Movement::FORWARD, SDL_GetTicks());
						break;
					case SDL_SCANCODE_S:
						gCamera.ProcessKeyboard(Movement::BACKWARD, SDL_GetTicks());
						break;
					case SDL_SCANCODE_A:
						gCamera.ProcessKeyboard(Movement::LEFT, SDL_GetTicks());
						break;
					case SDL_SCANCODE_D:
						gCamera.ProcessKeyboard(Movement::RIGHT, SDL_GetTicks());
						break;
#define offset 0.5
					case SDL_SCANCODE_UP:
					case SDL_SCANCODE_K:
						gCamera.ProcessMouseMovement(0, offset);
						break;
					case SDL_SCANCODE_DOWN:
					case SDL_SCANCODE_J:
						gCamera.ProcessMouseMovement(0, -offset);
						break;
					case SDL_SCANCODE_LEFT:
					case SDL_SCANCODE_H:
						gCamera.ProcessMouseMovement(-offset, 0);
						break;
					case SDL_SCANCODE_RIGHT:
					case SDL_SCANCODE_L:
						gCamera.ProcessMouseMovement(offset, 0);
						break;
					case SDL_SCANCODE_SPACE:
						gCamera.mode = gCamera.mode == CCamera::Mode::FPS ? CCamera::Mode::FLY : CCamera::Mode::FPS;
						break;
					default:
						break;
#undef offset
					}
				}

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
						ImGui::SliderFloat3("cam_front", &gCamera.Front[0], -1.0, 1.0);
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

				//gluLookAt(-5, 15, -10, 0, 0, 0, 0, 1, 0);

				glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);



        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
				display();

        //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        //SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


