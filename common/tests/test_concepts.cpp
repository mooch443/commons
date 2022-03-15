#include <concepts>

int main() {
	static_assert(std::convertible_to<int, int>, "Convertible_to not available.");
}
