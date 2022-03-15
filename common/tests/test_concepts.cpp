#include <concepts>

int main() {
	static_assert(std::convertible_to<int, int>, "std::convertible_to not available.");
}
