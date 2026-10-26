#pragma once
#include "Asset/Asset.h"

class LuaScript : public Asset
{
public:
	LuaScript() = default;
	LuaScript(const std::filesystem::path& filepath);
	LuaScript(const std::filesystem::path& filepath, const std::vector<uint8_t>& data);
	// Inherited via Asset
	bool Load(const std::filesystem::path& filepath) override;
	bool Load(const std::filesystem::path& filepath, const std::vector<uint8_t>& data) override;

	const std::string& GetSource() const { return m_Source; }

private:
	std::string m_Source;
};
