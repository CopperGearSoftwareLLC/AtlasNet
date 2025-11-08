#include "Cartograph.hpp"
#include "PartitionVisualization.hpp"
#include "Cartograph.hpp"
#include "EmbedResources.hpp"
#include "Interlink/Interlink.hpp"
#include "AtlasNet/AtlasNetBootstrap.hpp"
#include "Database/ShapeManifest.hpp"
#include "Database/EntityManifest.hpp"
void Cartograph::Startup()
{
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

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
	ImGui::LoadIniSettingsFromMemory(InitialLayout.data());

	int width, height, channels;
	unsigned char *pixels = stbi_load_from_memory(EmbedResources::map_search_png, EmbedResources::map_search_size, &width, &height, &channels, 4);
	if (!pixels)
	{
		std::cerr << "Failed to load icon image\n";
	}
	else
	{
		GLFWimage icon;
		icon.width = width;
		icon.height = height;
		icon.pixels = pixels;

		// Set the window icon
		glfwSetWindowIcon(_glfwwindow, 1, &icon);

		// Free the image memory after setting
		stbi_image_free(pixels);
	}

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
	if (ImGui::Button("Connect"))
	{
		ConnectTo(IP2Manager);
	}
	ImGui::SameLine();
	ImGui::InputTextWithHint("##connect", "IP address of Manager", &IP2Manager);
	ImGui::SameLine();
	vec4 Color = vec4(1.0f);
	if (connectionState == ConnectionState::eConnected)
		Color = vec4(0.1f, 1.0f, 0.1f, 1.0f);
	if (connectionState == ConnectionState::eFailed)
		Color = vec4(1.0f, 0.1f, 0.1f, 1.0f);
	ImGui::TextColored(ImVec4(Color.r, Color.g, Color.b, Color.a), "%s", connectionLog.c_str());
	ImGui::End();
}
void Cartograph::DrawMap()
{
	ImGui::Begin("Map", nullptr);
	static bool ShowNames = true;
	ImGui::Checkbox("Names", &ShowNames);
	if (ImPlot::BeginPlot("Map View", ImVec2(-1, -1), ImPlotFlags_CanvasOnly | ImPlotFlags_Equal))
	{
		ImPlot::PushPlotClipRect();
		int i = 0;
		for (auto &partition : partitions)
		{
			bool selected = !PartitionIndexSelected.has_value() || (i++) == PartitionIndexSelected.value();
			float PartitionAlpha = 1.0f;
			vec4 finalColor = vec4(partition.UniquePartitionColor, PartitionAlpha) * (selected ? vec4(1.0f) : vec4(0.6f));
			DrawToPlot(ImPlot::GetPlotDrawList(), partition.shape, mat4(1.0f), finalColor, 0.0f, 2.0f);
			for (const auto &entity : partition.entities)
			{
				ImVec2 pixelOffset;
				ImPlot::Annotation(entity.Position.x, entity.Position.z, GlmToImGui(finalColor), pixelOffset, false, "%ui", entity.ID);
				logger->DebugFormatted("entity at {}", glm::to_string(entity.Position));
			}
			const auto bounds = partition.shape.getBounds();
			const vec2 center = (bounds.first + bounds.second) / 2.0f;
			partition.ScreenSpaceShapeCenter = ImGuiToGlm(ImPlot::PlotToPixels(GlmToImGui(center)));
			if (ShowNames && selected)
				ImPlot::Annotation(center.x, center.y, GlmToImGui(finalColor), ImVec2(0, 0), false, "%s", partition.Name.c_str());
		}
		ImPlot::PopPlotClipRect();
		ImPlot::EndPlot();
	}

	ImGui::End();
}
void Cartograph::DrawPartitionGrid()
{

	static int32 ItemCountX = 2;
	static float ItemSpacing = 5.0f;
	ImGui::Begin("Partitions");

	if (ImGui::InputInt("Num X", &ItemCountX, 1, 3))
	{
		ItemCountX = glm::max(ItemCountX, 1);
	}
	if (ImGui::InputFloat("Min spacing", &ItemSpacing, 1, 10, "%.0f"))
	{
		ItemSpacing = glm::max(ItemSpacing, 1.0f);
	}

	ImGui::Separator();
	bool AnywindowHovered = false;
	if (ImGui::BeginChild("Partition grid"))
	{
		const ImVec2 AvailSpace = ImGui::GetContentRegionAvail();
		const vec2 gAvailSpace = {AvailSpace.x, AvailSpace.y};
		// ItemSize + MinItemSpacing because each has half of the spacing to either side
		const float ItemSizeX = glm::max(gAvailSpace.x / (float)ItemCountX - ItemSpacing, 0.0f);

		const vec2 OriginalCursorPos = ImGuiToGlm(ImGui::GetCursorPos());
		vec2 cursor = OriginalCursorPos;
		const size_t partitionCount = partitions.size();

		const auto origTest = ImGui::GetWindowPos();
		for (int y = 0; y < glm::ceil(partitionCount / (float)ItemCountX); y++)
		{
			cursor.x = OriginalCursorPos.x;
			cursor.y += ItemSpacing / 2.0f;
			for (int x = 0; x < ItemCountX; x++)
			{
				const int ID = x + y * ItemCountX;
				if (ID >= partitionCount)
					break;
				const ActivePartition &part = partitions[ID];
				const vec4 Color = vec4(part.UniquePartitionColor, 1.0f);
				// const NodeWorker& worker = workers.at(part.NodeID);

				cursor.x += ItemSpacing / 2.0f;
				ImGui::SetCursorPos(GlmToImGui(cursor));
				//::PushStyleColor(ImGuiCol_Border,GlmToImGui(vec4(worker.Color,1)));

				if (ImGui::BeginChild(ID, GlmToImGui(vec2(ItemSizeX)), ImGuiChildFlags_Borders))
				{
					if (ImGui::IsWindowHovered())
					{

						const vec2 itemCenter = ImGuiToGlm(ImGui::GetWindowPos()) + ImGuiToGlm(ImGui::GetWindowSize()) / 2.0f;
						const vec2 PartitionShapeMapCenter = part.ScreenSpaceShapeCenter;
						AnywindowHovered = true;
						PartitionIndexSelected = ID;
						ImGui::GetForegroundDrawList()->AddLine(ImGui::GetMousePos(), GlmToImGui(PartitionShapeMapCenter), ImGui::ColorConvertFloat4ToU32(GlmToImGui(Color)), 2.0f);
						
						if (ImGui::BeginTooltip())
						{
							ImGui::Text("ID: %s",part.Name.c_str());
							ImGui::Text("Node ID: %s",part.NodeID.c_str());
							ImGui::EndTooltip();
						}
					}

					const auto availSpace = ImGuiToGlm(ImGui::GetContentRegionAvail());

					auto bounds = part.shape.getBounds();
					mat4 transform = FitBoundsToTarget(glm::min(bounds.first, bounds.second),
													   glm::max(bounds.first, bounds.second),
													   vec2(availSpace * 0.5f),
													   0.05f); // small padding, e.g. 5%
					transform = glm::translate(glm::mat4(1.0f), glm::vec3(ImGuiToGlm(ImGui::GetCursorScreenPos()) + vec2(availSpace / 2.0f), 0.0f)) * transform;

					DrawTo(ImGui::GetWindowDrawList(), part.shape, transform, Color);
				}
				// ImGui::PopID();
				ImGui::EndChild();

				// ImGui::PopStyleColor();
				cursor.x += ItemSizeX + ItemSpacing / 2.0f;
			}
			cursor.y += ItemSpacing / 2.0f + ItemSizeX;
		}
	}
	if (!AnywindowHovered)
	{
		PartitionIndexSelected.reset();
	}
	ImGui::EndChild();
	// ImGui::SetCursorPos(GlmToImGui(OriginalCursorPos));

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

	const auto shapes = GridCellManifest::FetchAll(database.get());

	partitions.clear();

	int i = 0;
	for (const auto &shape : shapes)
	{
		ActivePartition partition;
		// partition.shape = Shape::FromString(shape.second);
		partition.shape = GridCellManifest::ParseGridShape(shape.second);
		partition.Name = shape.first;
		partition.UniquePartitionColor = GetUniqueColor(i++);
		partitions.push_back(partition);
	}
	std::sort(partitions.begin(), partitions.end(), [](const ActivePartition &p1, const ActivePartition &p2)
			  {bool nc = p1.NodeID < p2.NodeID;
				if (p1.NodeID != p2.NodeID)
				{
					return p1.NodeID < p2.NodeID;
				}
				else
				{
					return p1.Name < p2.Name;
				} });

	GetNodes();
	GetEntities();
}
void Cartograph::Render()
{
	if (ImGui::IsKeyPressed(ImGuiKey_D) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
	{
		DebugMode = !DebugMode;
	}
	DrawBackground();
	DrawConnectTo();
	DrawMap();
	DrawPartitionGrid();
	DrawLog();
	DrawOptions();
}
void Cartograph::ConnectTo(const std::string &ip)
{
	std::string DatabaseIP;
	ForEachContainer([&database = database, IP2Manager = IP2Manager, &DatabaseIP](const Container &c, const Json &detail)
					 {
		if (c.image == AtlasNetBootstrap::Get().DatabaseImageName)
		{
			DatabaseIP = IP2Manager + ":" + std::to_string(c.Out2InPorts.front().first);
			database = std::make_unique<RedisCacheDatabase>(false,IP2Manager,c.Out2InPorts.front().first);
			database->Connect();
			
			return;
		} });

	if (!database)
	{
		std::cerr << "Database not found " << std::endl;
		connectionState = ConnectionState::eFailed;
		connectionLog = "Failed to find database at " + IP2Manager;
	}
	else
	{
		connectionState = ConnectionState::eConnected;
		connectionLog = "Connected";
	}
}
void Cartograph::DrawTo(ImDrawList *drawlist, const GridShape &sh, const mat4 &transform, vec4 Color, float rounding, float thickness)
{
	for (const auto &cell : sh.cells)
	{

		vec2 finalMin = (vec2)(transform * vec4(cell.min, 0, 1.0f));
		vec2 finalMax = (vec2)(transform * vec4(cell.max, 0, 1.0f));
		drawlist->AddRect((GlmToImGui(finalMin)), (GlmToImGui(finalMax)), ImGui::ColorConvertFloat4ToU32(GlmToImGui(Color)), rounding, 0, thickness);

		// ImPlot::GetPlotDrawList()->AddTriangle(points[0], points[1], points[2], IM_COL32(255, 255, 255, 255));
	}
}
void Cartograph::DrawToPlot(ImDrawList *drawlist, const GridShape &sh, const mat4 &transform, vec4 Color, float rounding, float thickness)
{
	GridShape sh2;
	for (const auto &c : sh.cells)
	{
		GridCell c2;
		c2.col = c.col;
		c2.row = c.row;
		c2.min = ImGuiToGlm(ImPlot::PlotToPixels(GlmToImGui(c.min)));
		c2.max = ImGuiToGlm(ImPlot::PlotToPixels(GlmToImGui(c.max)));
		sh2.cells.push_back(c2);
	}
	DrawTo(drawlist, sh2, transform, Color, rounding, thickness);
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
		workers.insert(std::make_pair(w.id, w));
	}
}

void Cartograph::GetEntities()
{
	std::unordered_map<std::string, std::vector<AtlasEntity>> entities;
	logger->DebugFormatted("Getting Entities");

	EntityManifest::GetAllEntitiesSnapshot(database.get(), entities);
	logger->DebugFormatted("{} partitions with entities", entities.size());
	for (const auto &partition : entities)
	{
		std::string partitionName = partition.first;
		partitionName[partitionName.find_first_of('_')] = ' ';
		bool found = false;
		for (auto &activepart : partitions)
		{
			if (partitionName == activepart.Name)
			{
				found = true;
				for (const auto &entity : partition.second)
				{
					activepart.entities.push_back(entity);
				}
				break;
			}
		}
		if (!found) logger->ErrorFormatted("Didnt find {}. options where:\n",partitionName);
		
		for (const auto &activepart : partitions)
		{
			logger->ErrorFormatted("{}",activepart.Name);
		}
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
glm::mat4 Cartograph::FitBoundsToTarget(const glm::vec2 &minBound, const glm::vec2 &maxBound, const glm::vec2 &targetHalf, float padding)

{
	// --- center and size of source bounds ---
	glm::vec2 center = 0.5f * (minBound + maxBound);
	glm::vec2 size = glm::max(glm::abs(maxBound - minBound), glm::vec2(1e-12f)); // avoid div-by-zero

	// --- effective half-extent after padding (padding is fraction of half-extent) ---
	float pad = glm::clamp(padding, 0.0f, 0.49f);
	glm::vec2 effHalf = targetHalf * (1.0f - pad); // leave 'pad' of half-extent as margin on each side

	// --- uniform scale so that (size/2) -> effHalf, preserving aspect ---
	float sx = effHalf.x / (size.x * 0.5f);
	float sy = effHalf.y / (size.y * 0.5f);
	float s = glm::min(sx, sy);

	// We want: v' = S * (T * v)  where T moves center to origin, then S scales.
	glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(-center, 0.0f));
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(s, s, 1.0f));

	// Final transform maps bounds into [-effHalf, +effHalf] ⊆ [-targetHalf, +targetHalf]
	return S * T;
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

		if (DebugMode)
		{
			ImGui::ShowDemoWindow();
			ImPlot::ShowDemoWindow();
			ImPlot3D::ShowDemoWindow();
		}

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

const std::string Cartograph::InitialLayout = R"(
[Window][MainDockspace]
Pos=0,0
Size=1920,1080
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][ConnectTo]
Pos=8,8
Size=1590,67
Collapsed=0
DockId=0x00000005,0

[Window][Map]
Pos=8,77
Size=1590,995
Collapsed=0
DockId=0x00000006,0

[Window][Partitions]
Pos=1600,77
Size=312,622
Collapsed=0
DockId=0x00000008,0

[Window][Log]
Pos=1600,701
Size=312,371
Collapsed=0
DockId=0x00000004,0

[Window][Options]
Pos=1600,8
Size=312,67
Collapsed=0
DockId=0x00000007,0

[Docking][Data]
DockSpace       ID=0x8D32E0A4 Window=0x1C358F53 Pos=488,389 Size=1904,1064 Split=X Selected=0x55B716CB
  DockNode      ID=0x00000001 Parent=0x8D32E0A4 SizeRef=1590,1064 Split=Y Selected=0x55B716CB
    DockNode    ID=0x00000005 Parent=0x00000001 SizeRef=1590,67 Selected=0x2DA51D0C
    DockNode    ID=0x00000006 Parent=0x00000001 SizeRef=1590,995 CentralNode=1 Selected=0x55B716CB
  DockNode      ID=0x00000002 Parent=0x8D32E0A4 SizeRef=312,1064 Split=Y Selected=0x3228694B
    DockNode    ID=0x00000003 Parent=0x00000002 SizeRef=312,691 Split=Y Selected=0x3228694B
      DockNode  ID=0x00000007 Parent=0x00000003 SizeRef=312,67 Selected=0x10A3DF36
      DockNode  ID=0x00000008 Parent=0x00000003 SizeRef=312,622 Selected=0x3228694B
    DockNode    ID=0x00000004 Parent=0x00000002 SizeRef=312,371 Selected=0x139FDA3F

)";