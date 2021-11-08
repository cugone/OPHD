#include "TileMap.h"

#include "../Constants/Numbers.h"
#include "../Constants/UiConstants.h"
#include "../DirectionOffset.h"
#include "../Mine.h"
#include "../Things/Structures/Structure.h"
#include "../RandomNumberGenerator.h"

#include <NAS2D/Timer.h>
#include <NAS2D/Utility.h>
#include <NAS2D/ParserHelper.h>
#include <NAS2D/Xml/XmlElement.h>
#include <NAS2D/Math/PointInRectangleRange.h>

#include <algorithm>
#include <functional>
#include <array>


using namespace NAS2D;


namespace {
	const std::string MapTerrainExtension = "_a.png";
	const auto MapSize = NAS2D::Vector{300, 150};

	// Relative proportion of mines with yields {low, med, high}
	const std::map<Planet::Hostility, std::array<int, 3>> HostilityMineYields =
	{
		{Planet::Hostility::Low, {30, 50, 20}},
		{Planet::Hostility::Medium, {45, 35, 20}},
		{Planet::Hostility::High, {35, 20, 45}},
	};


	std::vector<NAS2D::Point<int>> generateMineLocations(NAS2D::Vector<int> mapSize, std::size_t mineCount)
	{
		auto randPoint = [mapSize]() {
			return NAS2D::Point{
				randomNumber.generate<int>(5, mapSize.x - 5),
				randomNumber.generate<int>(5, mapSize.y - 5)
			};
		};

		std::vector<NAS2D::Point<int>> locations;
		locations.reserve(mineCount);

		// Some locations might not be acceptable, so try up to twice as many locations
		// A high density of mines could result in many rejected locations
		// Don't try indefinitely to avoid possibility of infinite loop
		std::vector<bool> usedLocations(mapSize.x * mapSize.y);
		for (std::size_t i = 0; (locations.size() < mineCount) && (i < mineCount * 2); ++i)
		{
			// Generate a location and check surroundings for minimum spacing
			const auto point = randPoint();
			if (!usedLocations[point.x + mapSize.x * point.y])
			{
				locations.push_back(point);
				for (const auto& offset : DirectionScan3x3)
				{
					const auto usedPoint = point + offset;
					usedLocations[usedPoint.x + mapSize.x * usedPoint.y] = true;
				}
			}
		}

		return locations;
	}


	void placeMines(TileMap& tileMap, Planet::Hostility hostility, const std::vector<NAS2D::Point<int>>& locations)
	{
		const auto& mineYields = HostilityMineYields.at(hostility);
		const auto total = std::accumulate(mineYields.begin(), mineYields.end(), 0);

		const auto randYield = [mineYields, total]() {
			const auto randValue = randomNumber.generate<int>(1, total);
			return (randValue <= mineYields[0]) ? MineProductionRate::Low :
				(randValue <= mineYields[0] + mineYields[1]) ? MineProductionRate::Medium :
				MineProductionRate::High;
		};

		for (const auto& location : locations)
		{
			auto& tile = tileMap.getTile({location, 0});
			tile.pushMine(new Mine(randYield()));
			tile.index(TerrainType::Dozed);
		}
	}
}


TileMap::TileMap(const std::string& mapPath, const std::string& /*tilesetPath*/, int maxDepth, int mineCount, Planet::Hostility hostility, bool shouldSetupMines) :
	mSizeInTiles{MapSize},
	mMaxDepth(maxDepth)
{
	buildTerrainMap(mapPath);

	if (shouldSetupMines)
	{
		mMineLocations = generateMineLocations(mSizeInTiles, mineCount);
		placeMines(*this, hostility, mMineLocations);
	}
}


void TileMap::removeMineLocation(const NAS2D::Point<int>& pt)
{
	auto& tile = getTile({pt, 0});
	if (!tile.hasMine())
	{
		throw std::runtime_error("No mine found to remove");
	}

	mMineLocations.erase(find(mMineLocations.begin(), mMineLocations.end(), pt));
	tile.pushMine(nullptr);
}


bool TileMap::isValidPosition(const MapCoordinate& position) const
{
	return NAS2D::Rectangle{0, 0, mSizeInTiles.x, mSizeInTiles.y}.contains(position.xy) && position.z >= 0 && position.z <= mMaxDepth;
}


const Tile& TileMap::getTile(const MapCoordinate& position) const
{
	if (!isValidPosition(position))
	{
		throw std::runtime_error("Tile coordinates out of bounds: {" + std::to_string(position.xy.x) + ", " + std::to_string(position.xy.y) + ", " + std::to_string(position.z) + "}");
	}
	const auto mapPosition = position.xy.to<std::size_t>();
	const auto level = static_cast<std::size_t>(position.z);
	return mTileMap[((level * mSizeInTiles.y) + mapPosition.y) * mSizeInTiles.x + mapPosition.x];
}


Tile& TileMap::getTile(const MapCoordinate& position)
{
	const auto& constThis = *this;
	return const_cast<Tile&>(constThis.getTile(position));
}


void TileMap::buildTerrainMap(const std::string& path)
{
	const Image heightmap(path + MapTerrainExtension);

	const auto levelCount = static_cast<std::size_t>(mMaxDepth) + 1;
	mTileMap.resize(mSizeInTiles.x * mSizeInTiles.y * levelCount);

	/**
	 * Builds a terrain map based on the pixel color values in
	 * a maps height map.
	 * 
	 * Height maps by default are in grey-scale. This method assumes
	 * that all channels are the same value so it only looks at the red.
	 * Color values are divided by 50 to get a height value from 1 - 4.
	 */
	for (int depth = 0; depth <= mMaxDepth; depth++)
	{
		for (const auto point : PointInRectangleRange{Rectangle<int>::Create({0, 0}, mSizeInTiles)})
		{
			auto color = heightmap.pixelColor(point);
			auto& tile = getTile({point, depth});
			tile = {{point, depth}, static_cast<TerrainType>(color.red / 50)};
			if (depth > 0) { tile.excavated(false); }
		}
	}
}


NAS2D::Rectangle<int> TileMap::viewArea() const
{
	return {mOriginTilePosition.xy.x, mOriginTilePosition.xy.y, mEdgeLength, mEdgeLength};
}


void TileMap::mapViewLocation(const MapCoordinate& position)
{
	mOriginTilePosition.xy = {
		std::clamp(position.xy.x, 0, mSizeInTiles.x - mEdgeLength),
		std::clamp(position.xy.y, 0, mSizeInTiles.y - mEdgeLength)
	};
	currentDepth(position.z);
}


void TileMap::centerOn(NAS2D::Point<int> point)
{
	centerOn({point, mOriginTilePosition.z});
}


void TileMap::centerOn(const MapCoordinate& position)
{
	mapViewLocation({position.xy - NAS2D::Vector{mEdgeLength, mEdgeLength} / 2, position.z});
}


void TileMap::moveView(Direction direction)
{
	mapViewLocation({
		mOriginTilePosition.xy + directionEnumToOffset(direction),
		mOriginTilePosition.z + directionEnumToVerticalOffset(direction)
	});
}


void TileMap::currentDepth(int i)
{
	mOriginTilePosition.z = std::clamp(i, 0, mMaxDepth);
}


int TileMap::viewSize() const
{
	return mEdgeLength;
}


void TileMap::viewSize(int sizeInTiles)
{
	mEdgeLength = std::max(3, sizeInTiles);
}


void TileMap::serialize(NAS2D::Xml::XmlElement* element)
{
	// ==========================================
	// VIEW PARAMETERS
	// ==========================================
	element->linkEndChild(NAS2D::dictionaryToAttributes(
		"view_parameters",
		{{
			{"currentdepth", mOriginTilePosition.z},
			{"viewlocation_x", mOriginTilePosition.xy.x},
			{"viewlocation_y", mOriginTilePosition.xy.y},
		}}
	));

	// ==========================================
	// MINES
	// ==========================================
	auto* mines = new NAS2D::Xml::XmlElement("mines");
	element->linkEndChild(mines);

	for (const auto& location : mMineLocations)
	{
		auto& mine = *getTile({location, 0}).mine();
		mines->linkEndChild(mine.serialize(location));
	}


	// ==========================================
	// TILES
	// ==========================================
	auto* tiles = new NAS2D::Xml::XmlElement("tiles");
	element->linkEndChild(tiles);

	// We're only writing out tiles that don't have structures or robots in them that are
	// underground and excavated or surface and bulldozed.
	for (int depth = 0; depth <= maxDepth(); ++depth)
	{
		for (const auto point : PointInRectangleRange{Rectangle<int>::Create({0, 0}, mSizeInTiles)})
		{
			auto& tile = getTile({point, depth});
			if (
				((depth > 0 && tile.excavated()) || (tile.index() == TerrainType::Dozed)) &&
				(tile.empty() && tile.mine() == nullptr)
			)
			{
				tiles->linkEndChild(
					NAS2D::dictionaryToAttributes(
						"tile",
						{{
							{"x", point.x},
							{"y", point.y},
							{"depth", depth},
							{"index", static_cast<int>(tile.index())},
						}}
					)
				);
			}
		}
	}
}


void TileMap::deserialize(NAS2D::Xml::XmlElement* element)
{
	// VIEW PARAMETERS
	auto* viewParameters = element->firstChildElement("view_parameters");
	const auto dictionary = NAS2D::attributesToDictionary(*viewParameters);

	const auto viewX = dictionary.get<int>("viewlocation_x");
	const auto viewY = dictionary.get<int>("viewlocation_y");
	const auto viewDepth = dictionary.get<int>("currentdepth");

	mapViewLocation({{viewX, viewY}, viewDepth});

	for (auto* mineElement = element->firstChildElement("mines")->firstChildElement("mine"); mineElement; mineElement = mineElement->nextSiblingElement())
	{
		const auto mineDictionary = NAS2D::attributesToDictionary(*mineElement);

		const auto x = mineDictionary.get<int>("x");
		const auto y = mineDictionary.get<int>("y");

		Mine* mine = new Mine();
		mine->deserialize(mineElement);

		auto& tile = getTile({{x, y}, 0});
		tile.pushMine(mine);
		tile.index(TerrainType::Dozed);

		mMineLocations.push_back(Point{x, y});
	}

	// TILES AT INDEX 0 WITH NO THINGS
	for (auto* tileElement = element->firstChildElement("tiles")->firstChildElement("tile"); tileElement; tileElement = tileElement->nextSiblingElement())
	{
		const auto tileDictionary = NAS2D::attributesToDictionary(*tileElement);

		const auto x = tileDictionary.get<int>("x");
		const auto y = tileDictionary.get<int>("y");
		const auto depth = tileDictionary.get<int>("depth");
		const auto index = tileDictionary.get<int>("index");

		auto& tile = getTile({{x, y}, depth});
		tile.index(static_cast<TerrainType>(index));

		if (depth > 0) { tile.excavated(true); }
	}
}


bool TileMap::isVisibleTile(const MapCoordinate& position) const
{
	return viewArea().contains(position.xy) && position.z == mOriginTilePosition.z;
}


/**
 * Implements MicroPather interface.
 * 
 * \warning	Assumes stateStart and stateEnd are never nullptr.
 */
float TileMap::LeastCostEstimate(void* stateStart, void* stateEnd)
{
	return sqrtf(static_cast<float>((static_cast<Tile*>(stateEnd)->xy() - static_cast<Tile*>(stateStart)->xy()).lengthSquared()));
}


void TileMap::AdjacentCost(void* state, std::vector<micropather::StateCost>* adjacent)
{
	auto& tile = *static_cast<Tile*>(state);
	const auto tilePosition = tile.xy();

	for (const auto& offset : DirectionClockwise4)
	{
		const auto position = tilePosition + offset;
		if (!NAS2D::Rectangle{0, 0, mSizeInTiles.x, mSizeInTiles.y}.contains(position))
		{
			continue;
		}

		auto& adjacentTile = getTile({position, 0});
		float cost = constants::RouteBaseCost;

		if (adjacentTile.index() == TerrainType::Impassable)
		{
			cost = FLT_MAX;
		}
		else if (!adjacentTile.empty())
		{
			if (&adjacentTile == mPathStartEndPair.first || &adjacentTile == mPathStartEndPair.second)
			{
				cost *= static_cast<float>(adjacentTile.index()) + 1.0f;
			}
			else if (adjacentTile.thingIsStructure() && adjacentTile.structure()->structureId() == StructureID::SID_ROAD)
			{
				Structure& road = *adjacentTile.structure();

				if (road.state() != StructureState::Operational)
				{
					cost *= static_cast<float>(TerrainType::Difficult) + 1.0f;
				}
				else if (road.integrity() < constants::RoadIntegrityChange)
				{
					cost = 0.75f;
				}
				else
				{
					cost = 0.5f;
				}		
			}
			else
			{
				cost = FLT_MAX;
			}
		}
		else
		{
			cost *= static_cast<float>(adjacentTile.index()) + 1.0f;
		}

		micropather::StateCost nodeCost = {&adjacentTile, cost};
		adjacent->push_back(nodeCost);
	}
}


void TileMap::pathStartAndEnd(void* start, void* end)
{
	mPathStartEndPair = std::make_pair(start, end);
}
