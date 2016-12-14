#include "util/coordinate.hpp"
#include "util/coordinate_calculation.hpp"
#include "util/trigonometry_table.hpp"
#include "util/web_mercator.hpp"

#include <boost/assert.hpp>

#include <cmath>

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>

#include <iomanip>
#include <sstream>

namespace osrm
{
namespace util
{

namespace coordinate_calculation
{

// Does not project the coordinates!
std::uint64_t squaredEuclideanDistance(const Coordinate lhs, const Coordinate rhs)
{
    const std::uint64_t dx = static_cast<std::int32_t>(lhs.lon - rhs.lon);
    const std::uint64_t dy = static_cast<std::int32_t>(lhs.lat - rhs.lat);

    return dx * dx + dy * dy;
}

double haversineDistance(const Coordinate coordinate_1, const Coordinate coordinate_2)
{
    auto lon1 = static_cast<int>(coordinate_1.lon);
    auto lat1 = static_cast<int>(coordinate_1.lat);
    auto lon2 = static_cast<int>(coordinate_2.lon);
    auto lat2 = static_cast<int>(coordinate_2.lat);
    BOOST_ASSERT(lon1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon2 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat2 != std::numeric_limits<int>::min());
    const double lt1 = lat1 / COORDINATE_PRECISION;
    const double ln1 = lon1 / COORDINATE_PRECISION;
    const double lt2 = lat2 / COORDINATE_PRECISION;
    const double ln2 = lon2 / COORDINATE_PRECISION;

    const double dlat1 = lt1 * detail::DEGREE_TO_RAD;
    const double dlong1 = ln1 * detail::DEGREE_TO_RAD;
    const double dlat2 = lt2 * detail::DEGREE_TO_RAD;
    const double dlong2 = ln2 * detail::DEGREE_TO_RAD;

    const double dlong = dlong1 - dlong2;
    const double dlat = dlat1 - dlat2;

    const double aharv = std::pow(std::sin(dlat / 2.0), 2.0) +
                         std::cos(dlat1) * std::cos(dlat2) * std::pow(std::sin(dlong / 2.), 2);
    const double charv = 2. * std::atan2(std::sqrt(aharv), std::sqrt(1.0 - aharv));
    return detail::EARTH_RADIUS * charv;
}

double greatCircleDistance(const Coordinate coordinate_1, const Coordinate coordinate_2)
{
    auto lon1 = static_cast<int>(coordinate_1.lon);
    auto lat1 = static_cast<int>(coordinate_1.lat);
    auto lon2 = static_cast<int>(coordinate_2.lon);
    auto lat2 = static_cast<int>(coordinate_2.lat);
    BOOST_ASSERT(lat1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat2 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon2 != std::numeric_limits<int>::min());

    const double float_lat1 = (lat1 / COORDINATE_PRECISION) * detail::DEGREE_TO_RAD;
    const double float_lon1 = (lon1 / COORDINATE_PRECISION) * detail::DEGREE_TO_RAD;
    const double float_lat2 = (lat2 / COORDINATE_PRECISION) * detail::DEGREE_TO_RAD;
    const double float_lon2 = (lon2 / COORDINATE_PRECISION) * detail::DEGREE_TO_RAD;

    const double x_value = (float_lon2 - float_lon1) * std::cos((float_lat1 + float_lat2) / 2.0);
    const double y_value = float_lat2 - float_lat1;
    return std::hypot(x_value, y_value) * detail::EARTH_RADIUS;
}

double perpendicularDistance(const Coordinate segment_source,
                             const Coordinate segment_target,
                             const Coordinate query_location,
                             Coordinate &nearest_location,
                             double &ratio)
{
    using namespace coordinate_calculation;

    BOOST_ASSERT(query_location.IsValid());

    FloatCoordinate projected_nearest;
    std::tie(ratio, projected_nearest) =
        projectPointOnSegment(web_mercator::fromWGS84(segment_source),
                              web_mercator::fromWGS84(segment_target),
                              web_mercator::fromWGS84(query_location));
    nearest_location = web_mercator::toWGS84(projected_nearest);

    const double approximate_distance = greatCircleDistance(query_location, nearest_location);
    BOOST_ASSERT(0.0 <= approximate_distance);
    return approximate_distance;
}

double perpendicularDistance(const Coordinate source_coordinate,
                             const Coordinate target_coordinate,
                             const Coordinate query_location)
{
    double ratio;
    Coordinate nearest_location;

    return perpendicularDistance(
        source_coordinate, target_coordinate, query_location, nearest_location, ratio);
}

Coordinate centroid(const Coordinate lhs, const Coordinate rhs)
{
    Coordinate centroid;
    // The coordinates of the midpoints are given by:
    // x = (x1 + x2) /2 and y = (y1 + y2) /2.
    centroid.lon = (lhs.lon + rhs.lon) / FixedLongitude{2};
    centroid.lat = (lhs.lat + rhs.lat) / FixedLatitude{2};
    return centroid;
}

double degToRad(const double degree)
{
    using namespace boost::math::constants;
    return degree * (pi<double>() / 180.0);
}

double radToDeg(const double radian)
{
    using namespace boost::math::constants;
    return radian * (180.0 * (1. / pi<double>()));
}

double bearing(const Coordinate first_coordinate, const Coordinate second_coordinate)
{
    const double lon_diff =
        static_cast<double>(toFloating(second_coordinate.lon - first_coordinate.lon));
    const double lon_delta = degToRad(lon_diff);
    const double lat1 = degToRad(static_cast<double>(toFloating(first_coordinate.lat)));
    const double lat2 = degToRad(static_cast<double>(toFloating(second_coordinate.lat)));
    const double y = std::sin(lon_delta) * std::cos(lat2);
    const double x =
        std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(lon_delta);
    double result = radToDeg(std::atan2(y, x));
    while (result < 0.0)
    {
        result += 360.0;
    }

    while (result >= 360.0)
    {
        result -= 360.0;
    }
    return result;
}

double computeAngle(const Coordinate first, const Coordinate second, const Coordinate third)
{
    using namespace boost::math::constants;
    using namespace coordinate_calculation;

    if (first == second || second == third)
        return 180;

    BOOST_ASSERT(first.IsValid());
    BOOST_ASSERT(second.IsValid());
    BOOST_ASSERT(third.IsValid());

    const double v1x = static_cast<double>(toFloating(first.lon - second.lon));
    const double v1y =
        web_mercator::latToY(toFloating(first.lat)) - web_mercator::latToY(toFloating(second.lat));
    const double v2x = static_cast<double>(toFloating(third.lon - second.lon));
    const double v2y =
        web_mercator::latToY(toFloating(third.lat)) - web_mercator::latToY(toFloating(second.lat));

    double angle = (atan2_lookup(v2y, v2x) - atan2_lookup(v1y, v1x)) * 180. / pi<double>();

    while (angle < 0.)
    {
        angle += 360.;
    }

    BOOST_ASSERT(angle >= 0);
    return angle;
}

boost::optional<Coordinate>
circleCenter(const Coordinate C1, const Coordinate C2, const Coordinate C3)
{
    // free after http://paulbourke.net/geometry/circlesphere/
    // require three distinct points
    if (C1 == C2 || C2 == C3 || C1 == C3)
    {
        return boost::none;
    }

    // define line through c1, c2 and c2,c3
    const double C2C1_lat = static_cast<double>(toFloating(C2.lat - C1.lat)); // yDelta_a
    const double C2C1_lon = static_cast<double>(toFloating(C2.lon - C1.lon)); // xDelta_a
    const double C3C2_lat = static_cast<double>(toFloating(C3.lat - C2.lat)); // yDelta_b
    const double C3C2_lon = static_cast<double>(toFloating(C3.lon - C2.lon)); // xDelta_b

    // check for collinear points in X-Direction / Y-Direction
    if ((std::abs(C2C1_lon) < std::numeric_limits<double>::epsilon() &&
         std::abs(C3C2_lon) < std::numeric_limits<double>::epsilon()) ||
        (std::abs(C2C1_lat) < std::numeric_limits<double>::epsilon() &&
         std::abs(C3C2_lat) < std::numeric_limits<double>::epsilon()))
    {
        return boost::none;
    }
    else if (std::abs(C2C1_lon) < std::numeric_limits<double>::epsilon())
    {
        // vertical line C2C1
        // due to c1.lon == c2.lon && c1.lon != c3.lon we can rearrange this way
        BOOST_ASSERT(std::abs(static_cast<double>(toFloating(C3.lon - C1.lon))) >=
                         std::numeric_limits<double>::epsilon() &&
                     std::abs(static_cast<double>(toFloating(C2.lon - C3.lon))) >=
                         std::numeric_limits<double>::epsilon());
        return circleCenter(C1, C3, C2);
    }
    else if (std::abs(C3C2_lon) < std::numeric_limits<double>::epsilon())
    {
        // vertical line C3C2
        // due to c2.lon == c3.lon && c1.lon != c3.lon we can rearrange this way
        // after rearrangement both deltas will be zero
        BOOST_ASSERT(std::abs(static_cast<double>(toFloating(C1.lon - C2.lon))) >=
                         std::numeric_limits<double>::epsilon() &&
                     std::abs(static_cast<double>(toFloating(C3.lon - C1.lon))) >=
                         std::numeric_limits<double>::epsilon());
        return circleCenter(C2, C1, C3);
    }
    else
    {
        const double C2C1_slope = C2C1_lat / C2C1_lon;
        const double C3C2_slope = C3C2_lat / C3C2_lon;

        if (std::abs(C2C1_slope) < std::numeric_limits<double>::epsilon())
        {
            // Three non-collinear points with C2,C1 on same latitude.
            // Due to the x-values correct, we can swap C3 and C1 to obtain the correct slope value
            return circleCenter(C3, C2, C1);
        }
        // valid slope values for both lines, calculate the center as intersection of the lines

        // can this ever happen?
        if (std::abs(C2C1_slope - C3C2_slope) < std::numeric_limits<double>::epsilon())
            return boost::none;

        const double C1_y = static_cast<double>(toFloating(C1.lat));
        const double C1_x = static_cast<double>(toFloating(C1.lon));
        const double C2_y = static_cast<double>(toFloating(C2.lat));
        const double C2_x = static_cast<double>(toFloating(C2.lon));
        const double C3_y = static_cast<double>(toFloating(C3.lat));
        const double C3_x = static_cast<double>(toFloating(C3.lon));

        const double lon = (C2C1_slope * C3C2_slope * (C1_y - C3_y) + C3C2_slope * (C1_x + C2_x) -
                            C2C1_slope * (C2_x + C3_x)) /
                           (2 * (C3C2_slope - C2C1_slope));
        const double lat = (0.5 * (C1_x + C2_x) - lon) / C2C1_slope + 0.5 * (C1_y + C2_y);
        if (lon < -180.0 || lon > 180.0 || lat < -90.0 || lat > 90.0)
            return boost::none;
        else
            return Coordinate(FloatLongitude{lon}, FloatLatitude{lat});
    }
}

double circleRadius(const Coordinate C1, const Coordinate C2, const Coordinate C3)
{
    // a circle by three points requires thee distinct points
    auto center = circleCenter(C1, C2, C3);
    if (center)
        return haversineDistance(C1, *center);
    else
        return std::numeric_limits<double>::infinity();
}

Coordinate interpolateLinear(double factor, const Coordinate from, const Coordinate to)
{
    BOOST_ASSERT(0 <= factor && factor <= 1.0);

    const auto from_lon = static_cast<std::int32_t>(from.lon);
    const auto from_lat = static_cast<std::int32_t>(from.lat);
    const auto to_lon = static_cast<std::int32_t>(to.lon);
    const auto to_lat = static_cast<std::int32_t>(to.lat);

    FixedLongitude interpolated_lon{
        static_cast<std::int32_t>(from_lon + factor * (to_lon - from_lon))};
    FixedLatitude interpolated_lat{
        static_cast<std::int32_t>(from_lat + factor * (to_lat - from_lat))};

    return {std::move(interpolated_lon), std::move(interpolated_lat)};
}

// compute the signed area of a triangle
double signedArea(const Coordinate first_coordinate,
                  const Coordinate second_coordinate,
                  const Coordinate third_coordinate)
{
    const auto lat_1 = static_cast<double>(toFloating(first_coordinate.lat));
    const auto lon_1 = static_cast<double>(toFloating(first_coordinate.lon));
    const auto lat_2 = static_cast<double>(toFloating(second_coordinate.lat));
    const auto lon_2 = static_cast<double>(toFloating(second_coordinate.lon));
    const auto lat_3 = static_cast<double>(toFloating(third_coordinate.lat));
    const auto lon_3 = static_cast<double>(toFloating(third_coordinate.lon));
    return 0.5 * (-lon_2 * lat_1 + lon_3 * lat_1 + lon_1 * lat_2 - lon_3 * lat_2 - lon_1 * lat_3 +
                  lon_2 * lat_3);
}

// check if a set of three coordinates is given in CCW order
bool isCCW(const Coordinate first_coordinate,
           const Coordinate second_coordinate,
           const Coordinate third_coordinate)
{
    return signedArea(first_coordinate, second_coordinate, third_coordinate) > 0;
}

std::pair<util::Coordinate, util::Coordinate>
leastSquareRegression(const std::vector<util::Coordinate> &coordinates)
{
    BOOST_ASSERT(coordinates.size() >= 2);
    double sum_lon = 0, sum_lat = 0, sum_lon_lat = 0, sum_lon_lon = 0;
    double min_lon = static_cast<double>(toFloating(coordinates.front().lon));
    double max_lon = static_cast<double>(toFloating(coordinates.front().lon));
    for (const auto coord : coordinates)
    {
        min_lon = std::min(min_lon, static_cast<double>(toFloating(coord.lon)));
        max_lon = std::max(max_lon, static_cast<double>(toFloating(coord.lon)));
        sum_lon += static_cast<double>(toFloating(coord.lon));
        sum_lon_lon +=
            static_cast<double>(toFloating(coord.lon)) * static_cast<double>(toFloating(coord.lon));
        sum_lat += static_cast<double>(toFloating(coord.lat));
        sum_lon_lat +=
            static_cast<double>(toFloating(coord.lon)) * static_cast<double>(toFloating(coord.lat));
    }

    const auto dividend = coordinates.size() * sum_lon_lat - sum_lon * sum_lat;
    const auto divisor = coordinates.size() * sum_lon_lon - sum_lon * sum_lon;
    if (std::abs(divisor) < std::numeric_limits<double>::epsilon())
        return std::make_pair(coordinates.front(), coordinates.back());

    // slope of the regression line
    const auto slope = dividend / divisor;
    const auto intercept = (sum_lat - slope * sum_lon) / coordinates.size();

    const auto GetLatAtLon = [intercept,
                              slope](const util::FloatLongitude longitude) -> util::FloatLatitude {
        return {intercept + slope * static_cast<double>((longitude))};
    };

    const double offset = 0.00001;
    const util::Coordinate regression_first = {
        toFixed(util::FloatLongitude{min_lon - offset}),
        toFixed(util::FloatLatitude(GetLatAtLon(util::FloatLongitude{min_lon - offset})))};
    const util::Coordinate regression_end = {
        toFixed(util::FloatLongitude{max_lon + offset}),
        toFixed(util::FloatLatitude(GetLatAtLon(util::FloatLongitude{max_lon + offset})))};

    return {regression_first, regression_end};
}

// find the closest distance between a coordinate and a segment
double findClosestDistance(const Coordinate coordinate,
                           const Coordinate segment_begin,
                           const Coordinate segment_end)
{
    return haversineDistance(coordinate,
                             projectPointOnSegment(segment_begin, segment_end, coordinate).second);
}

// find the closest distance between a coordinate and a set of coordinates
double findClosestDistance(const Coordinate coordinate,
                           const std::vector<util::Coordinate> &coordinates)
{
    double current_min = std::numeric_limits<double>::max();

    // comparator updating current_min without ever finding an element
    const auto compute_minimum_distance = [&current_min, coordinate](const Coordinate lhs,
                                                                     const Coordinate rhs) {
        current_min = std::min(current_min, findClosestDistance(coordinate, lhs, rhs));
        return false;
    };

    std::adjacent_find(std::begin(coordinates), std::end(coordinates), compute_minimum_distance);
    return current_min;
}

// find the closes distance between two sets of coordinates
double findClosestDistance(const std::vector<util::Coordinate> &lhs,
                           const std::vector<util::Coordinate> &rhs)
{
    double current_min = std::numeric_limits<double>::max();

    const auto compute_minimum_distance_in_rhs = [&current_min, &rhs](const Coordinate coordinate) {
        current_min = std::min(current_min, findClosestDistance(coordinate, rhs));
        return false;
    };

    std::find_if(std::begin(lhs), std::end(lhs), compute_minimum_distance_in_rhs);
    return current_min;
}

std::vector<double> getDeviations(const std::vector<util::Coordinate> &from,
                                  const std::vector<util::Coordinate> &to)
{
    auto find_deviation = [&to](const util::Coordinate coordinate) {
        return findClosestDistance(coordinate, to);
    };

    std::vector<double> deviations_from;
    deviations_from.reserve(from.size());
    std::transform(
        std::begin(from), std::end(from), std::back_inserter(deviations_from), find_deviation);

    return deviations_from;
}

bool areParallel(const std::vector<util::Coordinate> &lhs, const std::vector<util::Coordinate> &rhs)
{
    const auto regression_lhs = leastSquareRegression(lhs);
    const auto regression_rhs = leastSquareRegression(rhs);

    const auto get_slope = [](const util::Coordinate from, const util::Coordinate to) {
        const auto diff_lat = static_cast<int>(from.lat) - static_cast<int>(to.lat);
        const auto diff_lon = static_cast<int>(from.lon) - static_cast<int>(to.lon);
        if (diff_lon == 0)
            return std::numeric_limits<double>::max();
        return static_cast<double>(diff_lat) / static_cast<double>(diff_lon);
    };

    const auto print_coord = [](const Coordinate coord) {
        std::ostringstream oss;
        oss << std::setprecision(12) << util::toFloating(coord.lat) << " "
                  << util::toFloating(coord.lon);
        return oss.str();
    };

    const auto null_island = Coordinate(FixedLongitude{0}, FixedLatitude{0});
    const auto difference_lhs = difference(regression_lhs.first, regression_lhs.second);
    const auto difference_rhs = difference(regression_rhs.first, regression_rhs.second);

    // we normalise the left slope to be zero, so we rotate the coordinates around 0,0 to match 90
    // degrees
    const auto bearing_lhs = bearing(null_island, difference_lhs);

    // we rotate to have one of the lines facing horizontally to the right (bearing 90 degree)
    const auto rotation_angle_radians = degToRad(bearing_lhs - 90);

    const auto rotated_difference_lhs = rotateCCWAroundZero(difference_lhs, rotation_angle_radians);
    const auto rotated_difference_rhs = rotateCCWAroundZero(difference_rhs, rotation_angle_radians);

    const auto slope_lhs = get_slope(null_island, rotated_difference_lhs);
    const auto slope_rhs = get_slope(null_island, rotated_difference_rhs);

    const auto compute_ratio = [](auto dividend, auto divisor) {
        if (std::abs(dividend) < std::abs(divisor))
            std::swap(dividend, divisor);
        auto save_divisor = (std::abs(divisor) < std::numeric_limits<decltype(divisor)>::epsilon())
                                ? std::numeric_limits<decltype(divisor)>::epsilon()
                                : divisor;
        return dividend / save_divisor;
    };

    const auto slope_lhs_2 = get_slope(regression_lhs.first, regression_lhs.second);
    const auto slope_rhs_2 = get_slope(regression_rhs.first, regression_rhs.second);

    const auto ratio = compute_ratio(slope_lhs, slope_rhs);

    // the left hand side has a slope of `0` after the rotation. We can check the slope of the right hand side to ensure we only considering slight slopes
    return std::abs(slope_rhs) < 0.1;   //ten percent incline at the most
}

Coordinate rotateCCWAroundZero(Coordinate coordinate, double angle_in_radians)
{
    /*
     * a rotation  around 0,0 in vector space is defined as
     *
     * | cos a   -sin a | . | lon |
     * | sin a    cos a |   | lat |
     *
     * resulting in cos a lon - sin a lon for the new longitude and sin a lon + cos a lat for the
     * new latitude
     */

    const auto cos_alpha = cos(angle_in_radians);
    const auto sin_alpha = sin(angle_in_radians);

    const auto lon = static_cast<double>(toFloating(coordinate.lon));
    const auto lat = static_cast<double>(toFloating(coordinate.lat));

    return {util::FloatLongitude{cos_alpha * lon - sin_alpha * lat},
            util::FloatLatitude{sin_alpha * lon + cos_alpha * lat}};
}

Coordinate difference(const Coordinate lhs, const Coordinate rhs)
{
    const auto lon_diff_int = static_cast<int>(lhs.lon) - static_cast<int>(rhs.lon);
    const auto lat_diff_int = static_cast<int>(lhs.lat) - static_cast<int>(rhs.lat);
    return {util::FixedLongitude{lon_diff_int}, util::FixedLatitude{lat_diff_int}};
}

} // ns coordinate_calculation
} // ns util
} // ns osrm
