#pragma once

#include "core/helpers.hpp"

namespace parametric_shapes
{
	//! \brief Create a quad a given tesselation level and make it
	//!        available to OpenGL.
	//!
	//! @param width the width of the quad
	//! @param height the height of the quad
	//! @param horizontal_split_count the number of times horizontal edges
	//!                               should be split: 0 means each horizontal
	//!                               line consist of a single edge, 1 gives
	//!                               you two edges, and so on.
	//! @param vertical_split_count the number of times vertical edges
	//!                             should be split: 0 means each vertical
	//!                             line consist of a single edge, 1 gives
	//!                             you two edges, and so on.
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createQuad(float const width, float const height,
	                             unsigned int const horizontal_split_count = 0u,
	                             unsigned int const vertical_split_count = 0u);

	//! \brief Create a sphere for a given tesselation level and make it
	//!        available to OpenGL.
	//!
	//! @param radius radius of the sphere
	//! @param longitude_split_count the number of times the longitude
	//!                              angle should be split: 0 means each
	//!                              longitudinal line consist of a single
	//!                              edge spanning the full 360°, with 1
	//!                              you get two edges (each spanning
	//!                              180°); 2 is the minimum for getting a
	//!                              3-D shape.
	//! @param latitude_split_count the number of times the latitude angle
	//!                             should be split: 0 means each
	//!                             latitudinal line consist of a single
	//!                             edge spanning the full 180°, with 1 you
	//!                             get two edges (each spanning 90°); 1 is
	//!                             the minimum for getting a 3-D shape.
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createSphere(float const radius,
	                               unsigned int const longitude_split_count,
	                               unsigned int const latitude_split_count);

	//! \brief Create a torus for a given tesselation level and make it
	//!        available to OpenGL.
	//!
	//! @param major_radius radius from the centre to the middle of the
	//!                     cross-section
	//! @param minor_radius radius of the cross-section (giving the torus
	//!                     its thickness)
	//! @param major_split_count the number of times the angle for the
	//!                          major ring should be split: 0 means each
	//!                          line going around the major ring consist
	//!                          of a single edge spanning the full 360°,
	//!                          with 1 you get two edges (each spanning
	//!                          180°); 2 is the minimum for getting a 3-D
	//!                          shape.
	//! @param minor_split_count the number of times the angle for the
	//!                          minor ring should be split: 0 means each
	//!                          line going around the minor ring consist
	//!                          of a single edge spanning the full 360°,
	//!                          with 1 you get two edges (each spanning
	//!                          180°); 2 is the minimum for getting a 3-D
	//!                          shape.
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createTorus(float const radius, float const inner_radius, unsigned int const long_split_count, unsigned int const lat_split_count);

	//! \brief Create a circle ring for a given tesselation level and make it
	//!        available to OpenGL.
	//!
	//! @param radius radius from the centre to the middle of the
	//!               cross-section
	//! @param spread_length length of the cross-section
	//! @param circle_split_count the number of times the angle for the
	//!                           circle should be split: 0 means each
	//!                           line going around the circle consist
	//!                           of a single edge spanning the full 360°,
	//!                           with 1 you get two edges (each spanning
	//!                           180°); 2 is the minimum for getting a 3-D
	//!                           shape.
	//! @param spread_split_count the number of times the lines going
	//!                           out from the centre should be split: 0
	//!                           means each line going out consists of a
	//!                           single edge spanning the full spread,
	//!                           with 1 you get two edges (each spanning
	//!                           half the spread).
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createCircleRing(float const radius,
	                                   float const spread_length,
	                                   unsigned int const circle_split_count,
	                                   unsigned int const spread_split_count);


	bonobo::mesh_data createShape(unsigned int const longitude_split_count,
							  unsigned int const latitude_split_count,
							  std::function<glm::vec3(float st, float ct, float sp, float cp)> calc_vertex,
							  std::function<glm::vec3(float st, float ct, float sp, float cp)> calc_tangent,
							  std::function<glm::vec3(float st, float ct, float sp, float cp)> calc_bitangent,
							  float max_theta, float max_phi);

}
