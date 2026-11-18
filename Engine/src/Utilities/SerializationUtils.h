#pragma once

#include "math/Vector2f.h"
#include "math/Vector3f.h"

#include "Asset/Material.h"

#include "Core/Colour.h"
#include "Asset/Asset.h"

#include "TinyXml2/tinyxml2.h"
#include "Scene/AssetManager.h"

namespace SerializationUtils
{
void Encode(tinyxml2::XMLElement* pElement, const Vector2f& vec2);

void Decode(tinyxml2::XMLElement const* pElement, Vector2f& vec2);

void Encode(tinyxml2::XMLElement* pElement, const Vector3f& vec3);

void Decode(tinyxml2::XMLElement const* pElement, Vector3f& vec3);

void Encode(tinyxml2::XMLElement* pElement, const Colour& colour);

void Decode(tinyxml2::XMLElement const* pElement, Colour& colour);

void Encode(tinyxml2::XMLElement* pElement, const std::filesystem::path& filepath);

void Decode(tinyxml2::XMLElement const* pElement, std::filesystem::path& filepath);

// A texture reference plus the filter/wrap its owner samples it with
void Encode(tinyxml2::XMLElement* pElement, const Ref<Texture2D>& texture, Texture::FilterMethod filterMethod, Texture::WrapMethod wrapMethod);

void Decode(tinyxml2::XMLElement const* pElement, Ref<Texture2D>& texture, Texture::FilterMethod& filterMethod, Texture::WrapMethod& wrapMethod);

std::string RelativePath(const std::filesystem::path& path);
std::filesystem::path AbsolutePath(const char* path);

}