$input a_position, a_texcoord0
$output v_pos, v_uv

#include "../../examples/common/common.sh"

void main() {
	v_pos = a_position.xyz;
	v_uv = a_texcoord0;
	gl_Position = mul(u_modelViewProj, vec4(a_position.xyz, 1.0f));
}