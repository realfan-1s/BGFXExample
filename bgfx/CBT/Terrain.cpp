#include "Terrain.h"
#include <imgui/imgui.h>
#include <bgfx_utils.h>
#include <string>
#include <cmath>
#include <camera.h>
#include "Timer.h"

using namespace bgfx;
using namespace bx;
using namespace ImGui;
using namespace Data;

Terrain::Terrain(const char* _name, const char* _description, const char* _url)
: entry::AppI(_name, _description, _url)
{
}

void Terrain::init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height)
{
	Args args(_argc, _argv);

	m_width = _width;
	m_height = _height;
	m_debug = BGFX_DEBUG_TEXT;
	m_reset = BGFX_RESET_NONE;

	bgfx::Init init;
	init.type = args.m_type;
	init.vendorId = args.m_pciId;
	init.resolution.width = m_width;
	init.resolution.height = m_height;
	init.resolution.reset = m_reset;
	bgfx::init(init);

	// Enable debug text.
	bgfx::setDebug(m_debug);

	// Set view 0 clear state.
	bgfx::setViewClear(0
		, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
		, 0x303030ff
		, 1.0f
		, 0
	);
	Vertex::Init();

	// 从shader中创建程序
	m_program = loadProgram("vs_cube", "fs_cube");
	m_heightProgram = loadProgram("vs_height_map", "fs_cube");

	imguiCreate();

	// SetHeightMap Or dynamic/static EBO/VBO
	m_ebo.idx = bgfx::kInvalidHandle;
	m_vbo.idx = bgfx::kInvalidHandle;
	m_dynamicVBO.idx = bgfx::kInvalidHandle;
	m_dynamicEBO.idx = bgfx::kInvalidHandle;
	m_heightTexture.idx = bgfx::kInvalidHandle;
	m_heightMapSampler = bgfx::createUniform("heightTextureSampler", bgfx::UniformType::Sampler);

	// Init BrushData and TerrainData
	m_brush.m_power = 0.5f;
	m_brush.m_size = 10;
	m_brush.m_raise = true;
	m_terrainMagn = 8;
	uint64_t texWidth = static_cast<uint64_t>(std::pow(2, m_terrainMagn));
	uint64_t square = texWidth * texWidth;
	m_terrainSize = texWidth;
	m_terrain.m_mode = 0;
	m_terrain.m_dirty = true;
	m_terrain.m_vbo = (Vertex*)BX_ALLOC(entry::getAllocator(), square * sizeof(Vertex));
	m_terrain.m_ebo = (uint16_t*)BX_ALLOC(entry::getAllocator(), square * sizeof(uint16_t) * 6);
	m_terrain.m_heightMap = (uint8_t*)BX_ALLOC(entry::getAllocator(), sizeof(uint8_t) * square);
	bx::memSet(m_terrain.m_heightMap, 0, sizeof(uint8_t) * square);
	m_terrain.m_height = (float*)BX_ALLOC(entry::getAllocator(), sizeof(float) * square);
	bx::memSet(m_terrain.m_height, 0.0f, sizeof(float) * square);

	// Set Model Matrix
	bx::mtxSRT(m_terrain.m_transform, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

	// Set Camera Parameters
	cameraCreate();
	cameraSetPosition({ m_terrainSize / 2.0f, 100.0f, 0.0f });
	cameraSetVerticalAngle(-bx::kPiQuarter);
}

int Terrain::shutdown()
{
	cameraDestroy();
	imguiDestroy();

	/*
	 * 销毁清除VBO、EBO、TexSampler、shader program
	 */
	if (isValid(m_vbo))
		destroy(m_vbo);
	if (isValid(m_ebo))
		destroy(m_ebo);
	if (isValid(m_dynamicVBO))
		destroy(m_dynamicVBO);
	if (isValid(m_dynamicEBO))
		destroy(m_dynamicEBO);
	if (isValid(m_heightMapSampler))
		destroy(m_heightMapSampler);
	if (isValid(m_heightTexture))
		destroy(m_heightTexture);
	destroy(m_program);
	destroy(m_heightProgram);

	/*
	 * 通过makeRef传递给bgfx的内存需要进行手动释放
	 */
	bgfx::frame();
	bx::AllocatorI* allocator = entry::getAllocator();
	BX_FREE(allocator, m_terrain.m_height);
	BX_FREE(allocator, m_terrain.m_vbo);
	BX_FREE(allocator, m_terrain.m_ebo);
	BX_FREE(allocator, m_terrain.m_heightMap);

	// Shutdown bgfx.
	bgfx::shutdown();

	return 0;
}

bool Terrain::update()
{
	if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
	{
		const float deltaTime = Timer::DeltaTime();

		imguiBeginFrame(m_mouseState.m_mx
			, m_mouseState.m_my
			, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
			| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
			| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
			, m_mouseState.m_mz
			, uint16_t(m_width)
			, uint16_t(m_height)
		);

		showExampleDialog(this);

		SetNextWindowPos(ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f), ImGuiCond_FirstUseEver);
		SetNextWindowSize(ImVec2(m_width / 5.0f, m_height / 3.5f), ImGuiCond_FirstUseEver);

		ImGui::Begin("Settings", nullptr, 0);
		Separator();
		// GUI Setting
		m_terrain.m_dirty |= RadioButton("Vertex Buffer", &m_terrain.m_mode, 0);
		m_terrain.m_dirty |= RadioButton("Dynamic Vertex Buffer", &m_terrain.m_mode, 1);
		m_terrain.m_dirty |= RadioButton("Height Texture", &m_terrain.m_mode, 2);
		ImGui::Separator();
		Checkbox("Raise Terrain", &m_brush.m_raise);
		SliderInt("Brush Size", &m_brush.m_size, 1, 50);
		SliderFloat("Brush Power", &m_brush.m_power, 0.0f, 1.0f);
		bool sizeDirty = false;
		sizeDirty |= SliderInt("Terrain Size Magnification", &m_terrainMagn, 7, 12);
		m_terrain.m_dirty |= sizeDirty;
		ImGui::End();
		imguiEndFrame();

		if (sizeDirty)
		{
			m_terrainSize = static_cast<uint64_t>(std::pow(2, m_terrainMagn));
			uint64_t square = m_terrainSize * m_terrainSize;
			bx::AllocatorI* allocator = entry::getAllocator();
			BX_FREE(allocator, m_terrain.m_vbo);
			BX_FREE(allocator, m_terrain.m_ebo);
			BX_FREE(allocator, m_terrain.m_heightMap);
			BX_FREE(allocator, m_terrain.m_height);
			m_terrain.m_vbo = (Vertex*)BX_ALLOC(entry::getAllocator(), square * sizeof(Vertex));
			m_terrain.m_ebo = (uint16_t*)BX_ALLOC(entry::getAllocator(), square * sizeof(uint16_t) * 6);
			m_terrain.m_heightMap = (uint8_t*)BX_ALLOC(entry::getAllocator(), square * sizeof(uint8_t));
			bx::memSet(m_terrain.m_heightMap, 0, sizeof(uint8_t) * square);
			m_terrain.m_height = (float*)BX_ALLOC(entry::getAllocator(), square * sizeof(float));
			bx::memSet(m_terrain.m_height, 0.0f, sizeof(float) * square);
		}

		// Update Camera
		cameraUpdate(deltaTime, m_mouseState, MouseOverArea());
		if (!MouseOverArea())
		{
			if (m_mouseState.m_buttons[entry::MouseButton::Left])
			{
				MonitorMouse(deltaTime);
			}
		}

		// Update Terrain
		if(m_terrain.m_dirty)
		{
			UpdateTerrain(sizeDirty);
			m_terrain.m_dirty = false;
		}

		// Set default view and projection matrix and viewport
		bgfx::setViewRect(0, 0, 0, m_width, m_height);
		cameraGetViewMtx(m_view);
		bx::mtxProj(m_proj, 60.0f, (float)m_width / (float)m_height, 0.1f, 2000.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(0, m_view, m_proj);
		bgfx::setTransform(m_terrain.m_transform);

		// Upload To GPU
		switch (m_terrain.m_mode)
		{
			case 0: // set vbo
				setVertexBuffer(0, m_vbo);
				setIndexBuffer(m_ebo);
				submit(0, m_program);
				break;
			case 1: // set dynamic vbo
				setVertexBuffer(0, m_dynamicVBO);
				setIndexBuffer(m_dynamicEBO);
				submit(0, m_program);
				break;
			case 2: // set height texture
				setVertexBuffer(0, m_vbo);
				setIndexBuffer(m_ebo);
				setTexture(0, m_heightMapSampler, m_heightTexture);
				submit(0, m_heightProgram);
				break;
			default:
				break;
		}

		// process submitted rendering primitives.
		bgfx::frame();
		return true;
	}

	return false;
}

void Terrain::UpdateTerrain(bool sizeDirty)
{
	const Memory* mem;
	switch (m_terrain.m_mode)
	{
	case 0:// recreate Vertex Buffer Objects
		UpdateTerrainMesh();
		if (isValid(m_vbo))
			destroy(m_vbo);
		mem = bgfx::makeRef(&m_terrain.m_vbo[0], sizeof(Vertex) * m_terrain.m_vboCount);
		m_vbo = createVertexBuffer(mem, Vertex::m_layout);

		if (isValid(m_ebo))
			destroy(m_ebo);
		mem = bgfx::makeRef(&m_terrain.m_ebo[0], sizeof(uint16_t) * m_terrain.m_eboCount);
		m_ebo = createIndexBuffer(mem);
		break;

	case 1: // Dynamic Vertex Buffer
		UpdateTerrainMesh();
		if (!isValid(m_dynamicVBO) || sizeDirty)
			m_dynamicVBO = createDynamicVertexBuffer(m_terrain.m_vboCount, Vertex::m_layout);
		mem = bgfx::makeRef(&m_terrain.m_vbo[0], sizeof(Vertex) * m_terrain.m_vboCount);
		bgfx::update(m_dynamicVBO, 0, mem);

		if (!isValid(m_dynamicEBO) || sizeDirty)
			m_dynamicEBO = createDynamicIndexBuffer(m_terrain.m_eboCount);
		mem = bgfx::makeRef(&m_terrain.m_ebo[0], sizeof(uint16_t) * m_terrain.m_eboCount);
		bgfx::update(m_dynamicEBO, 0, mem);
		break;

	case 2: // sample From Height Texture
		if (!isValid(m_vbo) || !isValid(m_ebo) || sizeDirty)
		{
			UpdateTerrainMesh();
			mem = bgfx::makeRef(&m_terrain.m_vbo[0], sizeof(Vertex) * m_terrain.m_vboCount);
			m_vbo = bgfx::createVertexBuffer(mem, Vertex::m_layout);

			mem = bgfx::makeRef(&m_terrain.m_ebo[0], sizeof(Vertex) * m_terrain.m_eboCount);
			m_ebo = bgfx::createIndexBuffer(mem);
		}

		if (!isValid(m_heightTexture) || sizeDirty)
		{
			m_heightTexture = createTexture2D(m_terrainSize, m_terrainSize, false, 1, TextureFormat::R8);
			mem = bgfx::makeRef(&m_terrain.m_heightMap[0], sizeof(uint8_t) * m_terrainSize * m_terrainSize);
			updateTexture2D(m_heightTexture, 0, 0, 0, 0, m_terrainSize, m_terrainSize, mem);
		}
		break;
	}
}

//TODO: Reset Terrain Mesh
void Terrain::UpdateTerrainMesh()
{
	m_terrain.m_vboCount = 0;
	for (uint64_t x = 0; x < m_terrainSize; ++x)
		for (uint64_t y = 0; y < m_terrainSize; ++y)
		{
			Vertex* vert = &m_terrain.m_vbo[m_terrain.m_vboCount];
			vert->pos_x = (float)x;
			vert->pos_y = m_terrain.m_heightMap[(x * m_terrainSize) + y];
			vert->pos_z = (float)y;
			vert->tex_u = (x + 0.5f) / m_terrainSize;
			vert->tex_v = (y + 0.5f) / m_terrainSize;
			m_terrain.m_vboCount++;
		}

	m_terrain.m_eboCount = 0;
	for (uint64_t x = 0; x < m_terrainSize; ++x)
	{
		uint64_t offsetX = x * m_terrainSize;
		for (uint64_t y = 0; y < m_terrainSize; ++y)
		{
			m_terrain.m_ebo[m_terrain.m_eboCount + 0] = offsetX + y + 1;
			m_terrain.m_ebo[m_terrain.m_eboCount + 1] = offsetX + y + m_terrainSize;
			m_terrain.m_ebo[m_terrain.m_eboCount + 2] = offsetX + y;
			m_terrain.m_ebo[m_terrain.m_eboCount + 3] = offsetX + y + m_terrainSize + 1;
			m_terrain.m_ebo[m_terrain.m_eboCount + 4] = offsetX + y + m_terrainSize;
			m_terrain.m_ebo[m_terrain.m_eboCount + 5] = offsetX + y + 1;

			m_terrain.m_eboCount += 6;
		}
	}
}

void Terrain::MonitorMouse(float deltaTime)
{
	// mouse Click
	float rayClip[4];
	rayClip[0] = ((2.0f * m_mouseState.m_mx) / m_width - 1.0f) * -1.0f;
	rayClip[1] = ((1.0f - (2.0f * m_mouseState.m_my) / m_height)) * -1.0f;
	rayClip[2] = -1.0f;
	rayClip[3] = 1.0f;

	float invProj[16];
	bx::mtxInverse(invProj, m_proj);

	float viewRay[4];
	bx::vec4MulMtx(viewRay, rayClip, invProj);
	viewRay[2] = -1.0f;
	viewRay[3] = 0.0f;

	float invView[16];
	mtxInverse(invView, m_view);

	float modelRay[4];
	bx::vec4MulMtx(modelRay, viewRay, invView);
	Vec3 rayDir = mul(normalize(load<Vec3>(modelRay)), -1.0f);
	Vec3 pos = cameraGetPosition();

	for (uint32_t i = 0; i < 1000; ++i)
	{
		pos = add(pos, rayDir);
		if (pos.x < 0 || pos.x >= static_cast<float>(m_terrainSize)
		|| pos.z < 0 || pos.z >= static_cast<float>(m_terrainSize))
		{
			continue;
		}

		uint32_t heightMapPos = ((uint32_t)pos.z * m_terrainSize) + (uint32_t)pos.x;
		if (pos.y < static_cast<float>(m_terrain.m_heightMap[heightMapPos]))
		{
			PaintTerrainHeight((uint32_t)pos.x, (uint32_t)pos.z, deltaTime);
			return;
		}
	}
}

void Terrain::PaintTerrainHeight(uint32_t _x, uint32_t _y, float deltaTime)
{
	for (int32_t x = -m_brush.m_size; x < m_brush.m_size; ++x)
		for (int32_t y = -m_brush.m_size; y < m_brush.m_size; ++y)
		{
			int32_t brushX = _x + x;
			if (brushX < 0 || brushX >= (int32_t)m_terrainSize)
				continue;
			int32_t brushY = _y + y;
			if (brushY < 0 || brushY >= (int32_t)m_terrainSize)
				continue;

			uint32_t heightTexcoord = (brushX * m_terrainSize) + brushY;
			float height = m_terrain.m_height[heightTexcoord];
			// Brush attenuation
				// Brush attenuation
			float a2 = static_cast<float>(x * x);
			float b2 = static_cast<float>(y * y);
			float brushAttn = static_cast<float>(m_brush.m_size) - bx::sqrt(a2 + b2);

			// Raise/Lower and scale by brush power.
			float perHeight = 150.0f * deltaTime;
			height += bx::clamp(brushAttn * m_brush.m_power, 0.0f, m_brush.m_power) > 0.0f && m_brush.m_raise
				? perHeight : -perHeight;
			m_terrain.m_height[heightTexcoord] = bx::clamp(height, 0.0f, 255.0f);
			m_terrain.m_heightMap[heightTexcoord] = (uint8_t)bx::clamp(height, 0.0f, 255.0f);
			m_terrain.m_dirty = true;
		}
}
