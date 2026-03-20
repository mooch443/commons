"""
LLDB visualizers for TRex container types.

Import this script from LLDB to register collection-style summaries and
synthetic children for:
- cmn::IllegalArray<T>
- robin_hood hash maps / sets
- ska flat_hash_map / bytell_hash_map style tables
- std::expected
"""

import lldb

_MAX_SYNTHETIC_CHILDREN = 4096
_MAX_BUCKETS = 1 << 20
_MAX_SHERWOOD_V3_LOOKUPS = 512
_MAX_ROBIN_HOOD_EXTRA_INFO = 255
_MAX_ILLEGAL_ARRAY_CAPACITY = 1 << 20

_ILLEGAL_ARRAY_REGEX = r"^cmn::IllegalArray<.+>$"
_ILLEGAL_ARRAY_CATEGORY = "trex_illegal_array"

_ROBIN_HOOD_REGEXES = (
    r"^robin_hood::detail::Table<.+>$",
    r"^robin_hood::unordered_map<.+>$",
    r"^robin_hood::unordered_set<.+>$",
    r"^robin_hood::unordered_flat_map<.+>$",
    r"^robin_hood::unordered_flat_set<.+>$",
    r"^robin_hood::unordered_node_map<.+>$",
    r"^robin_hood::unordered_node_set<.+>$",
)
_ROBIN_HOOD_CATEGORY = "trex_robin_hood"

_SHERWOOD_REGEXES = (
    r"^ska::detailv3::sherwood_v3_table<.+>$",
    r"^ska::detailv8::sherwood_v8_table<.+>$",
    r"^ska::flat_hash_map<.+>$",
    r"^ska::flat_hash_set<.+>$",
    r"^ska::bytell_hash_map<.+>$",
    r"^ska::bytell_hash_set<.+>$",
)
_SHERWOOD_CATEGORY = "trex_sherwood"

_STD_EXPECTED_REGEXES = (
    r"^std::expected<.+>$",
    r"^std::__1::expected<.+>$",
)
_STD_EXPECTED_CATEGORY = "trex_std_expected"


def _get_uint(child, default=0):
    return child.GetValueAsUnsigned(default) if child.IsValid() else default


def _get_int(child, default=0):
    return child.GetValueAsSigned(default) if child.IsValid() else default


def _is_pointer_type(sbtype):
    if not sbtype.IsValid():
        return False
    if hasattr(sbtype, "IsPointerType"):
        return sbtype.IsPointerType()
    return bool(sbtype.GetTypeClass() & lldb.eTypeClassPointer)


def _find_member(valobj, name, depth=8):
    if not valobj.IsValid():
        return lldb.SBValue()

    direct = valobj.GetChildMemberWithName(name)
    if direct.IsValid():
        return direct

    if depth <= 0:
        return lldb.SBValue()

    for index in range(valobj.GetNumChildren()):
        child = valobj.GetChildAtIndex(index)
        if not child.IsValid():
            continue
        found = _find_member(child, name, depth - 1)
        if found.IsValid():
            return found

    return lldb.SBValue()


def _create_named_value(parent, name, value):
    if not value.IsValid():
        return None

    pointer = value.AddressOf()
    if not pointer.IsValid():
        return value

    address = pointer.GetValueAsUnsigned(0)
    if not address:
        return value

    return parent.CreateValueFromAddress(name, address, value.GetType())


def _dereference_pointer(parent, name, pointer_value):
    if not pointer_value.IsValid():
        return None

    pointee_type = pointer_value.GetType().GetPointeeType()
    if not pointee_type.IsValid():
        return None

    address = pointer_value.GetValueAsUnsigned(0)
    if not address:
        return None

    return parent.CreateValueFromAddress(name, address, pointee_type)


def _container_summary(size, buckets):
    if buckets:
        return f"size={size}, buckets={buckets}, load={size / buckets:.3f}"
    return f"size={size}, buckets=0"


def _bool_text(value):
    return "true" if value else "false"


def _is_nonnull_pointer(value):
    return value.IsValid() and value.GetValueAsUnsigned(0) != 0


def _is_reasonable_count(value, limit):
    return 0 <= value <= limit


def _is_reasonable_bucket_count(value):
    return _is_reasonable_count(value, _MAX_BUCKETS)


class IllegalArraySyntheticProvider:
    """Pretty-printer that exposes IllegalArray elements as synthetic children."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._ptr = raw.GetChildMemberWithName("_ptr")
        size_child = raw.GetChildMemberWithName("_size")
        cap_child = raw.GetChildMemberWithName("_capacity")
        self._size = size_child.GetValueAsUnsigned(0) if size_child.IsValid() else 0
        self._capacity = cap_child.GetValueAsUnsigned(0) if cap_child.IsValid() else 0
        self._valid = False
        if self._ptr.IsValid():
            self._elem_type = self._ptr.GetType().GetPointeeType()
            self._elem_size = self._elem_type.GetByteSize()
        else:
            self._elem_type = None
            self._elem_size = 0

        self._valid = (
            _is_nonnull_pointer(self._ptr)
            and self._elem_type is not None
            and self._elem_size > 0
            and _is_reasonable_count(self._capacity, _MAX_ILLEGAL_ARRAY_CAPACITY)
            and _is_reasonable_count(self._size, min(self._capacity, _MAX_SYNTHETIC_CHILDREN))
            and self._size <= self._capacity
        )

    def has_children(self):
        return self._valid and bool(self._size)

    def num_children(self):
        return self._size if self._valid else 0

    def get_child_at_index(self, index):
        if not self._valid or index < 0 or index >= self._size:
            return None
        offset = index * self._elem_size
        return self._ptr.CreateChildAtOffset(f"[{index}]", offset, self._elem_type)


def illegal_array_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    size = _get_uint(raw.GetChildMemberWithName("_size"))
    capacity = _get_uint(raw.GetChildMemberWithName("_capacity"))
    ptr = raw.GetChildMemberWithName("_ptr")
    if (
        not _is_reasonable_count(capacity, _MAX_ILLEGAL_ARRAY_CAPACITY)
        or size > capacity
        or size > _MAX_SYNTHETIC_CHILDREN
        or (size and not _is_nonnull_pointer(ptr))
    ):
        return "invalid/uninitialized"
    return f"size={size}, capacity={capacity}"


class RobinHoodSyntheticProvider:
    """Expose robin_hood::Table entries as synthetic children."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._keyvals = _find_member(raw, "mKeyVals")
        self._info = _find_member(raw, "mInfo")
        self._size = _get_uint(_find_member(raw, "mNumElements"))
        self._mask = _get_uint(_find_member(raw, "mMask"))
        self._node_type = lldb.SBType()
        self._node_size = 0
        self._occupied_slots = []
        self._valid = False

        if not self._keyvals.IsValid() or not self._info.IsValid() or not self._size:
            return

        self._node_type = self._keyvals.GetType().GetPointeeType()
        if not self._node_type.IsValid():
            return

        info_type = self._info.GetType().GetPointeeType()
        if not info_type.IsValid():
            return

        buckets = self._mask + 1 if self._mask else 0
        if (
            not _is_nonnull_pointer(self._keyvals)
            or not _is_nonnull_pointer(self._info)
            or not _is_reasonable_bucket_count(buckets)
            or not _is_reasonable_count(self._size, _MAX_SYNTHETIC_CHILDREN)
            or self._size > buckets + _MAX_ROBIN_HOOD_EXTRA_INFO
        ):
            return

        self._node_size = self._node_type.GetByteSize()
        if self._node_size <= 0:
            return

        info_size = max(info_type.GetByteSize(), 1)
        max_scan = buckets + _MAX_ROBIN_HOOD_EXTRA_INFO
        self._valid = True

        for slot in range(max_scan):
            info_child = self._info.CreateChildAtOffset(
                f"_info[{slot}]",
                slot * info_size,
                info_type,
            )
            if _get_uint(info_child) != 0:
                self._occupied_slots.append(slot)
                if len(self._occupied_slots) == self._size:
                    break

    def has_children(self):
        return self._valid and bool(self._occupied_slots)

    def num_children(self):
        return len(self._occupied_slots) if self._valid else 0

    def get_child_index(self, name):
        if name.startswith("[") and name.endswith("]"):
            try:
                return int(name[1:-1])
            except ValueError:
                return -1
        return -1

    def get_child_at_index(self, index):
        if not self._valid or index < 0 or index >= len(self._occupied_slots):
            return None

        slot = self._occupied_slots[index]
        node = self._keyvals.CreateChildAtOffset(
            f"_node[{slot}]",
            slot * self._node_size,
            self._node_type,
        )
        if not node.IsValid():
            return None

        data = node.GetChildMemberWithName("mData")
        if not data.IsValid():
            return None

        if _is_pointer_type(data.GetType()):
            return _dereference_pointer(self.valobj, f"[{index}]", data)

        return _create_named_value(self.valobj, f"[{index}]", data)


def robin_hood_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    size = _get_uint(_find_member(raw, "mNumElements"))
    mask = _get_uint(_find_member(raw, "mMask"))
    buckets = mask + 1 if mask else 0
    keyvals = _find_member(raw, "mKeyVals")
    info = _find_member(raw, "mInfo")
    if (
        not _is_reasonable_bucket_count(buckets)
        or not _is_reasonable_count(size, _MAX_SYNTHETIC_CHILDREN)
        or size > buckets + _MAX_ROBIN_HOOD_EXTRA_INFO
        or (size and (not _is_nonnull_pointer(keyvals) or not _is_nonnull_pointer(info)))
    ):
        return "invalid/uninitialized"
    return _container_summary(size, buckets)


class SherwoodSyntheticProvider:
    """Expose ska sherwood table entries as synthetic children."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._entries = _find_member(raw, "entries")
        self._size = _get_uint(_find_member(raw, "num_elements"))
        self._num_slots_minus_one = _get_uint(_find_member(raw, "num_slots_minus_one"))
        self._max_lookups = _get_int(_find_member(raw, "max_lookups"), -1)
        self._occupied_slots = []
        self._layout = None
        self._valid = False

        if not self._entries.IsValid() or not self._size:
            return

        buckets = self._num_slots_minus_one + 1 if self._num_slots_minus_one else 0
        if (
            not _is_nonnull_pointer(self._entries)
            or not _is_reasonable_bucket_count(buckets)
            or not _is_reasonable_count(self._size, _MAX_SYNTHETIC_CHILDREN)
            or self._size > buckets
        ):
            return

        entry_sample = self._entries.Dereference()
        if not entry_sample.IsValid():
            return

        if entry_sample.GetChildMemberWithName("distance_from_desired").IsValid():
            self._layout = "v3"
            self._entry_type = entry_sample.GetType()
            self._entry_size = self._entry_type.GetByteSize()
            if self._entry_size <= 0:
                return
            self._collect_v3_slots()
            self._valid = True
            return

        control_bytes = entry_sample.GetChildMemberWithName("control_bytes")
        if control_bytes.IsValid():
            self._layout = "v8"
            self._block_type = entry_sample.GetType()
            self._block_size_bytes = self._block_type.GetByteSize()
            if self._block_size_bytes <= 0:
                return
            self._collect_v8_slots()
            self._valid = True

    def _collect_v3_slots(self):
        if self._max_lookups < 0 or self._max_lookups > _MAX_SHERWOOD_V3_LOOKUPS:
            return

        scan_limit = self._num_slots_minus_one + self._max_lookups
        if not _is_reasonable_count(scan_limit, _MAX_BUCKETS + _MAX_SHERWOOD_V3_LOOKUPS):
            return
        for slot in range(scan_limit):
            entry = self._entries.CreateChildAtOffset(
                f"_entry[{slot}]",
                slot * self._entry_size,
                self._entry_type,
            )
            if _get_int(entry.GetChildMemberWithName("distance_from_desired"), -1) >= 0:
                self._occupied_slots.append(slot)
                if len(self._occupied_slots) == self._size:
                    break

    def _collect_v8_slots(self):
        num_slots = self._num_slots_minus_one + 1 if self._num_slots_minus_one else 0
        if not num_slots or not _is_reasonable_bucket_count(num_slots):
            return

        block_sample = self._entries.Dereference()
        if not block_sample.IsValid():
            return

        control_bytes = block_sample.GetChildMemberWithName("control_bytes")
        block_width = control_bytes.GetNumChildren() if control_bytes.IsValid() else 0
        if not block_width:
            return

        for slot in range(num_slots):
            block_index = slot // block_width
            slot_in_block = slot % block_width
            block = self._entries.CreateChildAtOffset(
                f"_block[{block_index}]",
                block_index * self._block_size_bytes,
                self._block_type,
            )
            control = block.GetChildMemberWithName("control_bytes")
            metadata = _get_int(control.GetChildAtIndex(slot_in_block), -1)
            if metadata not in (-1, -2):
                self._occupied_slots.append(slot)
                if len(self._occupied_slots) == self._size:
                    break

    def has_children(self):
        return self._valid and bool(self._occupied_slots)

    def num_children(self):
        return len(self._occupied_slots) if self._valid else 0

    def get_child_index(self, name):
        if name.startswith("[") and name.endswith("]"):
            try:
                return int(name[1:-1])
            except ValueError:
                return -1
        return -1

    def get_child_at_index(self, index):
        if not self._valid or index < 0 or index >= len(self._occupied_slots):
            return None

        if self._layout == "v3":
            return self._get_v3_child(index)
        if self._layout == "v8":
            return self._get_v8_child(index)
        return None

    def _get_v3_child(self, index):
        slot = self._occupied_slots[index]
        entry = self._entries.CreateChildAtOffset(
            f"_entry[{slot}]",
            slot * self._entry_size,
            self._entry_type,
        )
        return _create_named_value(
            self.valobj,
            f"[{index}]",
            entry.GetChildMemberWithName("value"),
        )

    def _get_v8_child(self, index):
        slot = self._occupied_slots[index]
        block = self._entries.Dereference()
        control = block.GetChildMemberWithName("control_bytes")
        block_width = control.GetNumChildren()
        block_index = slot // block_width
        slot_in_block = slot % block_width
        block = self._entries.CreateChildAtOffset(
            f"_block[{block_index}]",
            block_index * self._block_size_bytes,
            self._block_type,
        )
        data = block.GetChildMemberWithName("data")
        return _create_named_value(
            self.valobj,
            f"[{index}]",
            data.GetChildAtIndex(slot_in_block),
        )


def sherwood_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    size = _get_uint(_find_member(raw, "num_elements"))
    num_slots_minus_one = _get_uint(_find_member(raw, "num_slots_minus_one"))
    buckets = num_slots_minus_one + 1 if num_slots_minus_one else 0
    entries = _find_member(raw, "entries")
    max_lookups = _get_int(_find_member(raw, "max_lookups"), -1)
    if (
        not _is_reasonable_bucket_count(buckets)
        or not _is_reasonable_count(size, _MAX_SYNTHETIC_CHILDREN)
        or size > buckets
        or (size and not _is_nonnull_pointer(entries))
        or (max_lookups > _MAX_SHERWOOD_V3_LOOKUPS)
    ):
        return "invalid/uninitialized"
    return _container_summary(size, buckets)


class StdExpectedSyntheticProvider:
    """Expose libc++ std::expected payload or error as a synthetic child."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._has_val = bool(_get_uint(_find_member(raw, "__has_val_")))
        self._value = _find_member(raw, "__val_")
        self._error = _find_member(raw, "__unex_")
        self._children = []

        if self._has_val:
            if self._value.IsValid():
                self._children.append(("value", self._value))
        else:
            if self._error.IsValid():
                self._children.append(("error", self._error))

    def has_children(self):
        return bool(self._children)

    def num_children(self):
        return len(self._children)

    def get_child_index(self, name):
        for index, (child_name, _) in enumerate(self._children):
            if name == child_name or name == f"[{index}]":
                return index
        return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self._children):
            return None

        name, value = self._children[index]
        return _create_named_value(self.valobj, name, value)


def std_expected_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    has_val = bool(_get_uint(_find_member(raw, "__has_val_")))
    value = _find_member(raw, "__val_")
    error = _find_member(raw, "__unex_")

    if has_val:
        if value.IsValid():
            return f"has_value={_bool_text(True)}"
        return f"has_value={_bool_text(True)}"

    if error.IsValid():
        return f"has_value={_bool_text(False)}"
    return f"has_value={_bool_text(False)}"


def __lldb_init_module(debugger, _dict):
    """Invoked automatically when the script is imported via `command script import`."""
    debugger.HandleCommand(
        f"type summary add --category {_ILLEGAL_ARRAY_CATEGORY} "
        f"--python-function {__name__}.illegal_array_summary "
        f"--regex {_ILLEGAL_ARRAY_REGEX}"
    )
    debugger.HandleCommand(
        f"type synthetic add --category {_ILLEGAL_ARRAY_CATEGORY} "
        f"--python-class {__name__}.IllegalArraySyntheticProvider "
        f"--regex {_ILLEGAL_ARRAY_REGEX}"
    )
    debugger.HandleCommand(f"type category enable {_ILLEGAL_ARRAY_CATEGORY}")

    for regex in _ROBIN_HOOD_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_ROBIN_HOOD_CATEGORY} "
            f"--python-function {__name__}.robin_hood_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_ROBIN_HOOD_CATEGORY} "
            f"--python-class {__name__}.RobinHoodSyntheticProvider "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_ROBIN_HOOD_CATEGORY}")

    for regex in _SHERWOOD_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_SHERWOOD_CATEGORY} "
            f"--python-function {__name__}.sherwood_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_SHERWOOD_CATEGORY} "
            f"--python-class {__name__}.SherwoodSyntheticProvider "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_SHERWOOD_CATEGORY}")

    for regex in _STD_EXPECTED_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_STD_EXPECTED_CATEGORY} "
            f"--python-function {__name__}.std_expected_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_STD_EXPECTED_CATEGORY} "
            f"--python-class {__name__}.StdExpectedSyntheticProvider "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_STD_EXPECTED_CATEGORY}")

    print("[LLDB] IllegalArray, robin_hood, sherwood, and std::expected pretty-printers loaded.")
