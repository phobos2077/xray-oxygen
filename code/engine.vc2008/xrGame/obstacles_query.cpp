////////////////////////////////////////////////////////////////////////////
//	Module 		: obstacles_query.cpp
//	Created 	: 10.04.2007
//  Modified 	: 10.04.2007
//	Author		: Dmitriy Iassenev
//	Description : obstacles query
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "obstacles_query.h"
#include "GameObject.h"
#include "ai_obstacle.h"
#include "ai_space.h"
#include "level_graph.h"
#include <algorithm> 

#pragma warning(push)
#pragma warning(disable:4995)
#include <malloc.h>
#pragma warning(pop)

void obstacles_query::set_intersection	(const obstacles_query &query)
{
	u32							n = m_obstacles.size();
	std::size_t					buffer_size = n*sizeof(OBSTACLES::value_type);
	OBSTACLES::value_type		*temp = (OBSTACLES::value_type*)_alloca(buffer_size);
    std::memcpy(temp,&*obstacles().begin(),buffer_size);
	m_obstacles.erase			(
		std::set_intersection(
			temp,
			temp + n,
			query.obstacles().begin(),
			query.obstacles().end(),
			m_obstacles.begin()
		),
		m_obstacles.end()
	);

	if (obstacles().size() == n)
		return;

	m_actual					= false;
}

void obstacles_query::merge				(const AREA &object_area)
{
	AREA temp(std::move(m_area));
	u32 area_size = temp.size();
	u32 destination_size = area_size + object_area.size();
	m_area.resize(destination_size);
	m_area.erase(std::set_union(temp.begin(), temp.end(), object_area.begin(), object_area.end(), m_area.begin()), m_area.end());
}

void obstacles_query::compute_area		()
{
	m_actual = true;
	m_area.clear();
	m_crc = 0;
	
	for (auto it : m_obstacles)
	{
		ai_obstacle				&obstacle = it.first->obstacle();
		merge					(obstacle.area());
		it.second				= obstacle.crc();
		m_crc					^= it.second;
	}
}

void obstacles_query::merge				(const obstacles_query &query)
{
	for (auto it : query.obstacles())
	{
		add(it.first);
	}
}

bool obstacles_query::merge				(const Fvector &position, const float &radius, const obstacles_query &query)
{
	merge						(query);

	if (actual()) 
	{
		if (!objects_changed(position, radius))
			return				(false);

		update_objects			(position, radius);
		return					(true);
	}

	u32							crc_before = crc();
	compute_area				();
	return						(crc() != crc_before);
}

bool obstacles_query::objects_changed	(const Fvector &position, const float &radius) const
{
	for (auto it : obstacles())
	{
		if (it.first->obstacle().crc() == it.second)
			continue;

		if (it.first->obstacle().distance_to(position) >= radius)
			continue;

		return true;
	}
	return false;
}

struct too_far_predicate 
{
	Fvector	m_position;
	float	m_radius_sqr;

	IC too_far_predicate(const Fvector &position, const float &radius)
	{
		m_position				= position;
		m_radius_sqr			= _sqr(radius);
	}

	IC bool operator()(const std::pair<const CGameObject*,u32> &object) const
	{
		typedef obstacles_query::AREA	AREA;
		const AREA				&area = object.first->obstacle().area();
		for (auto it : area) 
		{
			Fvector	vertex_position = ai().level_graph().vertex_position(it);
			const float distance_sqr = vertex_position.distance_to_sqr(m_position);
			if (distance_sqr < m_radius_sqr)
				return false;
		}
		return true;
	}
};

bool obstacles_query::remove_objects(const Fvector &position, const float &radius)
{
	update_objects				(position, radius);

	OBSTACLES::iterator I = std::remove_if(m_obstacles.begin(), m_obstacles.end(), too_far_predicate(position,radius));

	if (I == m_obstacles.end())
		return					(false);

	m_obstacles.erase			(I,m_obstacles.end());

	m_actual					= false;
	u32							crc_before = crc();
	compute_area				();
	return						(crc_before != crc());
}

void obstacles_query::remove_links		(CObject *object)
{
	OBSTACLES::iterator	I = m_obstacles.find(smart_cast<CGameObject*>(object));
	if (I == m_obstacles.end())
		return;

	m_obstacles.erase			(I);
}