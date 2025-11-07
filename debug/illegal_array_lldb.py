"""
LLDB visualizer for cmn::IllegalArray<T>.

Drop this script somewhere in the source tree (here under commons/debug) and
import it from LLDB to get collection-style summaries and element expansion.
"""

import lldb

_REGEX = r"^cmn::IllegalArray<.+>$"
_CATEGORY = "trex_illegal_array"


class IllegalArraySyntheticProvider:
    """Pretty-printer that exposes IllegalArray elements as synthetic children."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.update()

    def update(self):
        raw = self.valobj.GetNonSyntheticValue()
        self._ptr = raw.GetChildMemberWithName("_ptr")
        size_child = raw.GetChildMemberWithName("_size")
        self._size = size_child.GetValueAsUnsigned(0) if size_child.IsValid() else 0
        if self._ptr.IsValid():
            self._elem_type = self._ptr.GetType().GetPointeeType()
            self._elem_size = self._elem_type.GetByteSize()
        else:
            self._elem_type = None
            self._elem_size = 0

    def has_children(self):
        return self._ptr.IsValid() and bool(self._size)

    def num_children(self):
        return self._size

    def get_child_at_index(self, index):
        if not self._ptr.IsValid() or index < 0 or index >= self._size:
            return None
        offset = index * self._elem_size
        return self._ptr.CreateChildAtOffset(f"[{index}]", offset, self._elem_type)


def illegal_array_summary(valobj, _dict):
    raw = valobj.GetNonSyntheticValue()
    size_child = raw.GetChildMemberWithName("_size")
    cap_child = raw.GetChildMemberWithName("_capacity")
    size = size_child.GetValueAsUnsigned(0) if size_child.IsValid() else 0
    capacity = cap_child.GetValueAsUnsigned(0) if cap_child.IsValid() else 0
    return f"size={size}, capacity={capacity}"


def __lldb_init_module(debugger, _dict):
    """Invoked automatically when the script is imported via `command script import`."""
    debugger.HandleCommand(
        f"type summary add --category {_CATEGORY} "
        f"--python-function {__name__}.illegal_array_summary "
        f"--regex {_REGEX}"
    )
    debugger.HandleCommand(
        f"type synthetic add --category {_CATEGORY} "
        f"--python-class {__name__}.IllegalArraySyntheticProvider "
        f"--regex {_REGEX}"
    )
    debugger.HandleCommand(f"type category enable {_CATEGORY}")
    print("[LLDB] IllegalArray pretty-printer loaded.")
