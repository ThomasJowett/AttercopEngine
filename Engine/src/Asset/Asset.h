#pragma once

#include "Core/core.h"
#include "Asset/VirtualFileSystem.h"
#include "Core/Application.h"
#include "Utilities/FileUtils.h"
#include "Logging/Instrumentor.h"

#include <filesystem>

class Asset
{
public:
	virtual const std::filesystem::path& GetFilepath() const { return m_Filepath; };

	virtual bool Load(const std::filesystem::path& filepath) = 0;
	virtual bool Load(const std::filesystem::path& filepath, const std::vector<uint8_t>& data) = 0;

	virtual bool Reload()
	{
		bool loaded = Load(m_Filepath);
		if (loaded)
			m_Missing = false;
		return loaded;
	}

	// True when loading failed and this is a placeholder that keeps the asset's path
	bool IsMissing() const { return m_Missing; }

	// Turns this into the placeholder for an asset that failed to load
	virtual void MakeMissing(const std::filesystem::path& filepath)
	{
		m_Filepath = filepath;
		m_Filepath.make_preferred();
		m_Missing = true;
	}
protected:
	std::filesystem::path m_Filepath;
	bool m_Missing = false;
};

class AssetLibrary
{
public:
	AssetLibrary() = default;

	void Add(const Ref<Asset>& resource)
	{
		PROFILE_FUNCTION();
		CORE_ASSERT(!Exists(resource->GetFilepath()), "Asset already exists!");
		if(!resource->GetFilepath().empty())
			m_Assets[ToKey(resource->GetFilepath())] = resource;
	}

	void Remove(const std::filesystem::path& path)
	{
		PROFILE_FUNCTION();
		const std::filesystem::path filepath = ToKey(path);
		auto it = m_Assets.find(filepath);
		if (it != m_Assets.end())
		{
			m_Assets.erase(it);
		}
		else
		{
			ENGINE_ERROR("Asset not found: {0}", filepath);
		}
	}

	template<typename T>
	Ref<T> Load(const std::filesystem::path& path, Ref<VirtualFileSystem> vfs)
	{
		PROFILE_FUNCTION();
		const std::filesystem::path filepath = ToKey(path);
		if (Exists(filepath))
			return std::dynamic_pointer_cast<T>(m_Assets[filepath].lock());

		Ref<T> asset;
		if (filepath.empty())
		{
			ENGINE_ERROR("Cannot load asset with empty filepath");
			return nullptr;
		}
		// An asset that can't be loaded becomes a placeholder that keeps its path, so saving doesn't lose the reference
		asset = CreateRef<T>();
		bool loaded = false;
		std::filesystem::path absolutePath = std::filesystem::absolute(Application::GetOpenDocumentDirectory() / filepath);
		if (std::filesystem::exists(absolutePath)) {
			ENGINE_INFO("Loading asset from disk: {0}", absolutePath);
			loaded = asset->Load(filepath);
			if (!loaded)
				ENGINE_ERROR("Failed to load asset: {0}", filepath);
		}
		else if (vfs && vfs->Exists(filepath))
		{
			std::vector<uint8_t> data;
			if (vfs->ReadFile(filepath, data))
			{
				ENGINE_INFO("Loading asset from bundle: {0}", filepath);
				loaded = asset->Load(filepath, data);
				if (!loaded)
					ENGINE_ERROR("Failed to load asset from bundle: {0}", filepath);
			}
			else
			{
				ENGINE_ERROR("Could not read asset from bundle: {0}", filepath);
			}
		}
		else {
			ENGINE_ERROR("Asset does not exist: {0}", filepath);
		}

		if (!loaded)
		{
			asset = CreateRef<T>();
			asset->MakeMissing(filepath);
		}
		Add(asset);
		return std::dynamic_pointer_cast<T>(asset);
	}

	Ref<Asset> Get(const std::filesystem::path& path)
	{
		PROFILE_FUNCTION();
		const std::filesystem::path filepath = ToKey(path);
		CORE_ASSERT(Exists(filepath), "Asset does not Exist!");
		return m_Assets[filepath].lock();
	}

	template<typename T>
	Ref<T> Get(const std::filesystem::path& path)
	{
		PROFILE_FUNCTION();
		const std::filesystem::path filepath = ToKey(path);
		CORE_ASSERT(Exists(filepath), "Asset does not Exist!");
		return std::dynamic_pointer_cast<T>(m_Assets[filepath].lock());
	}

	bool Exists(const std::filesystem::path& path)
	{
		PROFILE_FUNCTION();
		const std::filesystem::path filepath = ToKey(path);
		if (m_Assets.find(filepath) != m_Assets.end())
			return m_Assets[filepath].use_count() > 0;
		return false;
	}

	void Clear()
	{
		PROFILE_FUNCTION();
		m_Assets.clear();
	}

	void CleanUnused()
	{
		PROFILE_FUNCTION();
		auto iter = m_Assets.begin();
		while (iter != m_Assets.end())
		{
			if (iter->second.use_count() <= 0)
				iter = m_Assets.erase(iter);
			else
				++iter;
		}
	}

private:
	// Assets are keyed by their project-relative path, however they were requested
	static std::filesystem::path ToKey(const std::filesystem::path& path)
	{
		return FileUtils::ToProjectRelative(path, Application::GetOpenDocumentDirectory());
	}

	std::unordered_map<std::filesystem::path, std::weak_ptr<Asset>> m_Assets;
};