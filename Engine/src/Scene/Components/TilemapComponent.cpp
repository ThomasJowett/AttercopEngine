#include "TilemapComponent.h"

#include "Reflection/Describe.h"
#include "Physics/CollisionLayers.h"
#include "Scripting/Lua/LuaBindings.h"
#include "TinyXml2/tinyxml2.h"
#include "Utilities/GeometryGenerator.h"

Vector2f TilemapComponent::IsoToWorld(uint32_t x, uint32_t y) const
{
	return Vector2f((float)((int)x - (int)y) / 2.0f, -(float)(x + y) / 4.0f);
}

Vector2f TilemapComponent::WorldToIso(Vector2f v) const
{
	return Vector2f((v.x - v.y * 2.0f), -(v.x + v.y * 2.0f));
}

namespace
{
	// Hexes are 1 unit wide; Tile Size only sets the height ratio
	float HexHeight(uint32_t tileWidth, uint32_t tileHeight)
	{
		return tileWidth > 0 ? (float)tileHeight / (float)tileWidth : 1.0f;
	}

	// Flat-top, odd columns shifted down half a hex
	Vector2f HexCentre(int q, int r, float hexHeight)
	{
		float y = hexHeight * ((float)r + ((q & 1) ? 0.5f : 0.0f));
		return Vector2f(0.75f * (float)q, -y);
	}
}

Vector2f TilemapComponent::GetHexSize() const
{
	return Vector2f(1.0f, HexHeight(tileWidth, tileHeight));
}

Vector2f TilemapComponent::GetHexSpriteSize() const
{
	if (!tileset || !tileset->GetSubTexture() || tileWidth == 0)
		return GetHexSize();

	float pixelsToUnits = 1.0f / (float)tileWidth;
	return Vector2f(tileset->GetSubTexture()->GetSpriteWidth() * pixelsToUnits, tileset->GetSubTexture()->GetSpriteHeight() * pixelsToUnits);
}

Vector2f TilemapComponent::HexToWorld(uint32_t q, uint32_t r) const
{
	return HexCentre((int)q, (int)r, GetHexSize().y);
}

Vector2f TilemapComponent::WorldToHex(Vector2f v) const
{
	// Stretch to a regular hex (circumradius 0.5), rows running down, then cube-round the axial coordinates
	float x = v.x;
	float y = -v.y * (0.8660254f / GetHexSize().y);

	float q = x / 0.75f;
	float r = (-x / 3.0f + 0.57735027f * y) / 0.5f;
	float s = -q - r;

	float roundedQ = std::round(q);
	float roundedR = std::round(r);
	float roundedS = std::round(s);

	float dq = std::abs(roundedQ - q);
	float dr = std::abs(roundedR - r);
	float ds = std::abs(roundedS - s);

	if (dq > dr && dq > ds)
		roundedQ = -roundedR - roundedS;
	else if (dr > ds)
		roundedR = -roundedQ - roundedS;

	// Axial to odd-column offset
	int column = (int)roundedQ;
	int row = (int)roundedR + (column - (column & 1)) / 2;
	return Vector2f((float)column, (float)row);
}

/* ------------------------------------------------------------------------------------------------------------------ */

void TilemapComponent::Rebuild()
{
	InvalidatePathfindingGrid();

	if (!tileset || !tileset->GetSubTexture())
	{
		mesh.reset();
		return;
	}

	if (!rebuildState)
	{
		rebuildState = CreateRef<RebuildState>();
	}

	// currentRebuildId is also read (under the same lock) by an in-flight background thread's
	// own supersession check below - incrementing it here without the lock would be a data
	// race against that read.
	uint32_t rid;
	{
		std::lock_guard<std::mutex> lock(rebuildState->mutex);
		rid = ++rebuildState->currentRebuildId;
	}

	std::vector<std::vector<uint32_t>> tilesCopy = tiles;
	uint32_t tw = tilesWide;
	uint32_t th = tilesHigh;
	uint32_t tWidth = tileWidth;
	uint32_t tHeight = tileHeight;
	Orientation orient = orientation;
	Colour col = tint;

	size_t maxTileIndex = tileset->GetNumberOfTiles();
	std::vector<std::array<Vector2f, 4>> tileCoords(maxTileIndex);
	for (size_t i = 0; i < maxTileIndex; i++)
	{
		tileset->SetCurrentTile(static_cast<uint32_t>(i));
		const Vector2f* texCoords = tileset->GetSubTexture()->GetTextureCoordinates();
		for (size_t v = 0; v < 4; v++)
		{
			tileCoords[i][v] = texCoords[v];
		}
	}

	uint32_t spriteWidth = tileset->GetSubTexture()->GetSpriteWidth();
	uint32_t spriteHeight = tileset->GetSubTexture()->GetSpriteHeight();
	Ref<Texture2D> tilesetTex = tileset->GetSubTexture()->GetTexture();

	std::weak_ptr<RebuildState> weakState = rebuildState;

	auto rebuildJob = [
		weakState, rid, tilesCopy, tw, th, tWidth, tHeight, orient, col,
		maxTileIndex, tileCoords, spriteWidth, spriteHeight, tilesetTex
	]() {
		std::vector<Vertex> verticesList;
		std::vector<uint32_t> indicesList;

		if (orient == Orientation::orthogonal)
		{
			// 0,0________ X
			//   |_|_|_|_|
			//   |_|_|_|_|
			//   |_|_|_|_|
			//   |_|_|_|_|
			//  Y

			Vector2f positions[4] = {
					{ 0.0f, 1.0f },
					{ 1.0f, 1.0f },
					{ 1.0f, 0.0f },
					{ 0.0f, 0.0f }
			};

			for (size_t i = 0; i < th; i++)
			{
				for (size_t j = 0; j < tw; j++)
				{
					if (i >= tilesCopy.size() || j >= tilesCopy[i].size() || tilesCopy[i][j] == 0)
						continue;

					if (tilesCopy[i][j] > maxTileIndex) {
						continue;
					}

					const auto& texCoords = tileCoords[tilesCopy[i][j] - 1];

					for (size_t v = 0; v < 4; v++)
					{
						Vertex vertex;
						vertex.position = Vector3f((float)(j)+positions[v].x, -(float)(i)-positions[v].y, 0.0f);
						vertex.normal.z = 1.0f;
						vertex.tangent.x = 1.0f;
						vertex.texcoord = Vector2f(texCoords[v].x, texCoords[v].y);
						verticesList.push_back(vertex);
					}
				}
			}
		}
		else if (orient == Orientation::isometric)
		{
			//   0,0
			//    /\
			//   /\/\
			// Y/\/\/\ X
			//  \/\/\/
			//   \/\/
			//    \/

			Vector2f positions[4] = {
					{ 0.0f, 0.0f },
					{ 1.0f, 0.0f },
					{ 1.0f, 1.0f },
					{ 0.0f, 1.0f }
			};

			auto isoToWorld = [](uint32_t x, uint32_t y) -> Vector2f {
				return Vector2f((float)((int)x - (int)y) / 2.0f, -(float)(x + y) / 4.0f);
			};

			for (uint32_t i = 0; i < th; i++)
			{
				for (uint32_t j = 0; j < tw; j++)
				{
					if (i >= tilesCopy.size() || j >= tilesCopy[i].size() || tilesCopy[i][j] == 0)
						continue;

					if (tilesCopy[i][j] > maxTileIndex) {
						continue;
					}

					const auto& texCoords = tileCoords[tilesCopy[i][j] - 1];

					Vector2f isoCoords = isoToWorld(j, i);

					for (uint32_t v = 0; v < 4; v++)
					{
						Vertex vertex;

						vertex.position.x = isoCoords.x + positions[v].x - 0.5f;
						vertex.position.y = isoCoords.y + positions[v].y - 0.5f;
						vertex.position.z = (i + j) * 0.0001f;

						vertex.normal.z = 1.0f;
						vertex.tangent.x = 1.0f;

						vertex.texcoord = Vector2f(texCoords[v].x, texCoords[v].y);

						verticesList.push_back(vertex);
					}
				}
			}
		}
		else if (orient == Orientation::hexagonal)
		{
			Vector2f positions[4] = {
					{ 0.0f, 0.0f },
					{ 1.0f, 0.0f },
					{ 1.0f, 1.0f },
					{ 0.0f, 1.0f }
			};

			// Tile Size is the hex footprint in pixels, scaled to 1 unit wide; the sprite keeps its proportions, bottom-aligned to the hex
			float hexHeight = HexHeight(tWidth, tHeight);
			float pixelsToUnits = tWidth > 0 ? 1.0f / (float)tWidth : 1.0f;
			float quadWidth = (float)spriteWidth * pixelsToUnits;
			float quadHeight = (float)spriteHeight * pixelsToUnits;

			for (uint32_t r = 0; r < th; r++)
			{
				for (uint32_t q = 0; q < tw; q++)
				{
					if (r >= tilesCopy.size() || q >= tilesCopy[r].size() || tilesCopy[r][q] == 0)
						continue;

					if (tilesCopy[r][q] > maxTileIndex) {
						continue;
					}

					const auto& texCoords = tileCoords[tilesCopy[r][q] - 1];

					Vector2f center = HexCentre((int)q, (int)r, hexHeight);

					for (size_t v = 0; v < 4; v++)
					{
						Vertex vertex;
						vertex.position.x = center.x + (positions[v].x - 0.5f) * quadWidth;
						vertex.position.y = center.y - hexHeight * 0.5f + positions[v].y * quadHeight;
						vertex.position.z = r * 0.0001f;

						vertex.normal.z = 1.0f;
						vertex.tangent.x = 1.0f;
						vertex.texcoord = Vector2f(texCoords[v].x, texCoords[v].y);
						verticesList.push_back(vertex);
					}
				}
			}
		}

		// One quad (4 vertices) per non-empty tile actually pushed above - looping over the tile
		// grid again here (instead of verticesList.size()) would drift out of sync whenever any
		// tile was skipped (empty, or an out-of-range index reset to 0), producing indices past
		// the end of the vertex buffer.
		indicesList.reserve((verticesList.size() / 4) * 6);
		for (uint32_t index = 0; index < (uint32_t)verticesList.size(); index += 4)
		{
			indicesList.push_back(index);
			indicesList.push_back(index + 1);
			indicesList.push_back(index + 2);

			indicesList.push_back(index);
			indicesList.push_back(index + 2);
			indicesList.push_back(index + 3);
		}

		if (auto state = weakState.lock())
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			if (rid >= state->currentRebuildId)
			{
				state->result.vertices = std::move(verticesList);
				state->result.indices = std::move(indicesList);
				state->result.texture = tilesetTex;
				state->result.tint = col;
				state->result.rebuildId = rid;
				state->hasResult = true;
			}
		}
	};

#ifdef __EMSCRIPTEN__
	// No threads in the web build; the result is still picked up by UpdateRebuild
	rebuildJob();
#else
	std::thread(std::move(rebuildJob)).detach();
#endif
}

void TilemapComponent::UpdateRebuild()
{
	if (!rebuildState)
		return;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	Ref<Texture2D> texture;
	Colour tintColor;
	bool hasNewResult = false;

	{
		std::lock_guard<std::mutex> lock(rebuildState->mutex);
		if (rebuildState->hasResult)
		{
			vertices = std::move(rebuildState->result.vertices);
			indices = std::move(rebuildState->result.indices);
			texture = std::move(rebuildState->result.texture);
			tintColor = rebuildState->result.tint;
			rebuildState->hasResult = false;
			hasNewResult = true;
		}
	}

	if (hasNewResult)
	{
		if (vertices.empty())
		{
			mesh.reset();
			return;
		}

		Ref<Material> material = CreateRef<Material>("Standard", tintColor);
		material->AddTexture(texture, 0);
		material->SetTwoSided(true);
		material->SetTransparency(true);

		mesh = CreateRef<Mesh>(GeometryGenerator::FlattenVertices(vertices), indices, material, s_StaticMeshLayout);
	}
}

const Astar::AstarGrid& TilemapComponent::GetPathfindingGrid()
{
	if (m_PathfindingGrid)
		return *m_PathfindingGrid;

	m_PathfindingGrid = CreateRef<Astar::AstarGrid>((int)tilesWide, (int)tilesHigh);
	if (orientation == Orientation::hexagonal)
		m_PathfindingGrid->topology = Astar::Topology::Hex;

	if (tileset && tileset->HasCollision() && !isTrigger)
	{
		for (uint32_t row = 0; row < tilesHigh && row < tiles.size(); row++)
		{
			for (uint32_t column = 0; column < tilesWide && column < tiles[row].size(); column++)
			{
				uint32_t index = tiles[row][column];
				if (index == 0 || index - 1 >= tileset->GetNumberOfTiles())
					continue;

				if (tileset->GetTile(index - 1).GetCollisionShape() != Tile::CollisionShape::None)
					m_PathfindingGrid->SetBlocked({ (int)column, (int)row }, true);
			}
		}
	}

	m_PathfindingGrid->LabelRegions();
	return *m_PathfindingGrid;
}

bool TilemapComponent::LocalToCell(Vector2f local, Astar::GridCoord& cell) const
{
	Vector2f coords;
	switch (orientation)
	{
	case Orientation::orthogonal:
		coords = Vector2f(local.x, -local.y);
		break;
	case Orientation::isometric:
		coords = WorldToIso(local);
		break;
	case Orientation::hexagonal:
		coords = WorldToHex(local);
		break;
	default:
		return false;
	}

	cell = { (int)std::floor(coords.x), (int)std::floor(coords.y) };
	return cell.x >= 0 && cell.y >= 0 && cell.x < (int)tilesWide && cell.y < (int)tilesHigh;
}

Vector2f TilemapComponent::CellToLocal(Astar::GridCoord cell) const
{
	if (orientation == Orientation::hexagonal)
		return HexCentre(cell.x, cell.y, GetHexSize().y);

	float x = (float)cell.x + 0.5f;
	float y = (float)cell.y + 0.5f;

	// Inverse of WorldToIso, taken at the middle of the tile's diamond
	if (orientation == Orientation::isometric)
		return Vector2f((x - y) / 2.0f, -(x + y) / 4.0f);

	return Vector2f(x, -y);
}

namespace TilemapComponentReflection
{
	// Rows of comma-separated tile indices, as the element's text
	void SaveTiles(const TilemapComponent& c, tinyxml2::XMLElement* element)
	{
		std::string csv = "\n";
		for (size_t y = 0; y < c.tiles.size(); ++y)
		{
			for (size_t x = 0; x < c.tiles[y].size(); ++x)
			{
				csv += std::to_string(c.tiles[y][x]);
				if (x + 1 < c.tiles[y].size() || y + 1 < c.tiles.size())
					csv += ',';
			}
			csv += '\n';
		}
		element->SetText(csv.c_str());
	}

	// Sized by TilesWide and TilesHigh, which are restored first; missing tiles are 0
	void LoadTiles(TilemapComponent& c, const tinyxml2::XMLElement* element)
	{
		c.tiles.assign(c.tilesHigh, std::vector<uint32_t>(c.tilesWide, 0));
		const char* text = element->GetText();
		if (!text)
			return;

		std::vector<std::string> values = SplitString(text, ',');
		for (size_t i = 0; i < values.size() && i < (size_t)c.tilesWide * c.tilesHigh; ++i)
			c.tiles[i / c.tilesWide][i % c.tilesWide] = (uint32_t)std::strtoul(values[i].c_str(), nullptr, 10);
	}

	// Keeps the tile grid the size TilesWide and TilesHigh say
	void ResizeTiles(Entity, TilemapComponent& c)
	{
		c.tiles.resize(c.tilesHigh);
		for (std::vector<uint32_t>& row : c.tiles)
			row.resize(c.tilesWide);
	}

	void RebuildMesh(Entity, TilemapComponent& c) { c.Rebuild(); }
}

namespace TilemapComponentReflection
{
	REFLECT_LUA_BEGIN(TilemapComponent)
		REFLECT_LUA_COLLISION_FILTER()
		REFLECT_LUA_FUNCTION(Rebuild, "Regenerate the tilemap's mesh from the current tile grid (runs in the background; call UpdateRebuild to pick up the result)")
		REFLECT_LUA_FUNCTION_CUSTOM("SetTile", "Set the tile index at the given column/row, if within bounds", [](Self& c, uint32_t x, uint32_t y, uint32_t tileId) {
			if (y < c.tiles.size() && x < c.tiles[y].size()) {
				c.tiles[y][x] = tileId;
				c.InvalidatePathfindingGrid();
			}
		});
		REFLECT_LUA_FUNCTION_CUSTOM("GetTile", "Get the tile index at the given column/row, or 0 if out of bounds", [](Self& c, uint32_t x, uint32_t y) -> uint32_t {
			if (y < c.tiles.size() && x < c.tiles[y].size()) {
				return c.tiles[y][x];
			}
			return 0;
		});
		REFLECT_LUA_FUNCTION_CUSTOM("Resize", "Resize the tile grid to the given width/height in tiles, preserving existing tile data", [](Self& c, uint32_t width, uint32_t height) {
			c.tilesWide = width;
			c.tilesHigh = height;
			c.tiles.resize(height);
			for (auto& row : c.tiles) {
				row.resize(width);
			}
			c.InvalidatePathfindingGrid();
		});
	REFLECT_LUA_END()
}

void TilemapComponent::Describe(Reflection::Builder<TilemapComponent>& c)
{
	c.LuaFunctions(&TilemapComponentReflection::RegisterLuaFunctions);
	using namespace TilemapComponentReflection;

	Reflection::Enum<Orientation>("TilemapOrientation", "Tile layouts, for TilemapComponent.Orientation", {
		{ "Orthogonal", Orientation::orthogonal },
		{ "Isometric", Orientation::isometric },
		{ "Staggered", Orientation::staggered },
		{ "Hexagonal", Orientation::hexagonal },
	});

	c.Name("Tilemap").Category("Rendering 2D")
		.Description("A grid of tiles from a tileset, with collision from the tileset's shapes");

	c.Field<&TilemapComponent::tileset>("Tileset", "The tileset this map draws its tiles from").MainAsset();
	c.Field<&TilemapComponent::tilesWide>("TilesWide", "Number of tile columns").Label("Width").Range(0.0f, 1000.0f).Speed(1.0f).EditFromStart().OnChanged(&ResizeTiles);
	c.Field<&TilemapComponent::tilesHigh>("TilesHigh", "Number of tile rows").Label("Height").Range(0.0f, 1000.0f).Speed(1.0f).EditFromStart().OnChanged(&ResizeTiles);
	c.Field<&TilemapComponent::tileWidth>("TileWidth", "Width of a single tile, in pixels");
	c.Field<&TilemapComponent::tileHeight>("TileHeight", "Height of a single tile, in pixels");
	c.Field<&TilemapComponent::tint>("Tint", "Colour multiplied over every tile");
	c.Field<&TilemapComponent::orientation>("Orientation", "How tiles are laid out: orthogonal, isometric, staggered or hexagonal");
	c.Field<&TilemapComponent::isTrigger>("IsTrigger", "Whether this tilemap's collision only reports overlaps instead of physically blocking");
	c.Layer<&TilemapComponent::layer>("Layer", "The collision layer this tilemap is on");
	c.Mask<&TilemapComponent::mask>("Mask", "The collision layers this tilemap collides with");
	c.Custom<&SaveTiles, &LoadTiles>("Tiles", "The tile index of each cell, row by row");

	c.OnChanged(&RebuildMesh);
}
