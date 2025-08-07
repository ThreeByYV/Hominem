#pragma once

#include <glad/glad.h>
#include <stdexcept>

class Mesh
{
public:
	Mesh();

	void CreateMesh(GLfloat* vertices, unsigned int* indices, unsigned int numOfVertices, unsigned int numOfIndices);
	void RenderMesh();
	void ClearMesh();

	~Mesh();


private:
	GLuint m_VBO, m_VAO, m_IBO;
	GLsizei m_indexCount;
};