#pragma once

#include "Common.h"
#include "Constants.h"

#include "Things/Structures/Structures.h"

#include <array>

/**
 * Contains population requirements for a given Structure.
 * 
 * Index 0: Workers.
 * Index 1: Scientists.
 * 
 * Any other index will result in exceptions being thrown.
 */
typedef std::array<int, 2> PopulationRequirements;


/** 
 * \class	StructureFactory
 * \brief	Provides a means of instantiating new structures and getting build
 *			cost / recycle value / population requirements.
 * 
 * StructureFactory is implemented as a static class and should never be
 * instantiated.
 * 
 * \note	StructureFactory::init() must be called prior to use.
 * 
 * \code{.cpp}
 * ResourcePool _rp = StructureFactory::costToBuild(SID_AGRIDOME);
 * PopulationRequirements _pr = StructureFactory::populationRequirements(SID_AGRIDOME);
 * \endcode
 */
class StructureFactory
{

public:
	static Structure* get(StructureID type);

	static const PopulationRequirements& populationRequirements(StructureID type);
	static const ResourcePool& costToBuild(StructureID type);
	static ResourcePool& recyclingValue(StructureID type);

	static bool canBuild(const ResourcePool& source, StructureID type);

	static void init();

private:
	StructureFactory() {}	// Explicitly declared private to prevent instantiation.
	~StructureFactory() {}	// Explicitly declared private to prevent instantiation.

	static void buildCostTable();
	static void buildPopulationRequirementsTable();
	static void buildRecycleValueTable();
	static ResourcePool recycleValue(StructureID type);

private:
	//static vector<ResourcePool> mStructureCostTable;
	static std::array<ResourcePool, SID_COUNT> mStructureCostTable;
	static std::array<ResourcePool, SID_COUNT> mStructureRecycleValueTable;
	static std::array<PopulationRequirements, SID_COUNT> mPopulationRequirementsTable;
};