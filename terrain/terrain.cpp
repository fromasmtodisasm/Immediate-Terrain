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

#include <QuadTree.h>
#include <Camera.h>
#include <set>
using namespace glm;

CCamera gCamera;
SDL_Window* window;
SDL_GLContext gl_context;
ImGuiIO io;

float K = 1.5; // split factor
vec2 point = vec2(0.25f, 0.25f);
vec2 quad_origin = vec2(0, 0);
float quad_size = 2.f;
float speed_factor = 0.1;
float POINT_SPEED = 0.001;
int DEPTH = 16;
bool is_wireframe = false;

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

	vec3 get_offset(Face f, vec2 of, float size)
	{
		vec3 o{ 0,0,0 };
		float& x = of.x;
		float& y = of.y;
		switch (f)
		{
		case Face::back:
			o = { -x, y, -size };
			break;
		case Face::right:
			o = { size, y, -x };
			break;
		case Face::left:
			o = { -size, y, x };
			break;
		case Face::top:
			o = { x, size, y };
			break;
		case Face::botoom:
			o = { -x, -size, -y };
			break;
		case Face::front:
			o = { x, y, size };
			break;
		default:
			assert(0);
			break;
		}
		return o;
	}

	Quad get_cube_face(Face f, float size, vec2 origin)
	{
		switch (f)
		{
			// White side - BACK
		case Face::back:
			return Quad({
			glm::vec3(size, -size, 0),
			glm::vec3(size, size, 0),
			glm::vec3(-size, size, 0),
			glm::vec3(-size, -size, 0),
			glm::vec3(   1.0,  1.0, 1.0 )
				});

			// Purple side - RIGHT
		case Face::right:
			return Quad({
				glm::vec3(0, -size, -size),
				glm::vec3(0, size, -size),
				glm::vec3(0, size, size),
				glm::vec3(0, -size, size),
				glm::vec3(  1.0,  0.0,  1.0 )
				});

			// Green side - LEFT
		case Face::left:
			return Quad({
			glm::vec3(0, -size, size),
			glm::vec3(0, size, size),
			glm::vec3(0, size, -size),
			glm::vec3(0, -size, -size),
			glm::vec3(   0.0,  1.0,  0.0 )
				});

			// Blue side - TOP
		case Face::top:
			return Quad({
			glm::vec3(size, 0, size),
			glm::vec3(size, 0, -size),
			glm::vec3(-size, 0, -size),
			glm::vec3(-size, 0, size),
			glm::vec3(   0.0,  0.0,  1.0 )
				});

			// Red side - BOTTOM
		case Face::botoom:
			return Quad({
			glm::vec3(size, 0, -size),
			glm::vec3(size, 0, size),
			glm::vec3(-size, 0, size),
			glm::vec3(-size, 0, -size),
			glm::vec3(   1.0,  0.0,  0.0 )
				});
		case Face::front:
			return Quad({
			 glm::vec3(size, -size, 0),      // P1 is red
			 glm::vec3(size, size, 0),      // P2 is green
			 glm::vec3(-size, size, 0),      // P3 is blue
			 glm::vec3(-size, -size, 0),      // P4 is 
			 glm::vec3(   0.0,  1.0, 1.0 )
				});
		default:
			assert(0);
			break;
		}
	}

	void render_quad(const Quad& quad)
	{
		glBegin(GL_QUADS);
		auto& q = quad;
		auto& c = quad.color;
	 
		glColor3f(c.r, c.g, c.b);
		glVertex3f(q.p1.x, q.p1.y, q.p1.z );      // P1 is red
		glVertex3f(q.p2.x, q.p2.y, q.p2.z );      // P2 is red
		glVertex3f(q.p3.x, q.p3.y, q.p3.z );      // P3 is red
		glVertex3f(q.p4.x, q.p4.y, q.p4.z );      // P4 is red
		glEnd();

	}

	void wireframe(bool mode)
	{
		int native = mode ? GL_LINE : GL_FILL;
		glPolygonMode(GL_FRONT_AND_BACK, native);
	}

	void draw_line(glm::vec3 p1, glm::vec3 p2, glm::vec3 color)
	{
		glColor3fv(&color[0]);
		glBegin(GL_LINES);
		glVertex3fv(&p1[0]);
		glVertex3fv(&p2[0]);
		glEnd();

	}

	void draw_axes(float size)
	{
		glLineWidth(4);
		size = 0.5 * size;
		draw_line({ 0, -size, 0 }, { 0, size, 0 }, {1,0,0});
		draw_line({ -size, 0, 0 }, { size, 0, 0 }, {0,1,0});
		draw_line({ 0, 0, -size }, { 0, 0, size }, {0,0,1});
		glLineWidth(1);
	}

	
// 0 ... 1 output range
__forceinline glm::vec2 world_coords_to_face_space(const float x,const float y,const float z) {
    return (glm::vec2(x/z,y/z)+1.0f)*0.5f;
}

// 0 ... 1 output range
__forceinline glm::vec2 world_coords_to_face_space(const Face face_type, const float x,const float y,const float z) {
    switch(face_type) // same as using coords_swizzle & unity_swizzle
    {
		case Face::right: return world_coords_to_face_space( y,  z,  glm::abs(x)); // px
    case Face::top: return world_coords_to_face_space(-x,  z,  glm::abs(y)); // py
    case Face::left: return world_coords_to_face_space(-y,  z,  glm::abs(x)); // nx
    case Face::botoom: return world_coords_to_face_space( x,  z,  glm::abs(y)); // ny
    case Face::back: return world_coords_to_face_space( y, -x,  glm::abs(z)); // pz
    case Face::front: return world_coords_to_face_space( y,  x,  glm::abs(z)); // nz
    default: break;
    };
    return glm::vec2(0.0);
}

}

class CRender : public IQuadTreeRender {
public:
	CRender()
	{

	}
  void draw_plane(double ox, double oy, double size, color3 color) override {
		auto q = get_cube_face(m_CurrentFace, 0.5 * size, vec2(ox, oy));
		q.color.r = color.r;
		q.color.g = color.g;
		q.color.b = color.b;

		vec3 origin = get_offset(m_CurrentFace, vec2(ox,oy), m_CurrentRadius);
		q.p1 = glm::normalize(q.p1 += origin)*m_CurrentRadius;;
		q.p2 = glm::normalize(q.p2 += origin)*m_CurrentRadius;;
		q.p3 = glm::normalize(q.p3 += origin)*m_CurrentRadius;;
		q.p4 = glm::normalize(q.p4 += origin)*m_CurrentRadius;;

		render_quad(q);
  }
	Face m_CurrentFace = Face::botoom;
	float m_CurrentRadius = 1;
};

class TreeRender : public ITreeVisitorCallback {
public:
  TreeRender(IQuadTreeRender *render) : render(render) {}
  virtual void BeforVisit(QuadTree *qt) {
		wireframe(is_wireframe);
	}
  virtual void AfterVisit(QuadTree *qt) {
		wireframe(false);
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


void draw_grid(int x_steps, int y_steps, float w, float h)
{
	float y_step_size = h / y_steps;
	float x_step_size = w / x_steps;
	glColor3f(0.8f, 0.8f, 0.8f);
	for (int y = 0; y < y_steps; y++)
	{
		glBegin(GL_TRIANGLE_STRIP);
		/*glVertex3f(-0.5 * w, 0, y * y_step_size - 0.5 * h + y_step_size);
		glVertex3f(-0.5 * w, 0, y * y_step_size - 0.5 * h);
		glVertex3f(-0.5 * w + x_step_size, 0, y * y_step_size - 0.5 * h);*/
		bool need_shift = false;
		int shift = 0;
		int x = 0;
		while (shift <= x_steps || !need_shift)
		{
			float vx = 0, vy = 0;
			if (x % 2 == 0)
			{
				vx = -0.5 * w + x_step_size * shift;
				vy = y * y_step_size - 0.5 * h;
				need_shift = false;
			}
			else
			{
				vx = -0.5 * w + x_step_size * shift;
				vy = y * y_step_size - 0.5 * h + y_step_size;
				need_shift = !need_shift;
			}
			glVertex3f(vx, 0, vy);
			shift = need_shift ? shift + 1 : shift;
			x++;
		}
		glEnd();
	}
}

int winW = 1280;
int winH = 720;

void glInit()
{
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	gCamera.updateCameraVectors();
	glLoadMatrixf(&gCamera.getProjectionMatrix()[0][0]);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&gCamera.getViewMatrix()[0][0]);
}
float rotate_x = 0;
float rotate_y = 0;

void update()
{
	point.x = glm::cos(speed_factor*POINT_SPEED*SDL_GetTicks());
	point.y = glm::sin(speed_factor*POINT_SPEED*SDL_GetTicks());
}

void display()
{
	glInit();
	rotate_x += 0.5;
	rotate_y += 0.5;
  // Reset transformations
	auto pos = gCamera.getPosition();
	auto point = pos + glm::normalize(gCamera.Front);
	auto up = gCamera.Up;

	std::vector<QuadTree> quadTrees;
	CRender render;
	render.m_CurrentRadius = 0.5 * quad_size;
	TreeRender treeRender = TreeRender(&render);

	for (int i = 0; i < 6; i++)
	{
		auto qt = QuadTree(DEPTH, quad_size, quad_origin.x, quad_origin.y, color3(1, 1, 0));
		auto p = world_coords_to_face_space(static_cast<Face>(i), ::point.x, 2, ::point.y);
		qt.split(p.x, p.y, K);
		quadTrees.push_back(qt);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	draw_grid(20, 20, 20, 20);
	draw_axes(20);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	for (int i = 0; i < 6; i++)
	{
		render.m_CurrentFace = static_cast<Face>(i);
		quadTrees[i].visit(&treeRender, 0, 0, 0);
	}
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

bool Init()
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return false;
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
    io = ImGui::GetIO(); (void)io;
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
		return true;

}

void Cleanup()
{
    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool ProcessEvents(std::set<int> &keycodes)
{
	bool done = false;
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
				is_wireframe = !is_wireframe;
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
#define offset 1 
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
		return done;
}
// Main code
int main(int, char**)
{
	if (Init())
	{
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.2, 0.2, 0.2, 1.00f);

    // Main loop
		std::set<int> keycodes;
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

			done = ProcessEvents(keycodes);

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

            ImGui::Text("Camera");
						ImGui::SliderAngle("Pitch", &gCamera.transform.rotation.x);
						ImGui::SliderAngle("Yaw", &gCamera.transform.rotation.y);
						ImGui::Separator();
            ImGui::SliderFloat("Split facotr", &K, 1.f, 3.f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::SliderInt("Depth", &DEPTH, 0, 32);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::SliderFloat("Radius", &quad_size, 1, 32);            // Edit 1 float using a slider from 0.0f to 1.0f
						ImGui::SliderFloat("FOV", &gCamera.FOV, 30.f, 150.f);
						ImGui::SliderFloat("point speed", &POINT_SPEED, 0.001f, 0.01f);
						ImGui::SliderFloat2("point", &point[0], 0.f, 1.f);
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
				update();

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
        SDL_GL_SwapWindow(window);
    }
		Cleanup();

	}
	return 0;
}


