#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
int main() {
  for (double value : {1e-300, 1.0, 1e200, 1e308, std::numeric_limits<double>::denorm_min()})
    std::cout << std::setprecision(17) << "component=" << value
              << " raw_hypot=" << std::hypot(value, value, 0.0) << '\n';
}
