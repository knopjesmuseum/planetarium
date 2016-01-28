// IcoSphere.h
// Precision Framework
//
// Created by Abhishek Sagi on 11/16/13.
// Original Author Anreas Kahler

#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <utility>
#include <algorithm>
#include <math.h>

#define TWO_PI 6.2831853
#define PI 3.1415927

struct TriangleIndices {
	int v1, v2, v3;
	TriangleIndices(int v1, int v2, int v3) {
		this->v1 = v1; this->v2 = v2; this->v3 = v3;
	}
};

class IcoSphere {
public:
	IcoSphere() {}
	~IcoSphere() {}
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> texcoords;
	std::vector<unsigned int> indices;
	void Create(int recursionLevel) {
		middlePointIndexCache.clear();
		vertices.clear();
		uvs.clear();
		normals.clear();
		texcoords.clear();
		indices.clear();
		index = 0;
		
		double t = (1.0 + sqrt(5.0)) / 2.0;

		AddVertex(glm::vec3( -1, t, 0));
		AddVertex(glm::vec3( 1, t, 0));
		AddVertex(glm::vec3( -1, -t, 0));
		AddVertex(glm::vec3( 1, -t, 0));

		AddVertex(glm::vec3( 0, -1, t));
		AddVertex(glm::vec3( 0, 1, t));
		AddVertex(glm::vec3( 0, -1, -t));
		AddVertex(glm::vec3( 0, 1, -t));

		AddVertex(glm::vec3( t, 0, -1));
		AddVertex(glm::vec3( t, 0, 1));
		AddVertex(glm::vec3( -t, 0, -1));
		AddVertex(glm::vec3( -t, 0, 1));

		std::vector<TriangleIndices> faces;
		faces.push_back(TriangleIndices(0, 11, 5));
		faces.push_back(TriangleIndices(0, 5, 1));
		faces.push_back(TriangleIndices(0, 1, 7));
		faces.push_back(TriangleIndices(0, 7, 10));
		faces.push_back(TriangleIndices(0, 10, 11));

		faces.push_back(TriangleIndices(1, 5, 9));
		faces.push_back(TriangleIndices(5, 11, 4));
		faces.push_back(TriangleIndices(11, 10, 2));
		faces.push_back(TriangleIndices(10, 7, 6));
		faces.push_back(TriangleIndices(7, 1, 8));

		faces.push_back(TriangleIndices(3, 9, 4));
		faces.push_back(TriangleIndices(3, 4, 2));
		faces.push_back(TriangleIndices(3, 2, 6));
		faces.push_back(TriangleIndices(3, 6, 8));
		faces.push_back(TriangleIndices(3, 8, 9));

		faces.push_back(TriangleIndices(4, 9, 5));
		faces.push_back(TriangleIndices(2, 4, 11));
		faces.push_back(TriangleIndices(6, 2, 10));
		faces.push_back(TriangleIndices(8, 6, 7));
		faces.push_back(TriangleIndices(9, 8, 1));

		for(int i = 0; i < recursionLevel; ++i) {
			std::vector<TriangleIndices> faces2;
	
			for(std::vector<TriangleIndices>::iterator tri = faces.begin(); tri != faces.end(); ++tri) {
				int a = GetMiddlePoint(tri->v1, tri->v2);
				int b = GetMiddlePoint(tri->v2, tri->v3);
				int c = GetMiddlePoint(tri->v3, tri->v1);

				faces2.push_back(TriangleIndices(tri->v1, a, c));
				faces2.push_back(TriangleIndices(tri->v2, b, a));
				faces2.push_back(TriangleIndices(tri->v3, c, b));
				faces2.push_back(TriangleIndices(a, b, c));
			}

			faces.clear();
			for(unsigned int j = 0; j < faces2.size(); ++j) {
				faces.push_back(faces2[j]);
			}
		}
		
		for(std::vector<TriangleIndices>::iterator tri = faces.begin(); tri != faces.end(); ++tri) {
			indices.push_back(tri->v1);
			indices.push_back(tri->v2);
			indices.push_back(tri->v3);
		}
	}

private:
	int index;
	std::map<int64_t, int> middlePointIndexCache;
	int AddVertex(glm::vec3 position) {
		position = glm::normalize(position);
		vertices.push_back(position);
		texcoords.push_back(glm::vec3(position.z, position.y, position.x));
		normals.push_back(glm::vec3(position.z, position.y, position.x));

		double u, v;
		if (position.x!=0.0f || position.z!=0.0f) u = 0.5 + atan2(position.x, position.z)/TWO_PI;
		else u = 0.0f;
		v = 0.5 + asin(position.y)/PI;
		uvs.push_back(glm::vec2(u, v));

		return index++;
	}
	int GetMiddlePoint(int p1, int p2) {
		bool firstPointIsSmaller = p1 < p2;
		int64_t smallerIndex = firstPointIsSmaller ? p1 : p2;
		int64_t greaterIndex = firstPointIsSmaller ? p2 : p1;
		int64_t key = (smallerIndex << 32) + greaterIndex;

		std::map<int64_t, int>::iterator foundValueIterator = middlePointIndexCache.find(key);
		if(foundValueIterator != middlePointIndexCache.end()) {
			return foundValueIterator->second;
		}

		glm::vec3 point1 = vertices[p1];
		glm::vec3 point2 = vertices[p2];
		glm::vec3 middle = glm::vec3( (point1.x + point2.x) / 2.0,
		(point1.y + point2.y) / 2.0,
		(point1.z + point2.z) / 2.0);

		int i = this->AddVertex(middle);

		this->middlePointIndexCache.insert(std::make_pair(key, i));
		return i;
	}
};
