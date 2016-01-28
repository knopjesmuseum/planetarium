// Values that stay constant for the whole mesh.
uniform mat4 MVP;

// Input vertex data, different for all executions of this shader.
attribute highp vec3 vertexPosition;
attribute highp vec3 cubeTexCoord;

// Output data ; will be interpolated for each fragment.
varying highp vec3 fragCubeTexCoord;

void main() {
	gl_Position = MVP * vec4(vertexPosition,1);
	fragCubeTexCoord = cubeTexCoord;
}
