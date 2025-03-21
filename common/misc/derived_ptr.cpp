#include "derived_ptr.h"

namespace cmn::gui {

#ifndef NDEBUG
IMPLEMENT(DebugPointers::allocated_pointers);
IMPLEMENT(DebugPointers::zones);
IMPLEMENT(DebugPointers::current_zone);

void DebugPointers::register_named(const std::string& key, void* ptr) {
    if (not ptr)
        return;
	if (current_zone.has_value()) {
		auto it = zones[current_zone.value()].pointers.find(ptr);
		if (it != zones[current_zone.value()].pointers.end()) {
			FormatWarning("Pointer", hex(ptr), " already contained in allocated_ptrs in zone", current_zone.value());
		}
		zones[current_zone.value()].pointers[ptr] = key;
		//Print(" ** registered ", zones[current_zone.value()].pointers.size(), " objects for ", key, " in zone ", current_zone.value(), " => ", hex(ptr));
		for (auto& [k, v] : allocated_pointers) {
			if (v.contains(ptr)) {
				FormatWarning("Pointer", hex(ptr), " already contained in allocated_ptrs for ", k);
			}
		}
	}
	else {
		allocated_pointers[key].insert(ptr);
		//Print(" ** registered ", allocated_pointers[key].size(), " objects for ", key);
	}
}


void DebugPointers::unregister_named(const std::string& key, void* ptr) {
    if (not ptr)
        return;
	if (current_zone.has_value()) {
		if (zones[current_zone.value()].pointers.find(ptr) != zones[current_zone.value()].pointers.end()) {
			zones[current_zone.value()].pointers.erase(ptr);
			//Print(" ** unregistered ", zones[current_zone.value()].pointers.size(), " objects for ", key, " => ", hex(ptr));
			return;
		}
	}

	for (auto& [k, v] : allocated_pointers) {
		if (v.find(ptr) != v.end()) {
			v.erase(ptr);
			//Print(" ** unregistered ", v.size(), " objects for ", k, " => ", hex(ptr));
			return;
		}
	}

	for (auto& [name, zone] : zones) {
		if (zone.pointers.find(ptr) != zone.pointers.end()) {
			zone.pointers.erase(ptr);
			//Print(" ** unregistered ", zone.pointers.size(), " objects for ", name, " => ", hex(ptr));
			return;
		}
	}

	FormatWarning("Cannot find object ", hex(ptr), " to unregister.");
}

void DebugPointers::clear() {
	allocated_pointers.clear();
}

void DebugPointers::start_zone(const std::string& name) {
	zones[name];
	current_zone = name;
	zones[name].count_at_start = zones[name].pointers.size();
	zones[name].at_start.clear();
	for (auto& [ptr, _] : zones[name].pointers) {
		zones[name].at_start.insert(ptr);
	}
	DebugHeader("Zone ", name, " started containing ", zones[name].count_at_start, " objects");
}

void DebugPointers::end_zone(const std::string& name) {
	if(not current_zone.has_value()
		|| current_zone.value() != name)
	{
		FormatError("Zone ", name, " was not started.");
		return;
	}

	std::unordered_map<std::string, size_t> counts;
	for (auto& [ptr, key] : zones[name].pointers) {
		counts[key]++;
	}

	std::unordered_map<std::string, size_t> new_counts;
	for (auto& [ptr, key] : zones[name].pointers) {
		if (zones[name].at_start.find(ptr) == zones[name].at_start.end()) {
			new_counts[key]++;
		}
	}

	/*Print("New objects:");
	for (auto& [key, count] : new_counts) {
		Print(" ** ", count, " objects of type ", key);
	}

	Print("Summary:");
	for (auto& [key, count] : counts) {
		Print(" ** ", count, " objects of type ", key);
	}*/

	current_zone.reset();
	DebugHeader("Zone ", name, " ended containing ", zones[name].pointers.size(), " objects (", zones[name].pointers.size() - zones[name].count_at_start, " new)");
}

#endif

}