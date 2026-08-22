// IDAPython script — run in IDA with the unpacked binary loaded.
// Dumps the skeleton function call chain around the comp+0x1A8 bone array access.
// This tells us exactly how animated bones flow from the skinning context to the render.

#include <idc.idc>
#include <ida.ida>

static main() {
    auto name, ea, seg_start, seg_end;

    // ═══ PART 1: Find references to bone hash values ═══
    // These hashes are embedded as immediate values in the binary.
    // The function that uses them is the skeleton evaluator.
    auto bone_hashes[];
    bone_hashes[0]  = 0xDED10611;  // BH_PELVIS
    bone_hashes[1]  = 0xA9CEFD4A;  // BH_HEAD
    bone_hashes[2]  = 0x07C159A2;  // BH_NECK
    bone_hashes[3]  = 0x8023796D;  // BH_SPINE
    bone_hashes[4]  = 0x22E53C03;  // BH_ROOT

    auto hash_names[];
    hash_names[0] = "BH_PELVIS";
    hash_names[1] = "BH_HEAD";
    hash_names[2] = "BH_NECK";
    hash_names[3] = "BH_SPINE";
    hash_names[4] = "BH_ROOT";

    auto found_funcs[];
    auto nfound = 0;

    for (auto hi = 0; hi < 5; hi++) {
        auto hval = bone_hashes[hi];
        // Search all segments for this immediate value
        auto seg_idx = 0;
        while (1) {
            seg_start = get_segm_by_idx(seg_idx);
            if (seg_start == BADADDR) break;
            seg_end = get_segm_end(seg_start);

            ea = seg_start;
            while (ea < seg_end && ea != BADADDR) {
                // Check if current instruction references this immediate
                auto prev_head = prev_head(ea, 0);
                if (prev_head != BADADDR) {
                    auto imm = get_operand_value(prev_head, 1);
                    if (imm == hval) {
                        // Found a reference — find containing function
                        auto func_ea = get_func_attr(prev_head, FUNCATTR_START);
                        if (func_ea != BADADDR) {
                            auto func_name = get_func_name(func_ea);
                            msg("[BONE-HASH] %s (0x%08X) referenced at %a in func %a (%s)\n",
                                hash_names[hi], hval, prev_head, func_ea, func_name);
                            // Check if this function is already found
                            auto dup = 0;
                            for (auto fi = 0; fi < nfound; fi++) {
                                if (found_funcs[fi] == func_ea) { dup = 1; break; }
                            }
                            if (!dup && nfound < 100) {
                                found_funcs[nfound++] = func_ea;
                            }
                        }
                    }
                }
                ea = next_head(ea, seg_end);
            }
            seg_idx++;
        }
    }

    msg("\n=== BONE HASH FUNCTIONS: %d unique functions found ===\n", nfound);

    // ═══ PART 2: For each function, dump the call chain ═══
    for (auto fi = 0; fi < nfound; fi++) {
        auto func_ea = found_funcs[fi];
        auto func_name = get_func_name(func_ea);
        msg("\n--- Function %d: %a (%s) ---\n", fi, func_ea, func_name);

        // Dump first 50 instructions
        auto inst_ea = func_ea;
        auto count = 0;
        while (inst_ea != BADADDR && count < 50) {
            auto disasm = generate_disasm_line(inst_ea, 0);
            msg("  %a: %s\n", inst_ea, disasm);

            // If it's a CALL, note the target
            auto mnem = print_insn_mnem(inst_ea);
            if (mnem == "call") {
                auto target = get_operand_value(inst_ea, 0);
                if (target != BADADDR) {
                    auto tname = get_name(target);
                    msg("    -> CALL TARGET: %a (%s)\n", target, tname);
                }
            }

            // If it loads from [reg+1A8h], that's the bone array pointer
            auto op0 = print_operand(inst_ea, 0);
            auto op1 = print_operand(inst_ea, 1);
            if (strstr(op1, "1A8") != -1 || strstr(op1, "+1A8h") != -1) {
                msg("    ** BONE ARRAY ACCESS at offset +0x1A8 **\n");
            }
            if (strstr(op1, "+0x30") != -1 || strstr(op1, "+30h") != -1) {
                msg("    ** STRIDE 0x30 ACCESS (48 bytes) **\n");
            }

            inst_ea = next_head(inst_ea, BADADDR);
            count++;
        }
    }

    // ═══ PART 3: Find cross-references to the skeleton function ═══
    msg("\n=== PART 3: Cross-references to skeleton functions ===\n");
    for (auto fi = 0; fi < nfound; fi++) {
        auto func_ea = found_funcs[fi];
        auto xr = get_first_cref_to(func_ea);
        while (xr != BADADDR) {
            auto caller_func = get_func_attr(xr, FUNCATTR_START);
            if (caller_func != BADADDR) {
                auto caller_name = get_func_name(caller_func);
                msg("  XREF to %a from %a (%s)\n", func_ea, xr, caller_name);
            }
            xr = get_next_cref_to(func_ea, xr, 0);
        }
    }

    // ═══ PART 4: Find the registry/skinning context offset ═══
    // Search for patterns that read from comp+N then dereference to palette
    msg("\n=== PART 4: Looking for comp+0x1A8 dereference pattern ===\n");
    // Pattern: mov rax, [reg+1A8h] ; which is REX.W 8B 83 1A800000 for rbx
    // Or: mov rdi, [rsi+1A8h]   ; REX.W 8B BB 1A800000

    ea = 0;
    auto pat_1A8[] = { 0x8B, 0x??, 0xA8, 0x01, 0x00, 0x00 }; // [reg+1A8h]
    // Search for mov reg, [reg+1A8h]
    seg_idx = 0;
    while (1) {
        seg_start = get_segm_by_idx(seg_idx);
        if (seg_start == BADADDR) break;
        seg_end = get_segm_end(seg_start);

        for (ea = seg_start; ea < seg_end - 7; ea = next_head(ea, seg_end)) {
            auto b0 = get_wide_byte(ea);
            auto b1 = get_wide_byte(ea + 1);
            // REX.W prefix + MOV r64, [r64+disp32]
            if ((b0 & 0x48) == 0x48 && b1 == 0x8B) {
                auto modrm = get_wide_byte(ea + 2);
                auto mod = (modrm >> 6) & 3;
                auto rm = modrm & 7;
                if (mod == 2 && rm != 4) { // [reg+disp32]
                    auto disp = get_wide_dword(ea + 3);
                    if (disp == 0x1A8) {
                        auto func_name = get_func_name(get_func_attr(ea, FUNCATTR_START));
                        msg("  +0x1A8 access at %a in func %s\n", ea, func_name);

                        // Dump context: 10 insns before and after
                        auto ctx = prev_head(ea, 0);
                        for (auto ci = 0; ci < 5 && ctx != BADADDR; ci++) {
                            ctx = prev_head(ctx, 0);
                        }
                        if (ctx == BADADDR) ctx = ea;
                        for (auto ci = 0; ci < 15; ci++) {
                            msg("    %a: %s\n", ctx, generate_disasm_line(ctx, 0));
                            ctx = next_head(ctx, BADADDR);
                        }
                    }
                }
            }
        }
        seg_idx++;
    }

    msg("\n=== IDA ANALYSIS COMPLETE ===\n");
}
