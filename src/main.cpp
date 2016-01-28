// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <termio.h>
#include <unistd.h>
#include <vector>

// Include graphics headers
#include "GLES2/gl2.h"

#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "startScreen.h"
#include "LoadShaders.h"
#include <FreeImage.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

// Include GPIO library
#include <wiringPi.h>

// Include config file
#include <libconfig.h>

using namespace glm;

#include "icosphere.h"

glm::mat4 projectionMatrix;
glm::mat4 viewMatrix;
glm::mat4 modelMatrix;
glm::mat4 mvpMatrix;

config_t cfg;
config_setting_t *cfgRoot, *cfgSetting, *cfgValue;
glm::quat accumRot;
float zoom;
float xoff;
float yoff;
float roff;

// Shader variables
GLuint programID;
GLuint mvpLoc;
GLuint positionLoc;
GLuint texcoordLoc;
GLuint elevationMapLoc;
GLuint heightsColorMapLoc;
GLuint depthsColorMapLoc;

GLint  levelLoc;
GLint  zminLoc;
GLint  zmaxLoc;

// Texture variables
GLuint elevationMapID;
GLuint heightsColorMapID;
GLuint depthsColorMapID;
unsigned short* raw;
unsigned char* cubeMapData[6];
unsigned char* heightsColorData;
unsigned char* depthsColorData;

IcoSphere sphere;
GLuint vertexbuffer;
GLuint uvbuffer;
GLuint normalbuffer;

std::vector<glm::vec3> vertices;
std::vector<glm::vec2> uvs;
std::vector<glm::vec3> normals;

unsigned int xres, yres;
int zmin, zmax;
int level;

bool quit;

//int heights[16]   = {-8000,-5000,-3000,-2000,-1000,-150 ,-20  ,-5   ,0   ,0   ,10  ,500 ,2000,3000,5000,8000};
//float heights[16] = {0,0.1875,0.3125,0.375,0.4375,0.490625,0.49875,0.4996875,0.4999,0.5001,0.500625,0.53125,0.625,0.6875,0.8125,1};
int numHeights = 7;
float heights[7] = {0,    0.00125, 0.0625, 0.25, 0.375, 0.625, 1   };
float heightR[7] = {0.00, 0.05,    0.76,   0.64, 0.52,  0.44,  0.84};
float heightG[7] = {0.36, 0.47,    0.72,   0.28, 0.11,  0.41,  0.84};
float heightB[7] = {0.25, 0.19,    0.41,   0.22, 0.30,  0.41,  0.84};

int numDepths = 9;
float depths[9] = {0,    0.000625, 0.0025, 0.01875, 0.125, 0.25, 0.375, 0.625, 1   };
float depthR[9] = {1.00, 0.96,     0.30,   0.20,    0.00,  0.00, 0.00,  0.03,  0.06};
float depthG[9] = {0.66, 0.98,     0.76,   0.59,    0.38,  0.26, 0.00,  0.00,  0.00};
float depthB[9] = {0.49, 0.60,     0.79,   0.82,    0.67,  0.73, 0.68,  0.59,  0.53};

static int stdin_fd = -1;
static struct termios original;

int irpin = 1;
int irfreq = 38000;
int pwmrange;
#define HDR_MARK	342
#define HDR_SPACE	171
#define ZERO_MARK	24
#define ZERO_SPACE	19
#define ONE_MARK	24
#define ONE_SPACE	62
#define END_MARK	24
#define END_SPACE	1427

class Encoder {
public:
	int pinA;
	int pinB;
	int pos;
	int pinALast;
	Encoder(int pinA, int pinB, int type) {
		this->pinA = pinA;
		this->pinB = pinB;
		pinMode(pinA, INPUT);
		pinMode(pinB, INPUT);
		pullUpDnControl (pinA, type);
		pullUpDnControl (pinB, type);
		pos = 0;
		pinALast = LOW;
	}
	void update() {
		int val = digitalRead(pinA);
		if ((pinALast == LOW) && (val == HIGH)) {
			if (digitalRead(pinB) == LOW) pos--;
			else pos++;
		} 
		pinALast = val;
	}
	int value() {
		int val = pos;
		pos = 0;
		return val;
	}
};

void pwmIR(bool bit, int cycles) {
	pwmWrite(irpin, bit ? pwmrange/2 : 0);
	delayMicroseconds(cycles);
}

void sendIR(unsigned long data, int nbits) {
	pwmrange = 19200000 / (irfreq * 5);		// pwm resolution			100
	int pwmpulse = 1000000 / irfreq;		// carrier cycle length		26
	pinMode(irpin, PWM_OUTPUT);
	pullUpDnControl (irpin, PUD_OFF);
	pwmSetClock(5);
	pwmSetRange(pwmrange);
	
	pwmIR(1, HDR_MARK*pwmpulse);
	pwmIR(0, HDR_SPACE*pwmpulse);
	for (int i = 0; i < nbits; i++) {
		if(data & 0x80000000) {
			pwmIR(1, ONE_MARK*pwmpulse);
			pwmIR(0, ONE_SPACE*pwmpulse);
		}
		else {
			pwmIR(1, ZERO_MARK*pwmpulse);
			pwmIR(0, ZERO_SPACE*pwmpulse);
		}
		data <<= 1;
	}
	pwmIR(1, END_MARK*pwmpulse);
	pwmIR(0, END_SPACE*pwmpulse);
}

bool keyPressed(int *character) {
	if (stdin_fd == -1) {
		stdin_fd = fileno(stdin);
		tcgetattr(stdin_fd, &original);
		struct termios term;
		memcpy(&term, &original, sizeof(term));
		term.c_lflag &= ~(ICANON|ECHO);
		tcsetattr(stdin_fd, TCSANOW, &term);
		setbuf(stdin, NULL);
	}
	int characters_buffered = 0;
	ioctl(stdin_fd, FIONREAD, &characters_buffered);
	bool pressed = (characters_buffered != 0);
	if (characters_buffered == 1) {
		int c = fgetc(stdin);
		if (character != NULL) *character = c;
	}
	else if (characters_buffered > 1) {
		while (characters_buffered) {
			int c = fgetc(stdin);
			--characters_buffered;
		}
	}
	return pressed;
}
void keyboardReset(void) {
	if (stdin_fd != -1) tcsetattr(stdin_fd, TCSANOW, &original);
}

void encodeElevation(unsigned char *data, glm::vec3 pos) {
	int u;
	pos = glm::normalize(pos);
	if (pos.x!=0.0f || pos.z!=0.0f) u = floor(xres*(0.5 + atan2(pos.x, pos.z)/TWO_PI));
	else u = 0;
	int v = floor(yres*(0.5 - asin(pos.y)/PI));
	
	unsigned short elevation = raw[v*xres + u];
	if (elevation!=0) zmin = min(zmin, elevation - 32768);
	else elevation = 32768;
	zmax = max(zmax, elevation - 32768);
	
	data[0] = elevation/256;
	data[1] = elevation%256;
	data[2] = 0;
/*	
	data[0] = 4*(elevation/4096);
	data[1] = 4*((elevation%4096)/64);
	data[2] = 4*((elevation%4096)%64);
*/}

void loadData(const char* filename) {
	// load elevation data from PNG file in (compressed) unsigned short format
	printf("loading %s\n", filename);
	
	FREE_IMAGE_FORMAT format = FreeImage_GetFileType(filename, 0);
	FIBITMAP* image = FreeImage_Load(format, filename);
	xres = FreeImage_GetWidth(image);
	yres = FreeImage_GetHeight(image);
	raw = (unsigned short*)FreeImage_GetBits(image);
	unsigned int cuberes = xres/4;
	
	printf("image size %i x %i\n", xres, yres);
	printf("cubemap size %i\n", cuberes);
	
	zmin = 0;
	zmax = 0;
	// create elevation cubemap
	printf("creating GL elevation cube map texture from data...\n");
	for (int i=0; i<6; i++) cubeMapData[i] = (unsigned char*)malloc(3*cuberes*cuberes*sizeof(unsigned char));
	for (unsigned int y=0; y<cuberes; y++) for (unsigned int x=0; x<cuberes; x++) for (unsigned int c=0; c<3; c++) {
		encodeElevation(&cubeMapData[0][3*(y*cuberes+x)], glm::vec3(1.0, 2.0*y/cuberes - 1.0, 2.0*x/cuberes - 1.0));
		encodeElevation(&cubeMapData[1][3*(y*cuberes+x)], glm::vec3(-1.0, 2.0*y/cuberes - 1.0, 1.0 - 2.0*x/cuberes));
		encodeElevation(&cubeMapData[2][3*(y*cuberes+x)], glm::vec3(2.0*x/cuberes - 1.0, -1.0, 1.0 - 2.0*y/cuberes));
		encodeElevation(&cubeMapData[3][3*(y*cuberes+x)], glm::vec3(2.0*x/cuberes - 1.0, 1.0, 2.0*y/cuberes - 1.0));
		encodeElevation(&cubeMapData[4][3*(y*cuberes+x)], glm::vec3(2.0*x/cuberes - 1.0, 2.0*y/cuberes - 1.0, -1.0));
		encodeElevation(&cubeMapData[5][3*(y*cuberes+x)], glm::vec3(1.0 - 2.0*x/cuberes, 2.0*y/cuberes - 1.0, 1.0));
	}
//	free(raw);
	printf("data between %i and %i\n", zmin, zmax);
	
	// Create one OpenGL texture
	glGenTextures(1, &elevationMapID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, elevationMapID);
	
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[0]);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[1]);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[2]);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[3]);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[4]);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, cuberes, cuberes, 0, GL_RGB, GL_UNSIGNED_BYTE, cubeMapData[5]);
//	for (int i=0; i<6; i++) free(cubeMapData[i]);
	
	// These params let the data through (seemingly) unmolested
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	// create color map
	printf("creating GL color map texture...\n");
	unsigned int colorres = 1024;
	heightsColorData = (unsigned char*)malloc(3*colorres*sizeof(unsigned char));
	depthsColorData = (unsigned char*)malloc(3*colorres*sizeof(unsigned char));
	for (unsigned int c=0; c<colorres; c++) {
		double h = 1.0*c/colorres;
		int i;
		double dh;
		
		for (i=0; i<numHeights-1; i++) if ((h>=heights[i]) && (h<heights[i+1])) break;
		dh = 1.0*(h-heights[i])/(heights[i+1]-heights[i]);
		heightsColorData[3*c+0] = 255.0*(heightR[i] + dh*(heightR[i+1] - heightR[i]));
		heightsColorData[3*c+1] = 255.0*(heightG[i] + dh*(heightG[i+1] - heightG[i]));
		heightsColorData[3*c+2] = 255.0*(heightB[i] + dh*(heightB[i+1] - heightB[i]));
		
		for (i=0; i<numDepths-1; i++) if ((h>=depths[i]) && (h<depths[i+1])) break;
		dh = 1.0*(h-depths[i])/(depths[i+1]-depths[i]);
		depthsColorData[3*c+0] = 255.0*(depthR[i] + dh*(depthR[i+1] - depthR[i]));
		depthsColorData[3*c+1] = 255.0*(depthG[i] + dh*(depthG[i+1] - depthG[i]));
		depthsColorData[3*c+2] = 255.0*(depthB[i] + dh*(depthB[i+1] - depthB[i]));
	}
	
	// Create one OpenGL texture
	glGenTextures(1, &heightsColorMapID);
	glBindTexture(GL_TEXTURE_2D, heightsColorMapID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, colorres, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, heightsColorData);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	// Create one OpenGL texture
	glGenTextures(1, &depthsColorMapID);
	glBindTexture(GL_TEXTURE_2D, depthsColorMapID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, colorres, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, depthsColorData);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
//	free(heightsColorData);
//	free(colorData);
	printf("done...\n");
}

void init() {
	config_init(&cfg);
	if (!config_read_file(&cfg, "planetarium.cfg")) {
		// create
	};
	cfgRoot = config_root_setting(&cfg);
	
	cfgSetting = config_setting_get_member(cfgRoot, "zoom");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "zoom", CONFIG_TYPE_FLOAT);
		config_setting_set_float(cfgSetting, 1.0);
	}
	zoom = config_setting_get_float(cfgSetting);
	cfgSetting = config_setting_get_member(cfgRoot, "roff");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "roff", CONFIG_TYPE_FLOAT);
		config_setting_set_float(cfgSetting, 0.0);
	}
	roff = config_setting_get_float(cfgSetting);
	
	cfgSetting = config_setting_get_member(cfgRoot, "xoff");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "xoff", CONFIG_TYPE_FLOAT);
		config_setting_set_float(cfgSetting, 0.0);
	}
	xoff = config_setting_get_float(cfgSetting);
	
	cfgSetting = config_setting_get_member(cfgRoot, "yoff");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "yoff", CONFIG_TYPE_FLOAT);
		config_setting_set_float(cfgSetting, 0.0);
	}
	yoff = config_setting_get_float(cfgSetting);
	
	cfgSetting = config_setting_get_member(cfgRoot, "startlevel");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "startlevel", CONFIG_TYPE_INT);
		config_setting_set_float(cfgSetting, 0);
	}
	level = config_setting_get_float(cfgSetting);
	
	cfgSetting = config_setting_get_member(cfgRoot, "startpos");
	if(!cfgSetting) {
		cfgSetting = config_setting_add(cfgRoot, "startpos", CONFIG_TYPE_ARRAY);
		config_setting_set_float_elem(cfgSetting, -1, 0.846148);
		config_setting_set_float_elem(cfgSetting, -1, 0.046318);
		config_setting_set_float_elem(cfgSetting, -1, 0.530542);
		config_setting_set_float_elem(cfgSetting, -1, 0.022153);
	}
	accumRot.w = config_setting_get_float_elem(cfgSetting, 0);
	accumRot.x = config_setting_get_float_elem(cfgSetting, 1);
	accumRot.y = config_setting_get_float_elem(cfgSetting, 2);
	accumRot.z = config_setting_get_float_elem(cfgSetting, 3);
	
	quit = false;
	
	// Initialize OpenGL
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); // Accept fragment if it closer to the camera than the former one
	glEnable(GL_CULL_FACE); // Cull triangles which normal is not towards the camera
	
	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "src/waterlevel.vsh", "src/waterlevel.fsh" );
	
	// Get a handle for our "MVP" uniform
	mvpLoc = glGetUniformLocation(programID, "MVP");
	
	// Get a handle for our buffers
	positionLoc = glGetAttribLocation(programID, "vertexPosition");
	texcoordLoc = glGetAttribLocation(programID, "cubeTexCoord");
	
	// Get a handle for other shader variables
	levelLoc = glGetUniformLocation(programID, "level" );
	zminLoc = glGetUniformLocation(programID, "zmin" );
	zmaxLoc = glGetUniformLocation(programID, "zmax" );

	// Set projection matrix
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	projectionMatrix = glm::ortho(-1.0f*viewport[2]/viewport[3], 1.0f*viewport[2]/viewport[3],-1.0f,1.0f,0.0f,100.0f);
	
	// View matrix: camera settings
	glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 3.0f);	// Positive Z axis
	glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);	// Look at origin
	glm::vec3 cameraUpVector = glm::vec3(0.0f, 1.0f, 0.0f);	// Head is up
	
	viewMatrix = glm::lookAt(
		cameraPosition,
		cameraTarget,
		cameraUpVector
	);
	
	// Get a handle for our "myTextureSampler" uniform
	elevationMapLoc = glGetUniformLocation(programID, "elevationMap");
	heightsColorMapLoc = glGetUniformLocation(programID, "heightsColorMap");
	depthsColorMapLoc = glGetUniformLocation(programID, "depthsColorMap");
	
	sphere.Create(3);
	for (unsigned int i = 0; i<sphere.indices.size(); i++) {
		vertices.push_back(sphere.vertices[sphere.indices[i]]);
		uvs.push_back(sphere.uvs[sphere.indices[i]]);
		normals.push_back(sphere.normals[sphere.indices[i]]);
	}
	
	// Load it into a VBO
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
	
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
	
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
	
	loadData("data/terra8M.png");
}
/*
void drawText(float x, float y, float size, const char* text) {
	FTGLPolygonFont font("LiberationMono-Regular.ttf");
	font.FaceSize(size);
	glPushMatrix();
		glTranslatef(x, y, 0.0);
		font.Render(text);
	glPopMatrix();
}
*/
void draw() {
	// Update model and MVP matrix
	glm::mat4 translationMatrix = glm::translate(xoff, yoff, 1.0f);
	
	glm::mat4 rotationMatrix = glm::eulerAngleZ(roff) * glm::toMat4(accumRot);
	
	glm::mat4 scalingMatrix = glm::scale(zoom, zoom, zoom);
	
	modelMatrix = translationMatrix * rotationMatrix * scalingMatrix;
	mvpMatrix = projectionMatrix * viewMatrix * modelMatrix; // Remember, matrix multiplication is the other way around
	
	// Set up shader and load it with data
	glUseProgram(programID);

	glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &mvpMatrix[0][0]);
	
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, elevationMapID);
	glUniform1i(elevationMapLoc, 0);
	
	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, heightsColorMapID);
	glUniform1i(heightsColorMapLoc, 1);
	
	glActiveTexture(GL_TEXTURE2);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, depthsColorMapID);
	glUniform1i(depthsColorMapLoc, 2);
	
	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(positionLoc);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// 2nd attribute buffer : normals
	glEnableVertexAttribArray(texcoordLoc);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glVertexAttribPointer(texcoordLoc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	
	// pass other variables
	glUniform1i(levelLoc, level);
	glUniform1i(zminLoc, zmin);
	glUniform1i(zmaxLoc, zmax);
	
	// Draw
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, vertices.size() );

	
	// Clean up
	glDisableVertexAttribArray(positionLoc);
	glDisableVertexAttribArray(texcoordLoc);
	glUseProgram(0);
	
	// Flush to screen
	updateScreen();
}

glm::vec3 rotXY(glm::vec3 v, float angle) {
	glm::vec3 rv;
	rv.x = v.x*sin(angle) + v.y*cos(angle);
	rv.y = v.x*cos(angle) - v.y*sin(angle);
	rv.z = v.z;
	return rv;
}

int main( void ) {
	wiringPiSetup();
	
	for (int i=0; i<3; i++) {
		sendIR(0x10C8E11E, 32); // Acer on
		delay(40);
	}
	
	InitGraphics();
	
	init();
	
	// settings for travel globe
/*
 	Encoder tap(15,16,PUD_UP);
	Encoder ball1L(4,2,PUD_DOWN);
	Encoder ball1R(7,0,PUD_DOWN);
	Encoder ball2L(12,3,PUD_DOWN);
	Encoder ball2R(14,13,PUD_DOWN);
	Encoder ball3L(10,11,PUD_DOWN);
	Encoder ball3R(5,6,PUD_DOWN);
*/	
	// settings for home globe
 	Encoder tap(16,15,PUD_UP);
	Encoder ball1L(2,4,PUD_DOWN);
	Encoder ball1R(0,7,PUD_DOWN);
	Encoder ball3L(12,3,PUD_DOWN);
	Encoder ball3R(13,14,PUD_DOWN);
	Encoder ball2L(5,6,PUD_DOWN);
	Encoder ball2R(10,11,PUD_DOWN);
	
	unsigned long lastpoll = 0;
	unsigned long lastdraw = millis();
	unsigned long lastmove = millis();
	while(!quit) {
		if (micros() - lastpoll > 100) {
			lastpoll = micros();
			tap.update();
			ball1L.update();
			ball1R.update();
			ball2L.update();
			ball2R.update();
			ball3L.update();
			ball3R.update();
		}
		if (millis() - lastmove > 300000) {
			lastmove = millis();
			level = config_setting_get_float(config_setting_get_member(cfgRoot, "startlevel"));
			accumRot.w = config_setting_get_float_elem(config_setting_get_member(cfgRoot, "startpos"), 0);
			accumRot.x = config_setting_get_float_elem(config_setting_get_member(cfgRoot, "startpos"), 1);
			accumRot.y = config_setting_get_float_elem(config_setting_get_member(cfgRoot, "startpos"), 2);
			accumRot.z = config_setting_get_float_elem(config_setting_get_member(cfgRoot, "startpos"), 3);
		}
//		if (millis() - lastdraw > 40) { // 25 fps
		if (millis() - lastdraw > 100) {
			lastdraw = millis();
			
			int inc = tap.value();
			int sign = inc>=0 ? 1 : -1;
			
			level = max(min(level+sign*(int)round(pow(abs(inc), 1.7)), zmax), zmin);
			
			// read encoders and twist readings 45⁰ to align with trackball mounting
			glm::vec3 v0 = rotXY(glm::vec3(ball1L.value(), ball1R.value(), 0), PI/4);
			glm::vec3 v1 = rotXY(glm::vec3(ball2L.value(), ball2R.value(), 0), PI/4);
			glm::vec3 v2 = rotXY(glm::vec3(ball3L.value(), ball3R.value(), 0), PI/4);
			
			// printf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n", v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
			
			float scale = 1.0*PI/360.0;
			
			glm::vec3 rot;
			rot.x =   scale * (v1.y/sin(TWO_PI/3.0) + v2.y/sin(-TWO_PI/3.0)) / 2.0f;
			rot.y = - scale * (v0.y + v1.y/cos(TWO_PI/3.0) + v2.y/cos(-TWO_PI/3.0)) / 3.0f;
			rot.z = - scale * (v0.x + v1.x + v2.x) / 3.0f;
			
			if (glm::length(rot) > 0.001) lastmove = millis();
			accumRot = glm::normalize(glm::quat(rot.x, vec3(1.0, 0.0, 0.0))) * accumRot;
			accumRot = glm::normalize(glm::quat(rot.y, vec3(0.0, 1.0, 0.0))) * accumRot;
			accumRot = glm::normalize(glm::quat(rot.z, vec3(0.0, 0.0, 1.0))) * accumRot;

			int c;
			if(keyPressed(&c)) {
				lastmove = millis();
				switch(c) {
					case 'x': xoff-=0.01; break;
					case 'X': xoff+=0.01; break;
					case 'y': yoff+=0.01; break;
					case 'Y': yoff-=0.01; break;
					case 'z': zoom-=0.01; break;
					case 'Z': zoom+=0.01; break;
					case 'r': roff-=0.05; break;
					case 'R': roff+=0.05; break;
					case '=': level++; break;
					case '-': level--; break;
					case '+': level+= 5; break;
					case '_': level-= 5; break;
					case 's':
						config_setting_set_float(config_setting_get_member(cfgRoot, "zoom"), zoom);
						config_setting_set_float(config_setting_get_member(cfgRoot, "roff"), roff);
						config_setting_set_float(config_setting_get_member(cfgRoot, "xoff"), xoff);
						config_setting_set_float(config_setting_get_member(cfgRoot, "yoff"), yoff);
						config_write_file(&cfg, "planetarium.cfg");
						break;
					case 'S':
						config_setting_set_int(config_setting_get_member(cfgRoot, "startlevel"), level);
						config_setting_set_float_elem(config_setting_get_member(cfgRoot, "startpos"), 0, accumRot.w);
						config_setting_set_float_elem(config_setting_get_member(cfgRoot, "startpos"), 1, accumRot.x);
						config_setting_set_float_elem(config_setting_get_member(cfgRoot, "startpos"), 2, accumRot.y);
						config_setting_set_float_elem(config_setting_get_member(cfgRoot, "startpos"), 3, accumRot.z);
						config_write_file(&cfg, "planetarium.cfg");
						break;
					case 'p':
						printf("level = %i\n", level);
						printf("accumRot = %f, %f, %f, %F\n", accumRot.w, accumRot.x, accumRot.y, accumRot.z);
						break;
					case 'q':
					case 27:
						quit = true;
						break;
				}
			}
			draw();
		}
	}
	
	config_destroy(&cfg);
	
	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &elevationMapID);
	glDeleteTextures(1, &heightsColorMapID);
	glDeleteTextures(1, &depthsColorMapID);
	
	keyboardReset();
	return 0;
}
