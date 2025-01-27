/*
 * LeanHRPT Decode
 * Copyright (C) 2021 Xerbo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MAP_H
#define MAP_H

#include <vector>
#include <array>
#include <string>
#include <QLineF>
#include "projection.h"

namespace map {
    // Checks that a Shapefile is readable and supported (Polyline/Polygon)
    bool verify_shapefile(std::string filename);

    // Decompose a Polyline/Polygon Shapefile into a list of line segments
    std::vector<QLineF> read_shapefile(std::string filename);

    // Sort line segments into 10x10 "buckets"
    std::array<std::vector<QLineF>, 36*18> index_line_segments(const std::vector<QLineF> &line_segments);

    // Warp an (indexed) map to fit a pass based off a point grid
    std::vector<QLineF> warp_to_pass(const std::array<std::vector<QLineF>, 36*18> &buckets, const std::vector<std::pair<xy, Geodetic>> &points, size_t xn);
}

#endif
