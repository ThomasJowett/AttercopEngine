#pragma once

#include "Core/core.h"
#include "Core/Colour.h"
#include "Scene/EntityReference.h"
#include "math/Quaternion.h"
#include "math/Vector2f.h"
#include "math/Vector3f.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class Asset;

namespace Reflection
{
	// In the same order as the alternatives of Value::Data
	enum class ValueType : uint8_t
	{
		Bool,
		Int,
		UInt,
		Float,
		String,
		Vec2,
		Vec3,
		Quaternion,
		Colour,
		Enum,
		Asset,
		Layer,
		Mask,
		Entity,
		List
	};

	const char* ValueTypeName(ValueType type);

	// Number of float parts a vector or colour has, 0 for other types
	int PartCount(ValueType type);

	// "X", "Y", "Z" for vectors, "R", "G", "B", "A" for colours; nullptr if out of range
	const char* PartName(ValueType type, int part);

	struct EnumType
	{
		struct Entry
		{
			std::string name;
			int32_t value;
		};

		std::string name;
		std::string description;
		std::vector<Entry> entries;

		// nullptr if no entry has this value
		const char* NameOf(int32_t value) const;
		std::optional<int32_t> ValueOf(std::string_view name) const;
	};

	struct EnumValue
	{
		const EnumType* type = nullptr;
		int32_t value = 0;

		bool operator==(const EnumValue& other) const { return type == other.type && value == other.value; }
		bool operator!=(const EnumValue& other) const { return !(*this == other); }
	};

	// An asset by its path, with the loaded asset when there is one
	struct AssetValue
	{
		std::filesystem::path path;
		Ref<Asset> asset;

		bool IsSet() const { return asset || !path.empty(); }

		// Same path, and the same asset when both hold one
		bool operator==(const AssetValue& other) const
		{
			return path == other.path && (!asset || !other.asset || asset == other.asset);
		}
		bool operator!=(const AssetValue& other) const { return !(*this == other); }
	};

	// One collision layer bit
	struct LayerValue
	{
		uint16_t bits = 0;

		bool operator==(const LayerValue& other) const { return bits == other.bits; }
		bool operator!=(const LayerValue& other) const { return bits != other.bits; }
	};

	// Any set of collision layer bits
	struct MaskValue
	{
		uint16_t bits = 0;

		bool operator==(const MaskValue& other) const { return bits == other.bits; }
		bool operator!=(const MaskValue& other) const { return bits != other.bits; }
	};

	class Value;
	using ValueList = std::vector<Value>;

	class Value
	{
	public:
		using Data = std::variant<bool, int32_t, uint32_t, float, std::string, Vector2f, Vector3f, Quaternion, Colour,
			EnumValue, AssetValue, LayerValue, MaskValue, EntityReference, ValueList>;

		Value() = default;
		Value(bool value) : m_Data(value) {}
		Value(int32_t value) : m_Data(value) {}
		Value(uint32_t value) : m_Data(value) {}
		Value(float value) : m_Data(value) {}
		Value(const char* value) : m_Data(std::string(value)) {}
		Value(std::string value) : m_Data(std::move(value)) {}
		Value(const Vector2f& value) : m_Data(value) {}
		Value(const Vector3f& value) : m_Data(value) {}
		Value(const Quaternion& value) : m_Data(value) {}
		Value(const Colour& value) : m_Data(value) {}
		Value(const EnumValue& value) : m_Data(value) {}
		Value(AssetValue value) : m_Data(std::move(value)) {}
		Value(const LayerValue& value) : m_Data(value) {}
		Value(const MaskValue& value) : m_Data(value) {}
		Value(const EntityReference& value) : m_Data(value) {}
		Value(ValueList value) : m_Data(std::move(value)) {}

		ValueType Type() const { return (ValueType)m_Data.index(); }

		template<typename T>
		bool Is() const { return std::holds_alternative<T>(m_Data); }

		// The value as T, which must be its type
		template<typename T>
		const T& As() const { return std::get<T>(m_Data); }
		template<typename T>
		T& As() { return std::get<T>(m_Data); }

		// nullptr if the value isn't a T
		template<typename T>
		const T* TryAs() const { return std::get_if<T>(&m_Data); }

		const Data& GetData() const { return m_Data; }

		// One float part of a vector or colour
		std::optional<float> GetPart(int part) const;
		bool SetPart(int part, float value);

		// Readable form for logs and test failures, not the saved form
		std::string ToString() const;

		bool operator==(const Value& other) const { return m_Data == other.m_Data; }
		bool operator!=(const Value& other) const { return !(m_Data == other.m_Data); }

	private:
		Data m_Data;
	};

	// Collision layers and masks as text: a layer's name, or "Layer N" for an unnamed slot
	std::string LayerToText(uint16_t layer);
	// An unknown name gives Default, with a warning ending in where; known is set to whether the name was found
	uint16_t LayerFromText(std::string_view text, bool* known = nullptr, std::string_view where = {});

	// "Everything", "Nothing", or layer names separated by ", "
	std::string MaskToText(uint16_t mask);
	// Unknown names are left out, with a warning ending in where; unknown lists them
	uint16_t MaskFromText(std::string_view text, std::vector<std::string>* unknown = nullptr, std::string_view where = {});

	// An unknown name gives fallback, with a warning ending in where; known is set to whether the name was found
	int32_t EnumFromText(const EnumType& type, std::string_view text, int32_t fallback, bool* known = nullptr,
		std::string_view where = {});
}
