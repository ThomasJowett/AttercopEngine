#pragma once

#include "Asset/Tileset.h"
#include "Core/Colour.h"
#include "Utilities/FileUtils.h"
#include "Utilities/SerializationUtils.h"
#include "Core/Application.h"
#include "Scene/AssetManager.h"
#include "Renderer/Mesh.h"
#include "AI/Astar.h"
#include "Physics/CollisionLayers.h"

#include <thread>
#include <mutex>
#include <vector>
#include <array>
#include "Reflection/ReflectionFwd.h"

class b2Body;

struct RebuildResult
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	Ref<Texture2D> texture;
	Colour tint;
	uint32_t rebuildId = 0;
};

struct RebuildState
{
	std::mutex mutex;
	RebuildResult result;
	bool hasResult = false;
	uint32_t currentRebuildId = 0;
};

struct TilemapComponent
{
	enum class Orientation
	{
		orthogonal,
		isometric,
		staggered,
		hexagonal
	};

	Ref<Tileset> tileset;
	Colour tint{ 1.0f, 1.0f,1.0f,1.0f };

	std::vector<std::vector<uint32_t>> tiles;
	uint32_t tilesWide = 0;
	uint32_t tilesHigh = 0;

	uint32_t tileWidth = 32;
	uint32_t tileHeight = 32;

	Orientation orientation = Orientation::orthogonal;

	bool isTrigger = false;
	uint16_t layer = CollisionLayers::DefaultLayer;
	uint16_t mask = CollisionLayers::Everything;

	Ref<Mesh> mesh;

	b2Body* runtimeBody = nullptr;

	Ref<RebuildState> rebuildState;

	TilemapComponent() = default;
	TilemapComponent(const TilemapComponent& other)
		: tileset(other.tileset), tint(other.tint), tiles(other.tiles),
		  tilesWide(other.tilesWide), tilesHigh(other.tilesHigh),
		  tileWidth(other.tileWidth), tileHeight(other.tileHeight),
		  orientation(other.orientation), isTrigger(other.isTrigger), layer(other.layer), mask(other.mask),
		  mesh(other.mesh), runtimeBody(nullptr), rebuildState(nullptr), m_PathfindingGrid(nullptr)
	{
	}

	TilemapComponent& operator=(const TilemapComponent& other)
	{
		if (this == &other)
			return *this;

		tileset = other.tileset;
		tint = other.tint;
		tiles = other.tiles;
		tilesWide = other.tilesWide;
		tilesHigh = other.tilesHigh;
		tileWidth = other.tileWidth;
		tileHeight = other.tileHeight;
		orientation = other.orientation;
		isTrigger = other.isTrigger;
		layer = other.layer;
		mask = other.mask;
		mesh = other.mesh;
		runtimeBody = nullptr;
		rebuildState = nullptr;
		m_PathfindingGrid = nullptr;

		return *this;
	}

	// A move keeps rebuildState, which a copy nulls, so a moved tilemap still picks up its background rebuild
	TilemapComponent(TilemapComponent&& other) noexcept
		: tileset(std::move(other.tileset)), tint(other.tint), tiles(std::move(other.tiles)),
		  tilesWide(other.tilesWide), tilesHigh(other.tilesHigh),
		  tileWidth(other.tileWidth), tileHeight(other.tileHeight),
		  orientation(other.orientation), isTrigger(other.isTrigger), layer(other.layer), mask(other.mask),
		  mesh(std::move(other.mesh)), runtimeBody(other.runtimeBody), rebuildState(std::move(other.rebuildState)),
		  m_PathfindingGrid(std::move(other.m_PathfindingGrid))
	{
		other.runtimeBody = nullptr;
	}

	TilemapComponent& operator=(TilemapComponent&& other) noexcept
	{
		if (this == &other)
			return *this;

		tileset = std::move(other.tileset);
		tint = other.tint;
		tiles = std::move(other.tiles);
		tilesWide = other.tilesWide;
		tilesHigh = other.tilesHigh;
		tileWidth = other.tileWidth;
		tileHeight = other.tileHeight;
		orientation = other.orientation;
		isTrigger = other.isTrigger;
		layer = other.layer;
		mask = other.mask;
		mesh = std::move(other.mesh);
		runtimeBody = other.runtimeBody;
		rebuildState = std::move(other.rebuildState);
		m_PathfindingGrid = std::move(other.m_PathfindingGrid);
		other.runtimeBody = nullptr;

		return *this;
	}

	TilemapComponent(Orientation orientation, uint32_t tilesWide, uint32_t tilesHigh)
		:orientation(orientation), tilesWide(tilesWide), tilesHigh(tilesHigh),
		tiles(tilesHigh)
	{
		for (auto& row : tiles)
		{
			row.resize(tilesWide);
		}
	}

	void Rebuild();
	void UpdateRebuild();

	// Built on first use from the tileset's collision shapes, with regions labelled; cleared whenever the tiles change
	const Astar::AstarGrid& GetPathfindingGrid();

	// Tile containing a point in the tilemap's local space; false if off the map or the orientation isn't supported
	bool LocalToCell(Vector2f local, Astar::GridCoord& cell) const;

	// Centre of a tile's ground (its diamond or hex, not the whole sprite) in local space
	Vector2f CellToLocal(Astar::GridCoord cell) const;

	bool SupportsPathfinding() const { return orientation != Orientation::staggered; }
	void InvalidatePathfindingGrid() { m_PathfindingGrid.reset(); }

	static void Describe(Reflection::Builder<TilemapComponent>& c);

	Vector2f IsoToWorld(uint32_t x, uint32_t y) const;
	Vector2f WorldToIso(Vector2f v) const;

	// Hexes are flat-top and 1 unit wide, odd columns shifted down half a hex; Tile Size sets the proportions
	Vector2f GetHexSize() const;

	// Size a hex tile's sprite is drawn at, bottom-aligned to its hex
	Vector2f GetHexSpriteSize() const;
	Vector2f HexToWorld(uint32_t q, uint32_t r) const;

	// Column and row of the hex containing a local-space point
	Vector2f WorldToHex(Vector2f v) const;

private:
	Ref<Astar::AstarGrid> m_PathfindingGrid;
};
