#include "Cartograph.hpp"
#include "PartitionVisualization.hpp"
#include "Cartograph.hpp"
#include "EmbedResources.hpp"
#include "Interlink/Interlink.hpp"
#include "AtlasNet/AtlasNetBootstrap.hpp"
#include "Database/ShapeManifest.hpp"
void Cartograph::Startup()
{
	// glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	_glfwwindow = glfwCreateWindow(1920, 1080, "Cartograph", nullptr, nullptr);
	glfwMakeContextCurrent(_glfwwindow);
	glewInit();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImPlot3D::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	ImFontConfig fontconfig;
	fontconfig.SizePixels = 20.0f;
	io.Fonts->AddFontDefault(&fontconfig);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForOpenGL(_glfwwindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// Interlink::Get().Init(InterlinkProperties{.ThisID = InterLinkIdentifier::MakeIDCartograph(), .logger = logger, .callbacks = InterlinkCallbacks{}});
}
void Cartograph::DrawBackground()
{

	// Remove window padding and borders
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("MainDockspace", nullptr, window_flags);
	ImGui::PopStyleVar(2);

	// Create the dockspace
	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// Draw background image behind everything in this window
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 win_pos = ImGui::GetWindowPos();
	ImVec2 win_size = ImGui::GetWindowSize();

	// Get center of the window
	ImVec2 center = ImVec2(win_pos.x + win_size.x * 0.5f, win_pos.y + win_size.y * 0.5f);

	// Assuming the image has known size
	ImVec2 image_size(512, 512);
	float BorderOffset = 10;
	float alpha = 80;
	ImVec2 image_min(center.x - (image_size.x * 0.5f), center.y - (image_size.y * 0.5f));
	ImVec2 image_max(center.x + (image_size.x * 0.5f), center.y + (image_size.y * 0.5f));
	ImVec2 border_min(center.x - (image_size.x * 0.5f + BorderOffset), center.y - (image_size.y * 0.5f + BorderOffset));
	ImVec2 border_max(center.x + (image_size.x * 0.5f + BorderOffset), center.y + (image_size.y * 0.5f + BorderOffset));
	static auto backgroundImage = LoadTextureFromCompressed(EmbedResources::map_search_png, EmbedResources::map_search_size);
	// Draw it slightly transparent
	draw_list->AddImage((ImTextureID)backgroundImage->GetHandle(), image_min, image_max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
	draw_list->AddRect(border_min, border_max, IM_COL32(255, 255, 255, alpha), BorderOffset, 0, 2.0f);
	ImGui::End();
}
void Cartograph::DrawConnectTo()
{
	ImGui::Begin("ConnectTo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::InputTextWithHint("##connect", "IP address of Manager", &IP2Manager);
	ImGui::SameLine();
	if (ImGui::Button("Connect"))
	{
		ConnectTo(IP2Manager);
	}
	ImGui::End();
}
void Cartograph::DrawMap()
{
	ImGui::Begin("Map", nullptr);
	ImPlot::BeginPlot("Map View", ImVec2(-1, -1), ImPlotFlags_CanvasOnly);

	ImPlot::PushPlotClipRect();
	for (const auto &partition : partitions)
	{
		for (const auto &tri : partition.shape.triangles)
		{
			ImVec2 points[3];
			for (int i = 0; i < 3; i++)
			{
				points[i] = ImPlot::PlotToPixels(ImVec2{tri[i].x, tri[i].y});
			}
			ImPlot::GetPlotDrawList()->AddTriangle(points[0], points[1], points[2], IM_COL32(255, 255, 255, 255));
		}
	}
	ImPlot::PopPlotClipRect();
	ImPlot::EndPlot();
	ImGui::End();
}
void Cartograph::DrawPartitionGrid()
{
	ImGui::Begin("Partitions");

	ImGui::End();
}
void Cartograph::DrawLog()
{
	ImGui::Begin("Log");
	ImGui::End();
}
void Cartograph::DrawOptions()
{
	ImGui::Begin("Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::InputInt("Polling Rate", &pollingRate, 100, 1000))
	{
		pollingRate = std::max(pollingRate, 100);
	}
	ImGui::End();
}
void Cartograph::Update()
{
	if (!database)
		return;

	const auto shapes = ShapeManifest::FetchAll(database.get());

	partitions.clear();

	for (const auto &shape : shapes)
	{
		ActivePartition partition;
		partition.shape = Shape::FromString(shape.second);
		partition.Name = shape.first;
		partitions.push_back(partition);
	}

	GetNodes();
}
void Cartograph::Render()
{
	DrawBackground();
	DrawConnectTo();
	DrawMap();
	DrawPartitionGrid();
	DrawLog();
	DrawOptions();
}
void Cartograph::ConnectTo(const std::string &ip)
{
	ForEachContainer([&database = database, IP2Manager = IP2Manager](const Container &c, const Json &detail)
					 {
		if (c.image == AtlasNetBootstrap::Get().DatabaseImageName)
		{
			database = std::make_unique<RedisCacheDatabase>(false,IP2Manager,c.Out2InPorts.front().first);
			database->Connect();
			return;
		} });
	
	if (!database)
	{
		std::cerr << "Database not found " << std::endl;
	}
}
std::shared_ptr<Texture> Cartograph::LoadTextureFromCompressed(const void *data, size_t size)
{
	std::shared_ptr<Texture> tex = std::make_shared<Texture>();
	ivec2 Dimensions;
	int channelCount;
	void *imageData = stbi_load_from_memory((const uint8 *)data, size, &Dimensions.x, &Dimensions.y, &channelCount, 4);
	tex->Alloc2D(Dimensions, eRGBA, GL_RGBA8_SNORM, imageData);
	tex->GenerateMipmaps();
	stbi_image_free(imageData);
	return tex;
}

void Cartograph::GetNodes()
{
	std::string nodesJson = DockerIO::Get().request("GET", "/nodes");
	Json nodes = Json::parse(nodesJson);
	workers.clear();
	int i = 0;
	for (auto &node : nodes)
	{
		NodeWorker w;

		w.id = node["ID"];
		w.hostname = node["Description"]["Hostname"];
		w.addr = node["Status"]["Addr"];
		w.Color = HSVtoRGB(GetUniqueColor(i++));
		workers.push_back(w);
	}
}

void Cartograph::ForEachContainer(std::function<void(const Cartograph::Container &, const Json &)> func)
{
	std::string response = DockerIO::Get().request("GET", "/containers/json");

	Json containers;
	try
	{
		containers = Json::parse(response);
	}
	catch (...)
	{
		std::cerr << "❌ Failed to parse Docker JSON response\n";
		return;
	}

	for (auto &container : containers)
	{
		if (!container.contains("Id") || !container.contains("Names"))
			continue;

		std::string id = container["Id"].get<std::string>();
		std::string name = container["Names"][0].get<std::string>();
		if (!name.empty() && name[0] == '/')
			name.erase(0, 1); // remove leading '/'

		// Query detailed info for each container
		std::string inspectRes = DockerIO::Get().request("GET", "/containers/" + id + "/json");

		Json detail = Json::parse(inspectRes);
		// std::cerr << detail.dump(4) << std::endl;
		std::string imageName = detail["Config"]["Image"].get<std::string>();
		imageName = imageName.substr(imageName.find_first_of('/') + 1);
		imageName = imageName.substr(0, imageName.find_first_of(':'));

		auto networks = detail["NetworkSettings"]["Networks"];

		Container c;
		c.image = imageName;

		if (networks.contains("AtlasNet") && networks["AtlasNet"].contains("IPAddress"))
			c.IP = networks["AtlasNet"]["IPAddress"].get<std::string>();
		c.name = detail["Name"];
		c.ID = detail["Id"];
		if (detail["Config"].contains("Labels") && detail["Config"]["Labels"].contains("com.docker.swarm.node.id"))
			c.Node = detail["Config"]["Labels"]["com.docker.swarm.node.id"];
		for (const auto &[key, value] : detail["NetworkSettings"]["Ports"].items())
		{
			const std::string outPort = key.substr(0, key.find_first_of('/'));
			if (value.empty())
				continue;
			const std::string inPort = value.front()["HostPort"];
			c.Out2InPorts.push_back({std::stoi(outPort), std::stoi(inPort)});
		}

		func(c, detail);
	}
}
vec3 Cartograph::HSVtoRGB(vec3 vec)
{
	float h = vec.x, s = vec.y, v = vec.z;
	float c = v * s;
	float x = c * (1 - fabs(fmod(h * 6.0f, 2.0f) - 1));
	float m = v - c;
	float r, g, b;
	if (h < 1.0 / 6)
	{
		r = c;
		g = x;
		b = 0;
	}
	else if (h < 2.0 / 6)
	{
		r = x;
		g = c;
		b = 0;
	}
	else if (h < 3.0 / 6)
	{
		r = 0;
		g = c;
		b = x;
	}
	else if (h < 4.0 / 6)
	{
		r = 0;
		g = x;
		b = c;
	}
	else if (h < 5.0 / 6)
	{
		r = x;
		g = 0;
		b = c;
	}
	else
	{
		r = c;
		g = 0;
		b = x;
	}
	return {r + m, g + m, b + m};
}
vec3 Cartograph::GetUniqueColor(int index)
{
	// Use a pseudo-random but deterministic hue distribution (golden ratio)
	const float golden_ratio_conjugate = 0.61803398875f;
	float hue = fmodf(index * golden_ratio_conjugate, 1.0f);
	float saturation = 0.6f;
	float value = 0.95f;
	return HSVtoRGB({hue, saturation, value});
}
void Cartograph::Run()
{
	Startup();
	auto lastUpdateCall = std::chrono::steady_clock::now();

	while (!glfwWindowShouldClose(_glfwwindow))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateCall).count();
		if (elapsed >= pollingRate)
		{
			lastUpdateCall = now;
			Update();
		}

		Update();
		Render();

		ImGui::ShowDemoWindow();
		ImPlot::ShowDemoWindow();
		ImPlot3D::ShowDemoWindow();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(_glfwwindow);
		glfwPollEvents();
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow *backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}
}