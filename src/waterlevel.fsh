// Values that stay constant for the whole mesh.
uniform samplerCube elevationMap;
uniform sampler2D heightsColorMap;
uniform sampler2D depthsColorMap;
uniform int level;
uniform int zmin;
uniform int zmax;

// Input vertex data, different for all executions of this shader.
varying vec3 fragCubeTexCoord;

void main() {
	vec4 texel = textureCube(elevationMap, fragCubeTexCoord);
	int elevation = 256*int(256.0*texel.r) + int(256.0*texel.g) + int(255.0*texel.b) - 32768;
//	int elevation = 4096*int(64.0*texel.r) + 64*int(64.0*texel.g) + int(64.0*texel.b) - 32768;
	
	if (elevation>=level) {
		float h = float(elevation-level)/float(zmax-level);
		gl_FragColor = texture2D(heightsColorMap, vec2(pow(h, 1.2), 0.0));
	}
	else {
		float h = float(level-elevation)/float(level-zmin);
		gl_FragColor = texture2D(depthsColorMap, vec2(pow(h, 1.2), 0.0));
	}
}
