#include "measurement_utilities.hpp"

#include "math/vector_funcs.hpp"

namespace we::world {

auto get_measurement_metrics(const measurement& measurement) noexcept -> measurement_metrics
{
   if (measurement.points.empty()) return {};

   measurement_metrics metrics = {
      .bbox = {measurement.points[0], measurement.points[0]},
   };

   for (std::size_t i = 1; i < measurement.points.size(); ++i) {
      metrics.bbox = math::integrate(metrics.bbox, measurement.points[i]);
      metrics.length += distance(measurement.points[i - 1], measurement.points[i]);
   }

   if (measurement.points.size() % 2 == 1) {
      metrics.centre = measurement.points[measurement.points.size() / 2];
   }
   else {
      const std::size_t midpoint = measurement.points.size() / 2;

      metrics.centre =
         (measurement.points[midpoint - 1] + measurement.points[midpoint]) / 2.0f;
   }

   return metrics;
}

}