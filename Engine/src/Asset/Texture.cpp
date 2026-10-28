#include "Texture.h"

#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/WebGPU/WebGPUTexture.h"
#include "Platform/Null/NullTexture.h"

Ref<Texture2D> Texture2D::GetMissingTexture()
{
	static Ref<Texture2D> s_MissingTexture;
	if (!s_MissingTexture)
	{
		uint32_t pixels[4][4];
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
				pixels[i][j] = ((i + j) % 2) ? 0xffff00ff : 0xff000000;
		}
		s_MissingTexture = Create(4, 4, Format::RGBA, 1, pixels);
		s_MissingTexture->SetFilterMethod(FilterMethod::Nearest);
	}
	return s_MissingTexture;
}

/* ------------------------------------------------------------------------------------------------------------------ */

Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, Format format, uint32_t samples, const void* pixels)
{
	PROFILE_FUNCTION();
	switch (Renderer::GetAPI())
	{
	case RendererAPI::API::None:
		return CreateRef<NullTexture2D>(width, height, format);
	case RendererAPI::API::OpenGL:
		return CreateRef<OpenGLTexture2D>(width, height, format, samples, pixels);
	case RendererAPI::API::WebGPU:
		return CreateRef<WebGPUTexture2D>(width, height, format, samples, pixels);
	default:
		break;
	}

	CORE_ASSERT(true, "Could not create Texture: Invalid Renderer API")
		return nullptr;
}

/* ------------------------------------------------------------------------------------------------------------------ */

Ref<Texture2D> Texture2D::Create(const std::filesystem::path& filepath)
{
	PROFILE_FUNCTION();
	switch (Renderer::GetAPI())
	{
	case RendererAPI::API::None:
		return CreateRef<NullTexture2D>(filepath);
	case RendererAPI::API::OpenGL:
		return CreateRef<OpenGLTexture2D>(filepath);
	case RendererAPI::API::WebGPU:
		return CreateRef<WebGPUTexture2D>(filepath);
	default:
		break;
	}

	CORE_ASSERT(true, "Could not create Texture: Invalid Renderer API")
		return nullptr;
}

Ref<Texture2D> Texture2D::Create(const std::filesystem::path& filepath, const std::vector<uint8_t>& imageData)
{
	PROFILE_FUNCTION();
	switch (Renderer::GetAPI())
	{
	case RendererAPI::API::None:
		return CreateRef<NullTexture2D>(filepath, imageData);
	case RendererAPI::API::OpenGL:
		return CreateRef<OpenGLTexture2D>(filepath, imageData);
	case RendererAPI::API::WebGPU:
		return CreateRef<WebGPUTexture2D>(filepath, imageData);

	default:
		break;
	}

	CORE_ASSERT(true, "Could not create Texture: Invalid Renderer API")
		return nullptr;
}


/* ------------------------------------------------------------------------------------------------------------------ */

bool Texture2D::Load(const std::filesystem::path& filepath)
{
	PROFILE_FUNCTION();
	m_Filepath = filepath;
	m_Filepath.make_preferred();
	return Reload();
}

bool Texture2D::Load(const std::filesystem::path& filepath, const std::vector<uint8_t>& data)
{
	ENGINE_ERROR("Textures can't be loaded from memory into an existing texture, use Texture2D::Create: {0}", filepath);
	return false;
}

/* ------------------------------------------------------------------------------------------------------------------ */

void TextureLibrary2D::Add(const Ref<Texture2D>& texture)
{
	PROFILE_FUNCTION();
	CORE_ASSERT(!Exists(texture->GetFilepath()), "Texture already exists!");
	m_Textures[texture->GetFilepath()] = texture;
}

/* ------------------------------------------------------------------------------------------------------------------ */

void TextureLibrary2D::Remove(const std::filesystem::path& name)
{
	PROFILE_FUNCTION();
	auto it = m_Textures.find(name);
	if (it != m_Textures.end())
	{
		m_Textures.erase(it);
	}
	else
	{
		ENGINE_ERROR("Texture not found: {0}", name);
	}
}

/* ------------------------------------------------------------------------------------------------------------------ */

Ref<Texture2D> TextureLibrary2D::Load(const std::filesystem::path& path, Ref<VirtualFileSystem> vfs)
{
	PROFILE_FUNCTION();
	if (Exists(path))
		return m_Textures[path];

	// A texture that can't be loaded is the checker placeholder, keeping its path
	Ref<Texture2D> texture;
	if (vfs && vfs->Exists(path) && !std::filesystem::exists(std::filesystem::absolute(Application::GetOpenDocumentDirectory() / path)))
	{
		std::vector<uint8_t> data;
		AssetManager::GetFileData(path.string(), data);
		texture = Texture2D::Create(path, data);
	}
	else
	{
		texture = Texture2D::Create(path);
	}

	if (texture)
		Add(texture);
	return texture;
}

/* ------------------------------------------------------------------------------------------------------------------ */

Ref<Texture2D> TextureLibrary2D::Get(const std::filesystem::path& name)
{
	CORE_ASSERT(Exists(name), "Texture does not exist!");
	return m_Textures[name];
}

/* ------------------------------------------------------------------------------------------------------------------ */

void TextureLibrary2D::Clear()
{
	PROFILE_FUNCTION();
	m_Textures.clear();
}

/* ------------------------------------------------------------------------------------------------------------------ */

bool TextureLibrary2D::Exists(const std::filesystem::path& name) const
{
	PROFILE_FUNCTION();
	return m_Textures.find(name) != m_Textures.end();
}