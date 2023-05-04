#pragma once

/**
 * Templete helper functions used exclusively by RobotPool.
 * 
 * This header and its functions should not be used anywhere else,
 * these are designed specifically to help improve code readability
 * and maintainability of the RobotPool class.
 */

#include <cstddef>
#include <stdexcept>


template <class T>
void eraseRobot(T& list, Robot* robot)
{
	for (auto it = list.begin(); it != list.end(); ++it)
	{
		if (&*it == robot)
		{
			list.erase(it);
			return;
		}
	}
}


template <class T>
bool hasIdleRobot(const T& list)
{
	for (auto& robot : list)
	{
		if (robot.idle()) { return true; }
	}
	return false;
}


template <class T>
auto& getIdleRobot(T& list)
{
	for (auto& robot : list)
	{
		if (robot.idle()) { return robot; }
	}
	throw std::runtime_error("Failed to get an idle robot");
}


template <class T>
std::size_t getIdleCount(const T& list)
{
	std::size_t count = 0;
	for (const auto& robot : list)
	{
		if (robot.idle()) { ++count; }
	}

	return count;
}


template <class T>
std::size_t robotControlCount(const T& list)
{
	std::size_t controlCounter{0};
	for (const auto& robot : list)
	{
		if (!robot.idle() && !robot.isDead()) { ++controlCounter; }
	}
	return controlCounter;
}
