#pragma once

#include <bgfx/bgfx.h>

namespace Data
{
struct Vertex
{
	float pos_x;
	float pos_y;
	float pos_z;
	float tex_u;
	float tex_v;

	static void Init()
	{
		m_layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
	}
	inline static bgfx::VertexLayout m_layout;
};


struct TerrainData {
	int32_t		m_mode;
	bool		m_dirty;
	float		m_transform[16];
	uint8_t*	m_heightMap;
	float*		m_height;

	Vertex*		m_vbo;
	uint32_t	m_vboCount;
	uint16_t*	m_ebo;
	uint32_t	m_eboCount;
};

struct BrushData {
	bool	m_raise;
	int32_t m_size;
	float	m_power;
};
}
