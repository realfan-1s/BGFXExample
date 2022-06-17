$input a_position, a_texcoord0
$output v_pos, v_uv

#include "../../examples/common/common.sh"

SAMPLER2D(s_heightTexture, 0);

void main() {
	v_uv = a_texcoord0;
	v_pos = a_position.xyz;
	v_pos.y = texture2DLod(s_heightTexture, a_texcoord0, 0.0f).x * 255.0f;
	gl_Position = mul(u_modelViewProj, vec4(v_pos.xyz, 1.0f));
}