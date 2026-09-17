"""Locate functions that reference the corrected R6 head and neck hashes.

Run this file with IDAPython while the unpacked RainbowSix.exe image is open.
"""

import idaapi
import ida_funcs
import ida_segment
import ida_ua


# Keep these synchronized with BipedBoneID in r6_bone_ids.h. 0xA9CEFD4A is
# BONE_CAMERANODE, not the head.
HEAD_HASH = 0x07C159A2
NECK_HASH = 0x8023796D

# Maximum distance between the two hash references.
PAIR_MAX_DIST = 0x100


def find_imm32_xrefs(target):
    """Return each .text instruction whose immediate operand equals target."""

    segment = ida_segment.get_segm_by_name(".text")
    if segment is None:
        print("[!] .text segment not found")
        return []

    hits = []
    ea = segment.start_ea

    while ea < segment.end_ea:
        instruction = ida_ua.insn_t()
        size = ida_ua.decode_insn(instruction, ea)
        if size <= 0:
            ea += 1
            continue

        for operand in instruction.ops:
            if operand.type == ida_ua.o_void:
                break
            if operand.type == ida_ua.o_imm and (operand.value & 0xFFFFFFFF) == target:
                hits.append(ea)
                break

        ea += size

    return hits


def find_containing_function(ea):
    """Return the start of the function containing ea, if IDA knows it."""

    function = ida_funcs.get_func(ea)
    return None if function is None else function.start_ea


def main():
    print("[*] Searching .text for corrected bone hashes...")

    head_hits = find_imm32_xrefs(HEAD_HASH)
    neck_hits = find_imm32_xrefs(NECK_HASH)

    print("[+] HEAD hash 0x%08X: %d hit(s)" % (HEAD_HASH, len(head_hits)))
    print("[+] NECK hash 0x%08X: %d hit(s)" % (NECK_HASH, len(neck_hits)))

    if not head_hits:
        print("[!] No HEAD hash references found.")
    if not neck_hits:
        print("[!] No NECK hash references found.")
    if not head_hits or not neck_hits:
        return

    pairs = []
    for head_ea in head_hits:
        for neck_ea in neck_hits:
            if abs(head_ea - neck_ea) <= PAIR_MAX_DIST:
                pairs.append((head_ea, neck_ea))

    print(
        "\n[+] %d (HEAD, NECK) pair(s) within 0x%X bytes:"
        % (len(pairs), PAIR_MAX_DIST)
    )
    if not pairs:
        print("    (none - check hash values and .text bounds)")
        return

    seen_functions = {}
    for head_ea, neck_ea in pairs:
        head_function = find_containing_function(head_ea)
        neck_function = find_containing_function(neck_ea)

        if head_function is None or neck_function is None:
            print(
                "    [-] pair @ 0x%X / 0x%X has no containing function"
                % (head_ea, neck_ea)
            )
            continue
        if head_function != neck_function:
            print(
                "    [-] pair @ 0x%X / 0x%X spans two functions (skip)"
                % (head_ea, neck_ea)
            )
            continue

        seen_functions.setdefault(head_function, []).append((head_ea, neck_ea))

    print(
        "\n[+] %d unique function(s) referencing both hashes:"
        % len(seen_functions)
    )
    for function_ea, function_pairs in sorted(seen_functions.items()):
        name = ida_funcs.get_func_name(function_ea) or "<unnamed>"
        print(
            "    fn 0x%X  %s   (%d pair%s)"
            % (
                function_ea,
                name,
                len(function_pairs),
                "" if len(function_pairs) == 1 else "s",
            )
        )
        for head_ea, neck_ea in function_pairs:
            print(
                "        HEAD imm @ 0x%X    NECK imm @ 0x%X    dist=%d"
                % (head_ea, neck_ea, abs(head_ea - neck_ea))
            )

    if len(seen_functions) == 1:
        function_ea = next(iter(seen_functions))
        print("\n[+] Jumping to bone-lookup function @ 0x%X" % function_ea)
        idaapi.jumpto(function_ea)


if __name__ == "__main__":
    main()
