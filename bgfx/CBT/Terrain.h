#pragma once

#include <entry\entry.h>
#include "BaseDataStructure.hpp"
#include <memory>

static constexpr Data::Vertex cubeVertices[] = {
	{-1.0f,  1.0f,  1.0f, 0xff000000 },
	{ 1.0f,  1.0f,  1.0f, 0xff0000ff },
	{-1.0f, -1.0f,  1.0f, 0xff00ff00 },
	{ 1.0f, -1.0f,  1.0f, 0xff00ffff },
	{-1.0f,  1.0f, -1.0f, 0xffff0000 },
	{ 1.0f,  1.0f, -1.0f, 0xffff00ff },
	{-1.0f, -1.0f, -1.0f, 0xffffff00 },
	{ 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static constexpr uint16_t cubeEBOs[] = {
	0, 1, 2,
	1, 3, 2,
	4, 6, 5,
	5, 6, 7,
	0, 2, 4,
	4, 2, 6,
	1, 5, 3,
	5, 7, 3,
	0, 4, 1,
	4, 5, 1,
	2, 3, 6,
	6, 3, 7,
};

class Terrain :public entry::AppI
{
public:
	Terrain(const char* _name, const char* _description, const char* _url);
	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override;
	int shutdown() override;
	bool update() override;
private:
	void UpdateTerrain(bool sizeDirty);
	void UpdateTerrainMesh();
	void MonitorMouse(float deltaTime);
	void PaintTerrainHeight(uint32_t _x, uint32_t _y, float deltaTime);
public:
	entry::MouseState m_mouseState;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;

	bgfx::VertexBufferHandle			m_vbo;
	bgfx::IndexBufferHandle				m_ebo;
	bgfx::ProgramHandle					m_program;
	bgfx::ProgramHandle					m_heightProgram;
	bgfx::DynamicVertexBufferHandle		m_dynamicVBO;
	bgfx::DynamicIndexBufferHandle		m_dynamicEBO;
	bgfx::UniformHandle					m_heightMapSampler;
	bgfx::TextureHandle					m_heightTexture;
	Data::TerrainData					m_terrain;
	Data::BrushData						m_brush;
	/*
	 * Height Texture Generation
	 */
	int32_t					m_terrainMagn;
	uint64_t				m_terrainSize;

	float					m_view[16];
	float					m_proj[16];
};
