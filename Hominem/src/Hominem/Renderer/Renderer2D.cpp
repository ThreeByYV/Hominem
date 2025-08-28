#include "hmnpch.h"
#include "Renderer2D.h"

#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLShader.h"


namespace Hominem {

	struct Renderer2DStorage
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> TextureShader;
		Ref<Shader> BasicShader;
		Ref<ShaderLibrary> ShaderLibrary;
		Ref<IndexBuffer> IndexBuffer;
		Ref<Texture2D> Texture2D;
		Ref<VertexBuffer> VertexBuffer;
	};

	static Renderer2DStorage* s_Data; 	//hidden from other translation units when linked

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DStorage();
		s_Data->QuadVertexArray = VertexArray::Create();

		s_Data->ShaderLibrary = std::make_shared<ShaderLibrary>();

		float vertices[4 * 5] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // 0: Bottom-left
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // 1: Bottom-right  
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,  // 2: Top-right
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f   // 3: Top-left
		};

		s_Data->VertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Pos" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		s_Data->VertexBuffer->SetLayout(layout);
		s_Data->QuadVertexArray->AddVertexBuffer(s_Data->VertexBuffer); //todo may need to leverage Unbind more, since there could be bad associations to the bind VAO


		unsigned int indices[6] = {
			0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
		};

		s_Data->IndexBuffer = IndexBuffer::Create(indices, ARRAY_SIZE_IN_ELEMENTS(indices));
		s_Data->QuadVertexArray->SetIndexBuffer(s_Data->IndexBuffer);

		//s_Data->TextureShader = s_Data->ShaderLibrary->Load("src/Hominem/Resources/Shaders/texture.glsl");
		s_Data->BasicShader = s_Data->ShaderLibrary->Load("src/Hominem/Resources/Shaders/frag.glsl");
		
		s_Data->Texture2D = Texture2D::Create("src/Hominem/Resources/Textures/drip.jpg");
		
		s_Data->Texture2D->Bind();
	}

	void Renderer2D::Shutdown()
	{
		delete s_Data; //need to destory any vertex arrays in the GPU while it's still in OpenGL context
	}

	void Renderer2D::BeginScene(OrthographicCamera& camera)
	{
		//usually only need to bind and upload the core shaders once, which is why they're here
		
		//std::dynamic_pointer_cast<OpenGLShader>(s_Data->TextureShader)->UploadUniformInt("u_Texture", 0); //the bound texture slot is 0
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->BasicShader)->Bind();
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->BasicShader)->UploadUniformMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->BasicShader)->UploadUniformMat4("u_Transform", glm::mat4(1.0f));

	}

	void Renderer2D::EndScene()
	{

	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->BasicShader)->Bind();
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->BasicShader)->UploadUniformFloat3("u_Color", glm::vec3(color.r, color.g, color.b));

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}
}
