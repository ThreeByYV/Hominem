#include "hmnpch.h"
//
//#include "Shader.h"
//#include <glad/glad.h>
//
//Hominem::Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc)
//{
//}
//
//Hominem::Shader::~Shader()
//{
//	glDeleteProgram(m_RendererID);
//}
//
//void Hominem::Shader::Bind() const
//{
//	glUseProgram(m_RendererID);
//}
//
//void Hominem::Shader::Unbind() const
//{
//	glUseProgram(0);
//}
//
//void Hominem::Shader::UploadUniformMat4(const glm::mat4& matrix)
//{
//	//glGetUniformfv();
//	//glUniformMatrix4fv();
//}
//
//uint32_t Hominem::Shader::CreateComputeShader(const std::filesystem::path& path)
//{
//	std::string shaderSource = ReadFile(path);
//
//	GLuint shaderHandle = glCreateShader(GL_COMPUTE_SHADER);
//
//	const GLchar* source = (const GLchar*)shaderSource.c_str();
//	glShaderSource(shaderHandle, 1, &source, 0);
//
//	glCompileShader(shaderHandle);
//
//	GLint isCompiled = 0;
//	glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &isCompiled);
//	if (isCompiled == GL_FALSE)
//	{
//		GLint maxLength = 0;
//		glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &maxLength);
//
//		std::vector<GLchar> infoLog(maxLength);
//		glGetShaderInfoLog(shaderHandle, maxLength, &maxLength, &infoLog[0]);
//
//		std::cerr << infoLog.data() << std::endl;
//
//		glDeleteShader(shaderHandle);
//		return -1;
//	}
//
//	GLuint program = glCreateProgram();
//	glAttachShader(program, shaderHandle);
//	glLinkProgram(program);
//
//	GLint isLinked = 0;
//	glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
//	if (isLinked == GL_FALSE)
//	{
//		GLint maxLength = 0;
//		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
//
//		std::vector<GLchar> infoLog(maxLength);
//		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
//
//		std::cerr << infoLog.data() << std::endl;
//
//		glDeleteProgram(program);
//		glDeleteShader(shaderHandle);
//
//		return -1;
//	}
//
//	//glDetachShader(program, shaderHandle);
//	return program;
//}
//
//uint32_t Hominem::Shader::ReloadComputeShader(uint32_t shaderHandle, const std::filesystem::path& path)
//{
//	uint32_t newShaderHandle = CreateComputeShader(path);
//
//	// Return old shader if compilation failed
//	if (newShaderHandle == -1)
//		return shaderHandle;
//
//	glDeleteProgram(shaderHandle);
//	return newShaderHandle;
//}
//
//uint32_t Hominem::Shader::CreateBaseShaders(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
//{
//	
//	std::string vertexShaderSource = ReadFile(vertexPath);
//	std::string fragmentShaderSource = ReadFile(fragmentPath);
//
//	// Vertex shader
//
//	GLuint vertexShaderHandle = glCreateShader(GL_VERTEX_SHADER);
//
//	const GLchar* source = (const GLchar*)vertexShaderSource.c_str();
//	glShaderSource(vertexShaderHandle, 1, &source, 0);
//
//	glCompileShader(vertexShaderHandle);
//
//	GLint isCompiled = 0;
//	glGetShaderiv(vertexShaderHandle, GL_COMPILE_STATUS, &isCompiled);
//	if (isCompiled == GL_FALSE)
//	{
//		GLint maxLength = 0;
//		glGetShaderiv(vertexShaderHandle, GL_INFO_LOG_LENGTH, &maxLength);
//
//		std::vector<GLchar> infoLog(maxLength);
//		glGetShaderInfoLog(vertexShaderHandle, maxLength, &maxLength, &infoLog[0]);
//
//		std::cerr << infoLog.data() << std::endl;
//
//		glDeleteShader(vertexShaderHandle);
//		return -1;
//	}
//
//	// Fragment shader
//
//	GLuint fragmentShaderHandle = glCreateShader(GL_FRAGMENT_SHADER);
//
//	source = (const GLchar*)fragmentShaderSource.c_str();
//	glShaderSource(fragmentShaderHandle, 1, &source, 0);
//
//	glCompileShader(fragmentShaderHandle);
//
//	isCompiled = 0;
//	glGetShaderiv(fragmentShaderHandle, GL_COMPILE_STATUS, &isCompiled);
//	if (isCompiled == GL_FALSE)
//	{
//		GLint maxLength = 0;
//		glGetShaderiv(fragmentShaderHandle, GL_INFO_LOG_LENGTH, &maxLength);
//
//		std::vector<GLchar> infoLog(maxLength);
//		glGetShaderInfoLog(fragmentShaderHandle, maxLength, &maxLength, &infoLog[0]);
//
//		std::cerr << infoLog.data() << std::endl;
//
//		glDeleteShader(fragmentShaderHandle);
//		return -1;
//	}
//
//	// Program linking
//
//	GLuint program = glCreateProgram();
//	glAttachShader(program, vertexShaderHandle);
//	glAttachShader(program, fragmentShaderHandle);
//	glLinkProgram(program);
//
//	GLint isLinked = 0;
//	glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
//	if (isLinked == GL_FALSE)
//	{
//		GLint maxLength = 0;
//		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
//
//		std::vector<GLchar> infoLog(maxLength);
//		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
//
//		std::cerr << infoLog.data() << std::endl;
//
//		glDeleteProgram(program);
//		glDeleteShader(vertexShaderHandle);
//		glDeleteShader(fragmentShaderHandle);
//
//		return -1;
//	}
//
//	glDetachShader(program, vertexShaderHandle);
//	glDetachShader(program, fragmentShaderHandle);
//	return program;
//}
//
//uint32_t Hominem::Shader::ReloadBaseShaders(uint32_t shaderHandle, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
//{
//	uint32_t newShaderHandle = CreateBaseShaders(vertexPath, fragmentPath);
//
//	// Return old shader if compilation failed
//	if (newShaderHandle == -1)
//		return shaderHandle;
//
//	glDeleteProgram(shaderHandle);
//	return newShaderHandle;
//}
//
//std::string Hominem::Shader::ReadFile(const std::filesystem::path& path)
//{
//	std::ifstream file(path);
//
//	if (!file.is_open())
//	{
//		std::cerr << "Failed to open file: " << path.string() << std::endl;
//		return {};
//	}
//
//	std::ostringstream contentStream;
//	contentStream << file.rdbuf();
//	return contentStream.str();
//}
