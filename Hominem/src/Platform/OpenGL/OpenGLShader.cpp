#include "hmnpch.h"

#include "OpenGLShader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include "Hominem/Utils/FileUtils.h"
#include "Hominem/Core/VFS.h"

namespace Hominem {

	static GLenum ShaderTypeFromString(const std::string& type)
	{
		if (type == "vertex")                        return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel")   return GL_FRAGMENT_SHADER;
		if (type == "geometry")                      return GL_GEOMETRY_SHADER;
		if (type == "tcs" || type == "tesscontrol")  return GL_TESS_CONTROL_SHADER;
		if (type == "tes" || type == "tesseval")     return GL_TESS_EVALUATION_SHADER;

		HMN_CORE_ASSERT(false, "Unknown shader type!");
		return 0;
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
		: m_Name(name)
	{
		std::unordered_map<GLenum, std::string> sources;

		std::string vertexShaderSource = ReadTextFile(vertexPath);
		std::string fragmentShaderSource = ReadTextFile(fragmentPath);

		sources[GL_VERTEX_SHADER] = vertexShaderSource;
		sources[GL_FRAGMENT_SHADER] = fragmentShaderSource;
		Compile(sources);
	}

	std::string OpenGLShader::InjectDefines(const std::string& sectionSrc,
	                                          const std::vector<std::string>& defines)
	{
		if (defines.empty()) return sectionSrc;

		// Find end of the #version line and insert defines right after it
		size_t versionEnd = sectionSrc.find('\n');
		if (versionEnd == std::string::npos) return sectionSrc;

		std::string preamble;
		for (const auto& def : defines)
			preamble += "#define " + def + "\n";

		return sectionSrc.substr(0, versionEnd + 1)
		     + preamble
		     + sectionSrc.substr(versionEnd + 1);
	}

	std::string OpenGLShader::ResolveIncludes(const std::string& source,
	                                            const std::string& shaderDir,
	                                            int                sourceId,
	                                            int&               nextId,
	                                            std::unordered_map<int, std::string>& sourceMap)
	{
		std::string result = source;
		const std::string token = "#include";
		size_t pos = 0;

		while ((pos = result.find(token, pos)) != std::string::npos)
		{
			size_t q1 = result.find('"', pos);
			size_t q2 = result.find('"', q1 + 1);
			if (q1 == std::string::npos || q2 == std::string::npos) break;

			// Count lines up to this include so we can resume the parent's line counter after
			int resumeLine = 1 + (int)std::count(result.begin(), result.begin() + pos, '\n');

			std::string includePath = shaderDir + "/" + result.substr(q1 + 1, q2 - q1 - 1);
			int fileId = nextId++;
			sourceMap[fileId] = includePath;

			std::string content = ReadTextFile(includePath);
			content = ResolveIncludes(content, shaderDir, fileId, nextId, sourceMap);

			// Wrap with #line so GLSL error messages show file:line instead of expanded-blob line
			std::string wrapped =
			    "\n#line 1 " + std::to_string(fileId) + "\n" +
			    content +
			    "\n#line " + std::to_string(resumeLine + 1) + " " + std::to_string(sourceId) + "\n";

			size_t lineEnd = result.find('\n', pos);
			result.replace(pos, (lineEnd == std::string::npos ? result.size() : lineEnd) - pos, wrapped);
			pos += wrapped.size();
		}

		return result;
	}

	OpenGLShader::OpenGLShader(const std::string& filepath,
	                            const std::vector<std::string>& defines)
		: m_Defines(defines)
	{
		std::string resolved = VFS::Resolve(filepath);
		std::string source   = ReadTextFile(resolved);
		std::string dir      = std::filesystem::path(resolved).parent_path().string();
		m_SourceMap.clear();
		m_SourceMap[0]     = resolved;
		int nextId         = 1;
		source             = ResolveIncludes(source, dir, 0, nextId, m_SourceMap);
		auto sections      = PreProcess(source);
		for (auto& [type, src] : sections)
			src = InjectDefines(src, defines);
		Compile(sections);

		m_Filepath     = filepath;
		m_IsSingleFile = true;

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count   = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	OpenGLShader::OpenGLShader(const std::string& filepath)
	{
		std::string resolved = VFS::Resolve(filepath);
		std::string source   = ReadTextFile(resolved);
		std::string dir      = std::filesystem::path(resolved).parent_path().string();
		m_SourceMap.clear();
		m_SourceMap[0]      = resolved;
		int nextId          = 1;
		source              = ResolveIncludes(source, dir, 0, nextId, m_SourceMap);
		auto shaderSources  = PreProcess(source);
		Compile(shaderSources);

		// Store filepath so we can reload later
		m_Filepath = filepath;
		m_IsSingleFile = true;

		//Get the name from a filepath
		
		//Resources/Shaders/frag.glsl
		auto lastSlash = filepath.find_last_of("/\\");
		//frag.glsl
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1; //skip the slash
		
		auto lastDot = filepath.rfind('.');
		
		//Resources/Shaders/frag
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		
		m_Name = filepath.substr(lastSlash, count);

	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name)
	{
		std::unordered_map<GLenum, std::string> sources;

		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;
		Compile(sources);
	}

	std::string OpenGLShader::ReadTextFile(const std::filesystem::path& path)
	{
		return FileUtils::ReadTextFile(path);
	}


	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source)
	{
		 std::unordered_map<GLenum, std::string> shaderSources;

		 const char* typeToken = "#type";

		 size_t typeTokenLength = strlen(typeToken);
		 size_t pos = source.find(typeToken, 0);

		 while (pos != std::string::npos)
		 {
			 size_t eol = source.find_first_of("\r\n", pos); //finds any new lines or character returns
			 HMN_CORE_ASSERT(eol != std::string::npos, "Syntax error in your shader!");

			 size_t begin = pos + typeTokenLength + 1; //moves one character forward
			
			 std::string type = source.substr(begin, eol - begin);
			 HMN_CORE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

			 size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			 pos = source.find(typeToken, nextLinePos);
			 
			/* If we’re at the last block → grab from nextLinePos to end of file.
				Otherwise → grab from nextLinePos up to the next #type */
			 shaderSources[ShaderTypeFromString(type)] =
				 (pos == std::string::npos)
				 ? source.substr(nextLinePos)
				 : source.substr(nextLinePos, pos - nextLinePos);

		 }

		 return shaderSources;
	}

	void OpenGLShader::Compile(std::unordered_map<GLenum, std::string> shaderSources)
	{
		GLuint program = glCreateProgram();

		HMN_CORE_ASSERT(shaderSources.size() <= 5, "We only support up to 5 shaders (vert/tcs/tes/geom/frag)");
		std::array<GLenum, 5> glShaderIDs = {0, 0, 0, 0, 0};

		int glShaderIDIndex = 0;

		for (auto& keyValue : shaderSources)
		{
			GLenum shaderType = keyValue.first;
			const std::string& source = keyValue.second;

			GLuint shader = glCreateShader(shaderType);

			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);


			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		
			if (isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				glDeleteShader(shader);

				// Replace numeric source IDs with filenames so errors read:
				// "ERROR: pbr.glsl:15" instead of "ERROR: 2:15"
				std::string log = infoLog.data();
				for (const auto& [id, path] : m_SourceMap)
				{
					std::string idStr  = std::to_string(id);
					std::string name   = std::filesystem::path(path).filename().string();
					// Replace both "id:line" (AMD/Intel) and "id(line)" (NVIDIA) patterns
					for (const std::string prefix : { idStr + ":", idStr + "(" })
					{
						size_t p = 0;
						while ((p = log.find(prefix, p)) != std::string::npos)
						{
							log.replace(p, idStr.size(), name);
							p += name.size();
						}
					}
				}
				HMN_CORE_ERROR("Shader compile error in '{}':\n{}", m_Name, log);
				HMN_CORE_ASSERT(false, "Shader compilation failure!");
				break;
			}

			glAttachShader(program, shader);
			glShaderIDs[glShaderIDIndex++] = shader;
		}

		m_RendererID = program; //rendererID can never be invalid program since everything above was successful

		// Link our program
		glLinkProgram(program);

		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
	
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			// We don't need the program anymore.
			glDeleteProgram(program);

			for (auto id : glShaderIDs)
				if (id) glDeleteShader(id);

			HMN_CORE_ERROR("{0}", infoLog.data());
			HMN_CORE_ASSERT(false, "Shader link failure!");
			return;
		}

		// Always detach shaders after a successful link.
		for (auto id : glShaderIDs)
			if (id) glDetachShader(program, id);

		// Explicitly bind the SceneUBO block to point 0.
		// layout(binding=N) is GL 4.2+ but some drivers ignore it without this call.
		GLuint sceneUBOIndex = glGetUniformBlockIndex(program, "SceneUBO");
		if (sceneUBOIndex != GL_INVALID_INDEX)
			glUniformBlockBinding(program, sceneUBOIndex, 0);
	}
	
	void OpenGLShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::Reload()
	{
		if (!m_IsSingleFile || m_Filepath.empty())
		{
			HMN_CORE_WARN("Shader '{0}' does not support hot reload (not created from a single file).", m_Name);
			return;
		}

		HMN_CORE_INFO("Reloading shader from file: {0}", m_Filepath);

		std::string source = ReadTextFile(VFS::Resolve(m_Filepath));
		if (source.empty())
		{
			HMN_CORE_ERROR("Failed to read shader file during reload: {0}", m_Filepath);
			return;
		}

		std::string resolved = VFS::Resolve(m_Filepath);
		std::string dir      = std::filesystem::path(resolved).parent_path().string();
		m_SourceMap.clear();
		m_SourceMap[0]       = resolved;
		int nextId         = 1;
		source             = ResolveIncludes(source, dir, 0, nextId, m_SourceMap);
		auto shaderSources = PreProcess(source);
		for (auto& [type, src] : shaderSources)
			src = InjectDefines(src, m_Defines);

		uint32_t oldProgram = m_RendererID;

		// Reset uniform cache, we’ll re-query locations for the new program
		m_UniformLocationCache.clear();

		Compile(shaderSources);

		// Clean up the old program
		if (oldProgram)
		{
			glDeleteProgram(oldProgram);
		}

		HMN_CORE_INFO("Shader '{0}' reloaded successfully.", m_Name);
	}


	void OpenGLShader::UnbindAll()
	{
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
		UploadUniformFloat(name, value);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		UploadUniformMat4(name, value);
	}

	void OpenGLShader::SetIntLoc(int loc, int v)
	{
		glUniform1i(loc, v);
	}

	void OpenGLShader::SetFloatLoc(int loc, float v)
	{
		glUniform1f(loc, v);
	}

	void OpenGLShader::SetFloat3Loc(int loc, const glm::vec3& v)
	{
		glUniform3f(loc, v.x, v.y, v.z);
	}

	void OpenGLShader::SetFloat4Loc(int loc, const glm::vec4& v)
	{
		glUniform4f(loc, v.x, v.y, v.z, v.w);
	}

	void OpenGLShader::SetMat4Loc(int loc, const glm::mat4& v)
	{
		glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(v));
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		GLint location = GetUniformLocation(name);
		glUniform1i(location, value);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		GLint location = GetUniformLocation(name);
		glUniform1f(location, value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& values)
	{
		GLint location = GetUniformLocation(name);
		glUniform2f(location, values.x, values.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& values)
	{
		GLint location = GetUniformLocation(name);
		glUniform3f(location, values.x, values.y, values.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& values)
	{
		GLint location = GetUniformLocation(name);
		glUniform4f(location, values.x, values.y, values.z, values.w);
	}

	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		GLint location = GetUniformLocation(name);
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		GLint location = GetUniformLocation(name);
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}


	GLint OpenGLShader::GetUniformLocation(const std::string& name) const
	{
		if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
		{
			return m_UniformLocationCache[name];
		}

		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformLocationCache[name] = location;
		return location;
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_RendererID);
	}


	OpenGLComputeShader::OpenGLComputeShader(const std::string& filepath)
	{
		// Derive name from filename without extension
		auto lastSlash = filepath.find_last_of("/\\");
		auto start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = (lastDot == std::string::npos) ? filepath.size() - start : lastDot - start;
		m_Name = filepath.substr(start, count);

		std::string src = FileUtils::ReadTextFile(VFS::Resolve(filepath));
		if (src.empty()) return;

		const char* cstr = src.c_str();
		GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(shader, 1, &cstr, nullptr);
		glCompileShader(shader);

		GLint ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			GLint len = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
			std::string log(len, '\0');
			glGetShaderInfoLog(shader, len, nullptr, log.data());
			HMN_CORE_ERROR("ComputeShader '{}' compile error: {}", filepath, log);
			glDeleteShader(shader);
			return;
		}

		m_RendererID = glCreateProgram();
		glAttachShader(m_RendererID, shader);
		glLinkProgram(m_RendererID);
		glDetachShader(m_RendererID, shader);
		glDeleteShader(shader);

		GLint linked = 0;
		glGetProgramiv(m_RendererID, GL_LINK_STATUS, &linked);
		if (!linked)
		{
			GLint len = 0;
			glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &len);
			std::string log(len, '\0');
			glGetProgramInfoLog(m_RendererID, len, nullptr, log.data());
			HMN_CORE_ERROR("ComputeShader '{}' link error: {}", filepath, log);
			glDeleteProgram(m_RendererID);
			m_RendererID = 0;
			return;
		}

		HMN_CORE_INFO("ComputeShader '{}' compiled OK", m_Name);
	}

	OpenGLComputeShader::~OpenGLComputeShader()
	{
		if (m_RendererID) glDeleteProgram(m_RendererID);
	}

	void OpenGLComputeShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLComputeShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const
	{
		glDispatchCompute(groupsX, groupsY, groupsZ);
		// Barrier so the VS can safely read the SSBO output
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void OpenGLComputeShader::SetUint(const std::string& name, uint32_t value)
	{
		glUniform1ui(GetUniformLocation(name), value);
	}

	void OpenGLComputeShader::SetInt(const std::string& name, int value)
	{
		glUniform1i(GetUniformLocation(name), value);
	}

	void OpenGLComputeShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
	}

	GLint OpenGLComputeShader::GetUniformLocation(const std::string& name) const
	{
		auto it = m_UniformCache.find(name);
		if (it != m_UniformCache.end()) return it->second;
		GLint loc = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformCache[name] = loc;
		return loc;
	}

}