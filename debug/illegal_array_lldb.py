"""
LLDB visualizers for TRex container types.

Import this script from LLDB to register collection-style summaries and
synthetic children for:
- cmn::IllegalArray<T>
- robin_hood hash maps / sets
- ska flat_hash_map / bytell_hash_map style tables
- std::expected
- std/libc++ mutexes and lock wrappers
- cmn::LoggedMutex / cmn::LoggedLock
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

_BFRAME_REGEXES = (
    r"^cmn::BFrame_t<.+>$",
)

_OPTIONAL_REGEXES = (
    r"^std::optional<.+>$",
    r"^std::__1::optional<.+>$",
    r"^cmn::TrivialOptional<.+>$",
)
_OPTIONAL_CATEGORY = "trex_optional"

_MUTEX_REGEXES = (
    r"^pthread_mutex_t$",
    r"^pthread_rwlock_t$",
    r"^_opaque_pthread_mutex_t$",
    r"^_opaque_pthread_rwlock_t$",
    r"^std(::__1)?::mutex$",
    r"^std(::__1)?::recursive_mutex$",
    r"^std(::__1)?::timed_mutex$",
    r"^std(::__1)?::recursive_timed_mutex$",
    r"^std(::__1)?::__shared_mutex_base$",
    r"^std(::__1)?::shared_mutex$",
    r"^std(::__1)?::shared_timed_mutex$",
    r"^cmn::LoggedMutex<.+>$",
)
_MUTEX_CATEGORY = "trex_mutex"

_LOCK_REGEXES = (
    r"^std(::__1)?::unique_lock<.+>$",
    r"^std(::__1)?::shared_lock<.+>$",
    r"^std(::__1)?::lock_guard<.+>$",
    r"^std(::__1)?::scoped_lock<.+>$",
    r"^cmn::LoggedLock<.+>$",
)
_LOCK_CATEGORY = "trex_lock"


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


def _is_reference_type(sbtype):
    if not sbtype.IsValid():
        return False
    if hasattr(sbtype, "IsReferenceType"):
        return sbtype.IsReferenceType()
    return bool(sbtype.GetTypeClass() & lldb.eTypeClassReference)


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

    value_type = value.GetType()
    if _is_reference_type(value_type):
        pointer = value.AddressOf()
        if not pointer.IsValid():
            return value
        address = pointer.GetValueAsUnsigned(0)
        pointee_type = value_type.GetDereferencedType()
        if not address or not pointee_type.IsValid():
            return value
        return parent.CreateValueFromAddress(name, address, pointee_type)

    pointer = value.AddressOf()
    if not pointer.IsValid():
        return value

    address = pointer.GetValueAsUnsigned(0)
    if not address:
        return value

    return parent.CreateValueFromAddress(name, address, value_type)


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


def _first_valid(*values):
    for value in values:
        if value is not None and value.IsValid():
            return value
    return lldb.SBValue()


def _value_address(value):
    if not value.IsValid():
        return 0
    if _is_pointer_type(value.GetType()):
        return value.GetValueAsUnsigned(0)
    pointer = value.AddressOf()
    if pointer.IsValid():
        return pointer.GetValueAsUnsigned(0)
    return 0


def _hex_or_null(address):
    return "null" if not address else hex(address)


def _value_text(value):
    if not value.IsValid():
        return None
    summary = value.GetSummary()
    if summary:
        return summary
    text = value.GetValue()
    if text is not None and text != "":
        return text
    return None


def _summary_join(parts):
    return ", ".join(part for part in parts if part)


def _type_name(value):
    if not value.IsValid():
        return ""
    sbtype = value.GetType()
    if not sbtype.IsValid():
        return ""
    return sbtype.GetName() or ""


def _collect_members_named(valobj, name, out, depth=8):
    if depth < 0 or not valobj.IsValid():
        return
    for index in range(valobj.GetNumChildren()):
        child = valobj.GetChildAtIndex(index)
        if not child.IsValid():
            continue
        if child.GetName() == name:
            out.append(child)
            continue
        _collect_members_named(child, name, out, depth - 1)


def _is_nonnull_pointer(value):
    return value.IsValid() and value.GetValueAsUnsigned(0) != 0


def _is_reasonable_count(value, limit):
    return 0 <= value <= limit


def _is_reasonable_bucket_count(value):
    return _is_reasonable_count(value, _MAX_BUCKETS)


def _trivial_optional_has_value(value):
    payload = value.GetChildMemberWithName("value_")
    if not payload.IsValid():
        return False

    payload_type = payload.GetType()
    if not payload_type.IsValid():
        return False

    if hasattr(payload_type, "IsUnsignedIntegerType") and payload_type.IsUnsignedIntegerType():
        bits = max(payload_type.GetByteSize() * 8, 1)
        invalid = (1 << bits) - 1
        return payload.GetValueAsUnsigned(0) != invalid

    if hasattr(payload_type, "IsSignedIntegerType") and payload_type.IsSignedIntegerType():
        bits = max(payload_type.GetByteSize() * 8, 1)
        invalid = -(1 << (bits - 1))
        return payload.GetValueAsSigned(0) != invalid

    summary = payload.GetSummary()
    if summary:
        return True
    return payload.GetValue() not in (None, "")


def _extract_visible_child_value(value, names):
    for source in (value, value.GetNonSyntheticValue()):
        if not source.IsValid():
            continue
        for name in names:
            child = source.GetChildMemberWithName(name)
            if child.IsValid():
                text = _value_text(child)
                if text and text.lower() not in {"has value=true", "has_value=true"}:
                    return text
        if source.GetNumChildren() == 1:
            child = source.GetChildAtIndex(0)
            if child.IsValid():
                text = _value_text(child)
                if text and text.lower() not in {"has value=true", "has_value=true"}:
                    return text
    return None


def _extract_optional_payload(raw):
    payload_names = (
        "__val_",
        "_M_value",
        "value_",
        "Value",
        "value",
    )
    for name in payload_names:
        payload = _find_member(raw, name, depth=8)
        if not payload.IsValid():
            continue
        text = _value_text(payload)
        if text and text.lower() not in {"has value=true", "has_value=true"}:
            return text
    return None


def _optional_has_value(raw, summary_lower):
    if "nullopt" in summary_lower or "has value=false" in summary_lower or "has_value=false" in summary_lower:
        return False
    if "has value=true" in summary_lower or "has_value=true" in summary_lower:
        return True

    engaged_names = (
        "__engaged_",
        "_M_engaged",
        "has_value",
    )
    for name in engaged_names:
        engaged = _find_member(raw, name, depth=8)
        if engaged.IsValid():
            return bool(_get_uint(engaged))

    return None


def _squash_optional_like(value):
    if not value.IsValid():
        return "null"

    type_name = _type_name(value)
    summary = value.GetSummary() or ""
    summary_lower = summary.lower()

    if "trivialoptional<" in type_name:
        if not _trivial_optional_has_value(value):
            return "null"
        payload = value.GetChildMemberWithName("value_")
        text = _value_text(payload)
        return text if text else "null"

    payload_names = (
        "Value",
        "_frame",
        "value",
        "__val_",
        "__value_",
        "_M_value",
        "value_",
    )
    text = _extract_visible_child_value(value, payload_names)
    if text:
        return text

    raw = value.GetNonSyntheticValue()
    if raw.IsValid():
        has_value = _optional_has_value(raw, summary_lower)
        if has_value is False:
            return "null"
        text = _extract_optional_payload(raw)
        if text:
            return text
        if has_value is True:
            return "value"

    text = _value_text(value)
    if text and text.lower() not in {"has value=true", "has_value=true"}:
        return text

    return "null"


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
        self._child_count = 0
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
            and _is_reasonable_count(self._size, self._capacity)
            and self._size <= self._capacity
        )
        if self._valid:
            self._child_count = min(self._size, _MAX_SYNTHETIC_CHILDREN)

    def has_children(self):
        return self._valid and bool(self._child_count)

    def num_children(self):
        return self._child_count if self._valid else 0

    def get_child_at_index(self, index):
        if not self._valid or index < 0 or index >= self._child_count:
            return None
        offset = index * self._elem_size
        return self._ptr.CreateChildAtOffset(f"[{index}]", offset, self._elem_type)


class EmptySyntheticProvider:
    """Suppress children for summary-only types."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj

    def update(self):
        return

    def has_children(self):
        return False

    def num_children(self):
        return 0

    def get_child_index(self, name):
        return -1

    def get_child_at_index(self, index):
        return None


def optional_summary(valobj, _dict):
    return _squash_optional_like(valobj.GetNonSyntheticValue())


def bframe_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    frame = raw.GetChildMemberWithName("_frame")
    return _squash_optional_like(frame)


def illegal_array_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    size = _get_uint(raw.GetChildMemberWithName("_size"))
    capacity = _get_uint(raw.GetChildMemberWithName("_capacity"))
    ptr = raw.GetChildMemberWithName("_ptr")
    if (
        not _is_reasonable_count(capacity, _MAX_ILLEGAL_ARRAY_CAPACITY)
        or size > capacity
        or (size and not _is_nonnull_pointer(ptr))
    ):
        return "invalid/uninitialized"
    if size > _MAX_SYNTHETIC_CHILDREN:
        return f"size={size}, capacity={capacity}, showing={_MAX_SYNTHETIC_CHILDREN}"
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


def _native_mutex_summary(raw):
    native = _first_valid(
        raw.GetChildMemberWithName("__m_"),
        raw if raw.GetChildMemberWithName("__sig").IsValid() else None,
    )
    signature = native.GetChildMemberWithName("__sig") if native.IsValid() else lldb.SBValue()
    parts = []
    if native.IsValid():
        parts.append(f"native={_hex_or_null(_value_address(native))}")
    sig_value = _get_int(signature, 0)
    if signature.IsValid() and sig_value:
        parts.append(f"sig=0x{sig_value:x}")
    return _summary_join(parts) or "opaque"


def _shared_mutex_summary(raw):
    base = _first_valid(raw.GetChildMemberWithName("__base_"), raw)
    state_child = _find_member(base, "__state_", depth=3)
    state = _get_uint(state_child)
    bits = max(state_child.GetType().GetByteSize() * 8, 1) if state_child.IsValid() else 32
    write_mask = 1 << (bits - 1)
    readers_mask = write_mask - 1
    writer = bool(state & write_mask)
    readers = state & readers_mask
    if writer:
        mode = "exclusive"
    elif readers:
        mode = f"shared({readers})"
    else:
        mode = "unlocked"
    parts = [f"mode={mode}"]
    if state_child.IsValid():
        parts.append(f"state=0x{state:x}")
    return _summary_join(parts)


def _timed_mutex_summary(raw):
    locked = bool(_get_uint(raw.GetChildMemberWithName("__locked_")))
    native = raw.GetChildMemberWithName("__m_")
    return _summary_join(
        (
            f"locked={_bool_text(locked)}",
            f"native={_hex_or_null(_value_address(native))}" if native.IsValid() else None,
        )
    )


def _recursive_timed_mutex_summary(raw):
    count = _get_uint(raw.GetChildMemberWithName("__count_"))
    owner = _find_member(raw.GetChildMemberWithName("__id_"), "__id_", depth=2)
    owner_value = owner.GetValueAsUnsigned(0) if owner.IsValid() else 0
    return _summary_join(
        (
            f"locked={_bool_text(bool(count))}",
            f"count={count}",
            f"owner={hex(owner_value)}" if owner_value else None,
        )
    )


def _logged_mutex_summary(raw):
    name = _value_text(raw.GetChildMemberWithName("_name"))
    owner = _value_text(raw.GetChildMemberWithName("_thread_name"))
    native = raw.GetChildMemberWithName("_mtx")
    return _summary_join(
        (
            f"name={name}" if name and name != '"<none>"' else None,
            f"native={_hex_or_null(_value_address(native))}" if native.IsValid() else None,
            f"last_owner={owner}" if owner and owner != '""' else None,
        )
    ) or "logged_mutex"


def mutex_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    type_name = raw.GetType().GetName() or ""

    if "LoggedMutex<" in type_name:
        return _logged_mutex_summary(raw)
    if "shared_mutex" in type_name or "shared_timed_mutex" in type_name or "__shared_mutex_base" in type_name:
        return _shared_mutex_summary(raw)
    if "recursive_timed_mutex" in type_name:
        return _recursive_timed_mutex_summary(raw)
    if "timed_mutex" in type_name:
        return _timed_mutex_summary(raw)
    return _native_mutex_summary(raw)


def _lock_pointer_summary(pointer_value):
    return f"mutex={_hex_or_null(_value_address(pointer_value))}" if pointer_value.IsValid() else None


def _single_lock_child(valobj, name, value):
    if not value.IsValid():
        return None
    if _is_pointer_type(value.GetType()):
        return _dereference_pointer(valobj, name, value)
    return _create_named_value(valobj, name, value)


def _scoped_lock_mutex_count(raw):
    direct_mutex = raw.GetChildMemberWithName("__m_")
    if direct_mutex.IsValid():
        return 1
    tuple_mutexes = []
    _collect_members_named(raw.GetChildMemberWithName("__t_"), "__value_", tuple_mutexes, depth=8)
    return len(tuple_mutexes)


class LockLikeSyntheticProvider:
    """Expose the underlying mutex for lock wrappers when LLDB can recover it."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._children = []
        type_name = raw.GetType().GetName() or ""

        if "scoped_lock" in type_name:
            self._collect_scoped_lock_children(raw)
            return

        mutex_value = _first_valid(
            raw.GetChildMemberWithName("__m_"),
            raw.GetChildMemberWithName("_mutex"),
        )
        child = _single_lock_child(self.valobj, "mutex", mutex_value)
        if child is not None and child.IsValid():
            self._children.append(child)

    def _collect_scoped_lock_children(self, raw):
        direct_mutex = raw.GetChildMemberWithName("__m_")
        if direct_mutex.IsValid():
            child = _single_lock_child(self.valobj, "mutex[0]", direct_mutex)
            if child is not None and child.IsValid():
                self._children.append(child)
            return

        tuple_mutexes = []
        _collect_members_named(raw.GetChildMemberWithName("__t_"), "__value_", tuple_mutexes, depth=8)
        for index, mutex_value in enumerate(tuple_mutexes):
            child = _single_lock_child(self.valobj, f"mutex[{index}]", mutex_value)
            if child is not None and child.IsValid():
                self._children.append(child)

    def has_children(self):
        return bool(self._children)

    def num_children(self):
        return len(self._children)

    def get_child_index(self, name):
        for index, child in enumerate(self._children):
            if child.IsValid() and child.GetName() == name:
                return index
        return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self._children):
            return None
        return self._children[index]


def lock_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    type_name = raw.GetType().GetName() or ""

    if "LoggedLock<" in type_name:
        owns_lock = bool(_get_uint(raw.GetChildMemberWithName("_owns_lock")))
        name = _value_text(raw.GetChildMemberWithName("_name"))
        mutex_ptr = raw.GetChildMemberWithName("_mutex")
        return _summary_join(
            (
                f"owns_lock={_bool_text(owns_lock)}",
                _lock_pointer_summary(mutex_ptr),
                f"name={name}" if name and name != '"<none>"' else None,
            )
        )

    if "unique_lock<" in type_name:
        owns_lock = bool(_get_uint(raw.GetChildMemberWithName("__owns_")))
        mutex_ptr = raw.GetChildMemberWithName("__m_")
        return _summary_join(
            (
                "mode=exclusive",
                f"owns_lock={_bool_text(owns_lock)}",
                _lock_pointer_summary(mutex_ptr),
            )
        )

    if "shared_lock<" in type_name:
        owns_lock = bool(_get_uint(raw.GetChildMemberWithName("__owns_")))
        mutex_ptr = raw.GetChildMemberWithName("__m_")
        return _summary_join(
            (
                "mode=shared",
                f"owns_lock={_bool_text(owns_lock)}",
                _lock_pointer_summary(mutex_ptr),
            )
        )

    if "lock_guard<" in type_name:
        mutex_ref = raw.GetChildMemberWithName("__m_")
        return _summary_join(("owns_lock=true", _lock_pointer_summary(mutex_ref)))

    if "scoped_lock<" in type_name:
        mutex_count = _scoped_lock_mutex_count(raw)
        return _summary_join(
            (
                "owns_lock=true",
                f"mutexes={mutex_count}" if mutex_count else "mutexes=?",
            )
        )

    return "lock"


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

    for regex in _BFRAME_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_OPTIONAL_CATEGORY} "
            f"--python-function {__name__}.bframe_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_OPTIONAL_CATEGORY} "
            f"--python-class {__name__}.EmptySyntheticProvider "
            f"--regex {regex}"
        )

    for regex in _OPTIONAL_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_OPTIONAL_CATEGORY} "
            f"--python-function {__name__}.optional_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_OPTIONAL_CATEGORY} "
            f"--python-class {__name__}.EmptySyntheticProvider "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_OPTIONAL_CATEGORY}")

    for regex in _MUTEX_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_MUTEX_CATEGORY} "
            f"--python-function {__name__}.mutex_summary "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_MUTEX_CATEGORY}")

    for regex in _LOCK_REGEXES:
        debugger.HandleCommand(
            f"type summary add --category {_LOCK_CATEGORY} "
            f"--python-function {__name__}.lock_summary "
            f"--regex {regex}"
        )
        debugger.HandleCommand(
            f"type synthetic add --category {_LOCK_CATEGORY} "
            f"--python-class {__name__}.LockLikeSyntheticProvider "
            f"--regex {regex}"
        )
    debugger.HandleCommand(f"type category enable {_LOCK_CATEGORY}")

    print("[LLDB] IllegalArray, robin_hood, sherwood, std::expected, optional, and mutex pretty-printers loaded.")
