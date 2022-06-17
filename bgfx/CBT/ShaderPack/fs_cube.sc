$input v_uv, v_pos

#include "../../examples/common/common.sh"

void main() {
	gl_FragColor = vec4(v_uv, v_pos.y / 255.0f, 1.0f);
}