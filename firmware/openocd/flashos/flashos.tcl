# FlashOS SRAM algorithm driver, shared by the N32H7xx configs.
#
# This is the machinery copy-pasted into n32h7xx_{cm7,cm4}_cmsisdap.cfg, with the
# target-specific parts pulled out into two procs the config calls:
#
#     flashos_setup    <algo base>                     -- where the blob goes
#     flashos_geometry <base> <size> <sector>          -- what the flash is
#
# A config calls both at load time, next to each other, where a typo in the size
# is a parse error rather than a surprise on a target. Neither touches hardware,
# so this works before `init`.
#
# The geometry reaches the algorithm through a parameter block in SRAM that
# flashos_geometry *describes* and flashos_init *writes*, and that every entry
# point receives the address of -- see FlashPrg.c. Nothing about the flash's
# shape is compiled into the blob, so the CM7 and CM4 configs no longer have to
# agree about it (they did not: the CM7 config declared a 2 MB part and the CM4
# config a 4 MB one, against a single binary that had one of the two baked in).
#
# The entry offsets come from flashop_sym.tcl, generated from the linked ELF by
# gen_sym.py. They used to be hand-copied into each config, twice.
#
# The dual-core config has no copy of any of this and needs none: it does no
# flashing on purpose, because a second openocd cannot open the same probe.

if {[catch {source [file join [file dirname [info script]] flashop_sym.tcl]} msg]} {
    error "flashos.tcl: cannot load flashop_sym.tcl ($msg) -- run 'make' in [file dirname [info script]]"
}

set ::FLASHPRG_ALGO_BIN [file join [file dirname [info script]] flashop.bin]

# Parameter block magic and version, mirroring FlashPrg.c. Spelled out here
# rather than derived so that a mismatch is a visible edit on both sides.
set ::FLASHPRG_PARAM_MAGIC   0x47525046
set ::FLASHPRG_PARAM_VERSION 1
set ::FLASHPRG_PARAM_WORDS   9

# The `size` field, in BYTES: FlashPrg.c compares it against sizeof(FlashPrg_Params).
# This is the one field whose unit is not obvious from the block, and writing the
# word count here instead is exactly the mistake that was made -- every entry point
# answered "bad block" on real hardware while the 39 host-side checks passed,
# because the writer and the reader were each internally consistent and only
# disagreed with the C. The check below is what makes that a load-time error.
set ::FLASHPRG_PARAM_SIZE_BYTES 36
if {$::FLASHPRG_PARAM_SIZE_BYTES != 4 * $::FLASHPRG_PARAM_WORDS} {
    error "flashos.tcl: FLASHPRG_PARAM_SIZE_BYTES ($::FLASHPRG_PARAM_SIZE_BYTES) is not 4 x FLASHPRG_PARAM_WORDS ($::FLASHPRG_PARAM_WORDS)"
}

# Geometry defaults; a config overrides these before calling flashos_setup.
set ::FLASH_BASE        0x15000000
set ::FLASH_SIZE        0
set ::FLASH_SECTOR_SIZE 0x1000

# ---------------------------------------------------------------- address map

# Everything the algorithm needs at a fixed address, derived from the base so the
# same table serves the CM7 image at 0x24000000 and the CM4 image at 0x30000000.
# The shape is the one the configs used to spell out by hand:
#
#     base + 0                    .. blob
#     base + round16(blob size)   .. parameter block
#     base + 0xFE0                    stack top, growing down
#     base + 0xFF0                    BKPT that returns from a call
#     base + 0x1000 .. +0x20000       two 62 KB image buffers
#
# The 0xFE0/0xFF0/0x1000 offsets are the layout the configs have always used and
# are kept as they were; only the buffers' length is ours to choose. It was one
# 16 KB buffer, which is 33 chunks for the 526596-byte image, and every chunk
# costs two debug calls (program and verify) plus the enter/leave around them, at
# ~18 ms of round trip each. 32 KB took that to 17 chunks; the size below is what
# is left when both buffers are as large as the 128 KB region allows, which is 9.
#
# The bound is the RAM, not anything about the part: two buffers plus the 0x1000
# header have to come to at most the 0x20000 the target's linker script gives as
# AXI_SRAM, so each is (0x20000 - 0x1000) / 2 = 0xF800. That is *tighter* than
# the other thing worth watching -- the ROM write routine takes its length in a
# field whose width is not documented, and 0xFFFF is the first value that could
# not fit in 16 bits -- so the field width does not actually bind here. It would
# if the target had more RAM, and that is when to go and find out.
#
# Two, not one, because flash_program_bin runs a program call and the staging of
# the next chunk concurrently: they use different resources -- the target's XSPI
# controller and the SWD link -- and the flash write is the shorter of the two, so
# the staging of chunk k+1 is done inside the program call for chunk k. That needs
# somewhere to put chunk k+1 that is not the buffer being programmed from.
proc flashos_setup {base} {
    set ::FLASH_ALGO_BASE $base

    foreach name {Init UnInit EraseChip EraseSector EraseRange ProgramPage Verify FlashSum} {
        # + 1 because the symbol table carries the code address and the core needs
        # the Thumb bit set to be willing to branch there.
        set ::FOS_$name [expr {$base + [set ::FLASHPRG_OFF_$name] + 1}]
    }

    set ::FOS_PARAM_ADDR [expr {$base + (($::FLASHPRG_BLOB_SIZE + 15) & ~15)}]
    set ::FOS_STACK_TOP  [expr {$base + 0xFE0}]
    set ::FOS_BKPT_ADDR  [expr {$base + 0xFF0}]
    set ::FOS_BUF_ADDR   [expr {$base + 0x1000}]
    set ::FOS_BUF_SIZE   0xF800
    set ::FOS_BUF2_ADDR  [expr {$::FOS_BUF_ADDR + $::FOS_BUF_SIZE}]

    # Catch a grown blob or a shrunken reservation here, where the numbers are,
    # rather than as a corrupted parameter block on a target.
    set param_end [expr {$::FOS_PARAM_ADDR + 4 * $::FLASHPRG_PARAM_WORDS}]
    if {$param_end > $::FOS_STACK_TOP} {
        error "flashos_setup: parameter block ends at [format 0x%X $param_end], past the stack top [format 0x%X $::FOS_STACK_TOP]"
    }

    # The buffers are the one part of this layout that grows, so they are the one
    # part that can grow past the end of the RAM they are in -- which on a target
    # means the driver staging bytes over whatever lives there. The address is the
    # base's business rather than this proc's, so the size of the region is a
    # parameter the caller sets from the target's memory map.
    #
    # 128 KB is a default, not a fact about any part. Picking the region is the
    # caller's job and it is not a matter of finding free-looking addresses: on the
    # N32H787 every SRAM the CM7 can see is also reachable by the CM4 or its DMA
    # engines, and both of the regions this layout was first given were quietly
    # being written by one of them. Occupancy that nothing observes is not
    # occupancy. The rule that survived is the one the H787 cfg now follows: stage
    # in memory that belongs to the core running the algorithm and to nothing else,
    # which on the CM7 means its DTCM. See n32h787_flash.cfg for the two regions
    # that failed and how, and README.md for the measurement that found it.
    if {![info exists ::FOS_SRAM_BYTES]} {
        set ::FOS_SRAM_BYTES 0x20000
    }
    set buf_end [expr {$::FOS_BUF2_ADDR + $::FOS_BUF_SIZE}]
    if {$buf_end > $base + $::FOS_SRAM_BYTES} {
        error "flashos_setup: buffers end at [format 0x%X $buf_end], past the [format 0x%X [expr {$base + $::FOS_SRAM_BYTES}]] end of a [expr {$::FOS_SRAM_BYTES / 1024}] KB region"
    }
}

# ------------------------------------------------------- D-cache maintenance

# The N32H787's Cortex-M7 runs with its D-cache enabled (CCR.DC, bit 16), and a
# debug-port write does not snoop it. The AP updates SRAM; the core goes on
# reading its own cached copy of that line. So every address this driver writes
# into SRAM has to be dropped from the core's cache as well, or the algorithm
# runs against a parameter block and an image buffer that the debugger reads
# back perfectly and the CPU cannot see.
#
# This is not subtle in practice. With the block written and `mrw` reading it
# back field for field correct, Init answered 1 for every entry point; clearing
# CCR.DC made it answer 0 immediately. Writing a deliberately corrupt magic
# through the AP changed nothing the core could observe, which is the staleness
# measured directly.
#
# DCIMVAC is 0xE000EF5C on the ARMv7-M SCB: the written value is an MVA and the
# line covering it is dropped. Verified against the part rather than taken from
# the manual -- a bad magic written through the AP stayed invisible until one
# write per line landed on DCIMVAC, after which Init saw it.
set ::SCB_DCIMVAC      0xE000EF5C
set ::CORTEX_DHCSR     0xE000EDF0
set ::DCACHE_LINE_SIZE 32

# Drop every line covering [addr, addr+size). Cheap enough to be unconditional:
# 2 writes for the parameter block, 12 for the blob, and 512 per 16 KB chunk,
# which is a few tenths of a second against a ~12 s write. Harmless on a target
# whose cache is off -- the instruction is then a no-op rather than wrong.
proc _dcache_invalidate {addr size} {
    # With CCR.DC (0xE000ED14 bit 16) clear there is no D-cache to snoop: AP
    # writes are visible to the core immediately. This part also answers the
    # by-MVA maintenance registers with a bus fault while the cache is off
    # (seen when flashing firmware that runs with the cache disabled, e.g. the
    # first flash over a TCM-partition switch), so the "no-op when off" path
    # must skip the writes entirely. Everything this driver invalidates lives
    # in DTCM, which is not cacheable in any configuration.
    if {([catch {mrw 0xe000ed14} ccr] == 0) && ($ccr & 0x00010000) == 0} {
        return
    }
    set end [expr {$addr + $size}]
    for {set a [expr {$addr & ~($::DCACHE_LINE_SIZE - 1)}]} {$a < $end} \
        {incr a $::DCACHE_LINE_SIZE} {
        mww $::SCB_DCIMVAC $a
    }
    # AP writes are posted, so drain the queue with a read before anything
    # depends on the maintenance having completed.
    mrw $::CORTEX_DHCSR
}

# ------------------------------------------------------------ the parameters

# Declare what the flash is. Validates and records; writes nothing, so a config
# can call this at load time. Anything that runs the algorithm writes the block
# first through _flashos_write_geometry.
proc flashos_geometry {base size sector} {
    if {![info exists ::FOS_PARAM_ADDR]} {
        error "flashos_geometry: call flashos_setup first"
    }
    if {$base & 0x00FFFFFF} {
        error "flashos_geometry: base [format 0x%08X $base] is not on a 16 MB boundary"
    }
    if {$size <= 0 || $sector <= 0} {
        error "flashos_geometry: size and sector must be positive (got $size, $sector)"
    }
    if {($size % $sector) != 0} {
        error "flashos_geometry: size $size is not a whole number of $sector byte sectors"
    }
    set mask  0xFF000000
    set match [expr {$base & 0xFF000000}]

    # The mask only covers the top 8 bits, so the whole device has to fit inside
    # the 16 MB window those bits select. A part that ran past it would have its
    # upper addresses rejected by the algorithm's own check, as errors on the
    # last few sectors of an otherwise successful write.
    if {[expr {($base + $size) & $mask}] != $match} {
        error "flashos_geometry: base [format 0x%08X $base] + size [format 0x%X $size] leaves the 16 MB window the address mask covers"
    }

    set ::FLASH_PARAM_MASK  $mask
    set ::FLASH_PARAM_MATCH $match
    set ::FLASH_BASE        $base
    set ::FLASH_SIZE        $size
    set ::FLASH_SECTOR_SIZE $sector

    puts "Flash geometry: base [format 0x%08X $base] size [format 0x%X $size] sector [format 0x%X $sector]"
    return
}

# Write the block flashos_geometry declared, at FOS_PARAM_ADDR. 36 bytes, one
# word per mww, which is why FlashPrg_Params is all uint32_t.
#
# Called before every use of the algorithm rather than once at setup time: the
# block lives in SRAM, which survives a core reset but not a re-run of this
# script, and Init is cheap enough that re-asserting what the host believes is
# worth more than the nine writes. It is what makes the algorithm's rejection of
# a stale block meaningful -- a mismatch means the two sides genuinely disagree,
# not that someone forgot to call something.
proc _flashos_write_geometry {} {
    if {![info exists ::FLASH_BASE]} {
        error "flashos: no geometry declared -- call flashos_geometry"
    }
    set words [list $::FLASHPRG_PARAM_MAGIC $::FLASHPRG_PARAM_VERSION \
                     $::FLASHPRG_PARAM_SIZE_BYTES \
                     $::FLASH_BASE $::FLASH_SIZE $::FLASH_SECTOR_SIZE \
                     $::FLASH_PARAM_MASK $::FLASH_PARAM_MATCH 0]

    set i 0
    foreach w $words {
        mww [expr {$::FOS_PARAM_ADDR + 4 * $i}] $w
        incr i
    }

    # The algorithm reads this block back as data, so the core's copy of those
    # lines has to go -- see the D-cache notes above.
    _dcache_invalidate $::FOS_PARAM_ADDR [expr {4 * $::FLASHPRG_PARAM_WORDS}]
}

# Read the block back, field by field. Only used on the failure path, so it costs
# nothing when things work -- but it is the whole diagnosis when they do not, and
# it needs a halted target, which is why it is not part of the write.
proc _flashos_dump_geometry {} {
    set names {magic version size base size sector mask match flags}
    set i 0
    foreach n $names {
        set a [expr {$::FOS_PARAM_ADDR + 4 * $i}]
        if {[catch {set v [mrw $a]} msg]} {
            puts "    +[format %02X [expr {4 * $i}]] $n = <read failed: $msg>"
        } else {
            puts "    +[format %02X [expr {4 * $i}]] $n = [format 0x%08X $v]"
        }
        incr i
    }
}

# ------------------------------------------------------------------ plumbing

proc _safe_load_image {file addr} {
    if {[catch {load_image $file $addr} msg]} {
        puts "  load_image Fail: $msg"
        puts "  Please disconnect and reconnect the debugger, then try again,"
        error $msg
    }
}

proc flashos_load {} {
    if {![file exists $::FLASHPRG_ALGO_BIN]} {
        error "Flash algorithm binary not found: $::FLASHPRG_ALGO_BIN"
    }
    # The offset table and the image have to be the same build, or the config
    # would call entry points belonging to a different blob. The size is what
    # gen_sym.py recorded when it read the symbol table.
    set actual [file size $::FLASHPRG_ALGO_BIN]
    if {$actual != $::FLASHPRG_BLOB_SIZE} {
        error "$::FLASHPRG_ALGO_BIN is $actual bytes but flashop_sym.tcl describes a $::FLASHPRG_BLOB_SIZE byte image -- rerun 'make' in [file dirname [info script]]"
    }

    puts "Loading [file tail $::FLASHPRG_ALGO_BIN] to [format 0x%08X $::FLASH_ALGO_BASE]..."
    _safe_load_image $::FLASHPRG_ALGO_BIN $::FLASH_ALGO_BASE

    # The blob's literal pool is data the core reads, so it needs the same
    # treatment as the parameter block. Cover the block too: it sits immediately
    # after the image and may share its last cache line.
    _dcache_invalidate $::FLASH_ALGO_BASE \
        [expr {($::FOS_PARAM_ADDR - $::FLASH_ALGO_BASE) + 4 * $::FLASHPRG_PARAM_WORDS}]

    set first_word [mrw $::FLASH_ALGO_BASE]
    puts "Algorithm @ [format 0x%08X $::FLASH_ALGO_BASE]: [format 0x%08X $first_word]"
    if {$first_word == 0 || $first_word == 0xFFFFFFFF} {
        error "Flash algorithm not loaded correctly: first word = [format 0x%08X $first_word]"
    }
}

# OpenOCD >= 0.12 lowercases register names ("xpsr"), older versions use
# "xPSR". Probed once (target must be halted) and cached.
set ::XPSR_REG ""
proc _xpsr_regname {} {
    if {$::XPSR_REG eq ""} {
        if {![catch {reg xPSR}]} {
            set ::XPSR_REG xPSR
        } else {
            set ::XPSR_REG xpsr
        }
    }
    return $::XPSR_REG
}

# Call a FlashOS function in SRAM: arm the BKPT, set SP/args/LR/PC, resume, and
# read the result from R0 once the function lands on the BKPT.
# All flash functions return 0 = OK, 1 = Failed.
#
# BKPT and LR must be re-armed before EVERY call: the ROM-calling wrappers return
# to the *saved* entry LR, so a stale LR (ROM leftovers, or 0xFFFFFFFF /
# EXC_RETURN right after reset) HardFaults.
#
# Four arguments is the ceiling, not a target: the ARM EABI passes them in R0-R3
# and a fifth would have to go on the stack. An earlier version of this proc
# silently dropped anything past the fourth; it errors now, because every call
# below is written to fit and a silent drop would mean calling a function with
# the wrong argument count.
#
# Split into enter/leave so a caller can put work in the gap between the resume
# and the result. The target halts on the BKPT by itself, so looking away for a
# while costs nothing -- and `resume` returns as soon as the core is running, so
# the gap starts immediately. flash_program_bin is the only user: it stages the
# next chunk of the image into the other buffer while the flash write runs.
proc _flashos_call_enter {func_addr args} {
    if {[llength $args] > 4} {
        error "flashos_call: [format 0x%08X $func_addr] called with [llength $args] arguments, at most 4 can be passed"
    }

    mww $::FOS_BKPT_ADDR 0x0000BE00
    reg sp $::FOS_STACK_TOP

    # ARM EABI: first 4 args in R0-R3, the rest zeroed
    set regs {r0 r1 r2 r3}
    set i 0
    foreach val $args {
        reg [lindex $regs $i] $val
        incr i
    }
    for {} {$i < 4} {incr i} {
        reg [lindex $regs $i] 0
    }

    reg lr [expr {$::FOS_BKPT_ADDR | 1}]

    # T=1 is mandatory: "reset halt" leaves the core with xPSR.T=0, and the first
    # fetch would fault (INVSTATE) before the function runs.
    reg [_xpsr_regname] 0x01000000

    # Interrupts off for the duration of the call, restored by _flashos_call_leave.
    #
    # This is not belt and braces. The algorithm is loaded into the bottom of AXI
    # SRAM and the buffers extend to the top of it, and measuring where the target
    # application actually keeps its RAM (dump 0x24000000 for 128 KB while halted)
    # shows that both of those overlap it: the application's data lives in
    # 0x24000000..0x2400DFFF, which is where this blob and its stack go, and its
    # stack lives in 0x2401E000..0x2401FFFF, which is the last 8 KB of buffer 2.
    # The application is not running -- we resumed into our own routine -- but its
    # interrupt handlers still are, and they run against a .data segment that has
    # been overwritten with our blob. An interrupt landing in this window is
    # therefore an application handler executing with corrupted globals and
    # pushing onto a stack that the host is staging image bytes into.
    #
    # That is the shape of the corruption this was written for: at 5000 kHz one
    # flash run in several came back with a couple of hundred wrong bytes in two
    # sectors, and those two sectors are exactly the top 8 KB of buffer 2, i.e.
    # exactly the application's stack. Masking interrupts here removes the only
    # way anything but this routine can execute while it runs.
    #
    # PRIMASK, not the debug halt mask: it has to survive the resume. It is a
    # core register write rather than a memory one because ARMv7-M keeps PRIMASK
    # separate from xPSR.
    reg primask 1

    reg pc $func_addr
    resume
}

proc _flashos_call_leave {} {
    wait_halt 15000                       ;# generous: covers flash erase

    # Back to what the application expects before anything else runs again. The
    # core is halted between calls, so nothing can execute in the gap.
    reg primask 0

    set ret_raw [reg r0]
    set ret 0
    if {![regexp {(0x[0-9a-fA-F]+)} $ret_raw -> ret_hex]} {
        set ret_hex $ret_raw
    }
    set ret [expr {$ret_hex}]
    return $ret
}

# The two halves back to back, which is what everything except flash_program_bin
# wants.
proc flashos_call {func_addr args} {
    _flashos_call_enter $func_addr {*}$args
    return [_flashos_call_leave]
}

# ---------------------------------------------------------- flash operations

# Every call below passes FOS_PARAM_ADDR, which is where flashos_geometry put the
# block the algorithm reads. That address is the only thing tying the two sides
# together; there is no compiled-in default to fall back on, by design.
proc flashos_init {} {
    _flashos_write_geometry
    set ret [flashos_call $::FOS_Init $::FOS_PARAM_ADDR 0 1]
    if {$ret != 0} {
        puts "FlashOS Init failed with return code $ret"
        # Init returns 1 in exactly one case: the block at FOS_PARAM_ADDR did not
        # validate (see params_get in FlashPrg.c). The XSPI clock is not touched
        # on that path, so there is nothing to undo.
        #
        # Print the block back rather than guessing at the cause. The first real
        # hardware run failed here and the message named the wrong culprit (a
        # mismatched blob) when the block was in fact intact and one field was in
        # the wrong unit -- the values are the evidence, so show them.
        puts "  the parameter block at [format 0x%08X $::FOS_PARAM_ADDR] was rejected:"
        _flashos_dump_geometry
        puts "  FlashPrg.c accepts it only if magic/version match, size == 4 x words"
        puts "  ($::FLASHPRG_PARAM_SIZE_BYTES), flags == 0, size and sector are nonzero,"
        puts "  size is a whole number of sectors, and base & mask == match."
        error "FlashOS Init returned $ret"
    }
    puts "FlashOS Init OK."
}

proc flashos_uninit {} {
    flashos_call $::FOS_UnInit 0
    puts "FlashOS UnInit OK."
}

# Requires flashos_init to have run in this session: EraseSector reads the same
# block Init did, and writing it here would mean nine more mww per sector, for a
# value that cannot have changed.
#
# flash_program_bin does not use this any more -- it erases its whole range in one
# EraseRange call -- so this is the single-sector entry point, for tests and for
# poking at one sector without touching its neighbours.
proc flashos_erase_sector {addr} {
    set ret [flashos_call $::FOS_EraseSector $addr $::FOS_PARAM_ADDR]
    if {$ret != 0} {
        puts "FlashOS EraseSector @ [format 0x%08X $addr] failed with return code $ret"
        error "FlashOS EraseSector @ [format 0x%08X $addr] returned $ret"
    }
}

# The target's own checksum of a range of the FLASH -- not of the buffer. See
# FlashSum in FlashPrg.c for why that distinction is the whole point of it, and
# _flashos_chunk_gate below for what it is used for here.
proc flashos_flash_sum {addr size} {
    return [flashos_call $::FOS_FlashSum $addr $size $::FOS_PARAM_ADDR]
}

# The switch the entry points pass on the command line. A proc rather than a bare
# `set ::FLASHOS_GATE 1`, because openocd prints the result of any -c command that
# has one: the set would put a stray "1" in the middle of the run, and the run's
# own output is unfiltered by default. This returns nothing.
proc flashos_gate_on {} {
    set ::FLASHOS_GATE 1
    # Explicit: a proc returns its last command's value, and that is the "1" this
    # exists to keep out of the output.
    return
}

# The byte values of a string, in order -- the one place the two openocd builds
# this has to run on are known to differ, so the check that they agree is here
# rather than at the top.
#
# `binary scan cu*` is the exact way and the fast one: 389 ms for the 551172-byte
# image. The Linux openocd has it. The Windows openocd in this package does not --
# its Jim is built without it -- and it is built without UTF-8 support as well, so
# its strings are indexed by byte, which makes `split` plus one `scan %c` per byte
# exact there. That is the fallback.
#
# THE TWO MUST NOT BE MIXED UP, and the guard is the caller's: the number of bytes
# this returns is checked against the file's size. A build that has neither
# `binary` nor byte-indexed strings -- a UTF-8 Jim without the binary extension --
# would decode multi-byte sequences into single characters and answer with fewer
# values than the file has bytes, and that is a loud error rather than sums over
# the wrong bytes.
proc _flashos_byte_values {s} {
    if {![catch {binary scan $s cu* bytes}]} {
        return $bytes
    }
    set bytes {}
    foreach ch [split $s {}] {
        scan $ch %c v
        lappend bytes $v
    }
    return $bytes
}

# The host half of the gate: one Fletcher-16 pair per chunk, over the bytes that
# chunk holds, computed the way the algorithm's FlashSum computes them on the
# target -- `a` is the running byte sum, `b` the running sum of `a`, both masked
# to 16 bits, packed (b << 16) | a. Sets ::CHUNK_SIZE and ::CHUNK_SUMS, which is
# what flash_program_bin gates on; see the block there for why the comparison is
# worth a call per chunk.
#
# WHY IT LIVES HERE AND NOT IN THE HOST SCRIPT. It used to be a python3 heredoc in
# flash.sh, and flash.bat would have had to carry its own copy of the arithmetic
# -- a gate implemented twice is a gate that agrees with itself on one platform
# only. In here it is one implementation, it runs in the interpreter openocd
# already has, and neither entry point needs a Python on PATH.
#
# The reads are byte-based, which is what _sram_load_chunk also relies on and
# checks: `read $fh $n` returns n bytes whatever they are. Slicing a chunk out of
# a Tcl string by index instead is the trap that file carries a note about.
proc flashos_gate {image {chunk_size ""}} {
    if {$chunk_size eq ""} {
        set chunk_size $::FOS_BUF_SIZE
    }
    set fh [open $image rb]
    set sums {}
    set off 0
    set nbytes 0
    while {[string length [set c [read $fh $chunk_size]]] > 0} {
        set bytes [_flashos_byte_values $c]
        incr nbytes [llength $bytes]
        set a 0
        set b 0
        foreach byte $bytes {
            set a [expr {($a + $byte) & 0xFFFF}]
            set b [expr {($b + $a) & 0xFFFF}]
        }
        set s [expr {($b << 16) | $a}]
        # FlashSum answers 0xFFFFFFFF to mean "call refused", so a chunk that
        # genuinely sums to that could not be told from a rejection. Failing here
        # is the difference between a loud error and a rare and mysterious one.
        if {$s == 0xFFFFFFFF} {
            close $fh
            error "flashos_gate: the chunk at byte offset $off of $image sums to 0xFFFFFFFF, which is FLASHPRG_SUM_ERROR -- a refusal could not be told from a real sum"
        }
        lappend sums $s
        incr off $chunk_size
    }
    close $fh
    # The guard described above. `file size` is a byte count on both builds.
    set size [file size $image]
    if {$nbytes != $size} {
        error "flashos_gate: read $size bytes of $image but summed $nbytes byte values -- this openocd indexes strings by character, so the sums would be over the wrong bytes"
    }
    set ::CHUNK_SIZE $chunk_size
    set ::CHUNK_SUMS $sums
    puts "gate   : [llength $sums] chunk checksums (0x[format %X $chunk_size] bytes each) from [file tail $image]"
}

# Erase `count` consecutive sectors in one call. Same requirement as above: the
# algorithm validates the block on every call and flashos_init is what writes it.
#
# The loop lives in the algorithm rather than here because a call is a breakpoint,
# five register writes, a resume, a halt wait and a read-back, and that costs far
# more than a sector erase does: measured on this target, ~18 ms per call against
# ~1.8 ms of actual erase. 129 sectors erased this image in 2536 ms as 129 calls
# and 245 ms as one. A zero count is legal and costs nothing.
#
# flash_program_bin does not use this any more -- it posts the same call itself, so
# that it can stage the first chunk over it instead of waiting. What is left here is
# the waited-for entry point, for a caller that wants the erase done when it returns.
proc flashos_erase_range {adr count} {
    set ret [flashos_call $::FOS_EraseRange $adr $count $::FOS_PARAM_ADDR]
    if {$ret != 0} {
        puts "FlashOS EraseRange @ [format 0x%08X $adr] x$count failed with return code $ret"
        error "FlashOS EraseRange @ [format 0x%08X $adr] x$count returned $ret"
    }
}

# Standalone entry point -- the documented way to wipe the part is
# `-c "reset init" -c "flash_mass_erase"`, which does not go through
# flash_program_bin -- so this writes the block itself rather than assuming Init
# ran. EraseChip is the one call where the walk bounds come from the block, so a
# stale one would erase the wrong length.
proc flash_mass_erase {} {
    _flashos_write_geometry
    set ret [flashos_call $::FOS_EraseChip $::FOS_PARAM_ADDR]
    if {$ret != 0} {
        puts "FlashOS EraseChip failed with return code $ret"
        error "FlashOS EraseChip returned $ret"
    }
    puts "Mass erase complete."
}

# Stage one chunk in SRAM via a temp file: a bulk load_image beats a word-by-word
# mww loop.
#
# The chunk is read out of the image at an absolute byte offset, and deliberately
# NOT sliced out of a Tcl string. Jim Tcl strings are UTF-8, so `string length`
# counts characters and `string range` indexes by character: on the 526596-byte
# image this was written for, `string length` answers 521917. Slicing by that
# count shifts every chunk by the multi-byte sequences before it and stops 4679
# bytes short of the end -- the image reaches the flash misaligned, and because
# Verify compares the same buffer it was programmed from, it agrees with itself
# and passes. Measured on this openocd: `seek` and `read $fh $n` are byte-based
# (reading n=16384 moved the file position from 16384 to 32768 while the string
# came back as 16338 characters), and `puts -nonewline` on a binary channel
# round-trips byte for byte. Going through the file keeps every offset a byte
# offset. `load_image` cannot do this itself: its optional arguments filter by
# address, not by file offset.
proc _sram_load_chunk {bin_file src_offset size dst_addr} {
    set tmp "_chunk_tmp.bin"
    set in [open $bin_file rb]
    seek $in $src_offset
    set chunk [read $in $size]
    close $in
    if {[string bytelength $chunk] != $size} {
        error "_sram_load_chunk: read [string bytelength $chunk] bytes at offset $src_offset, wanted $size"
    }
    set fh [open $tmp wb]
    fconfigure $fh -translation binary
    puts -nonewline $fh $chunk
    close $fh
    _safe_load_image $tmp $dst_addr
    file delete $tmp

    # No cache maintenance here, deliberately. These writes do not snoop the
    # core's D-cache, so the lines do have to be discarded before the core reads
    # them -- but doing it from this side costs one single-word AP write per
    # 32-byte line, each its own USB round trip: measured at 212 ms per 16 KB
    # chunk, about 7 s of a 17.6 s flash. The core does the same range in a few
    # hundred cycles, so ProgramPage and Verify (see discard_cached_lines in
    # FlashPrg.c) handle it now. If a per-chunk invalidate is ever added back
    # here, the chunking test in test_flashos.tcl fails and says so.
}

# Chunked programmer: FOS_BUF_SIZE per call, verify right after each program while
# the buffer still holds the chunk.
# How many whole sectors [start, start+len) spans, for a sector aligned start.
# Split out of flash_program_bin so the arithmetic can be checked without a
# target: getting this one short leaves the last sector of the image unerased,
# and whether that is even noticed depends on what was in the flash before.
# Reset the target, let it run, and report whether it is running anything sane.
#
# The leg the flash flow went without for a long time. An image written 0x6000
# bytes too low passed every content check for 63 consecutive soak runs, because
# nothing in the flow ever restarted the target: openocd's shutdown leaves the
# core halted where it stopped, and a fresh connect was measured not to reset it
# either -- the PC comes back exactly where the previous session left it. Content
# being byte-perfect says nothing about the board running it.
#
# The judgement is deliberately weak, because it has to hold for any image: no
# fault latched in CFSR, and the core in Thread mode rather than in an exception.
# IPSR is the low 9 bits of xPSR, so `xpsr & 0x1FF == 0` is the Thread-mode test.
# It cannot tell a healthy application from one spinning in its own idle loop, and
# it is not meant to.
#
# A proc rather than a sequence of -c commands in each entry point, because the
# parsing would otherwise exist twice and the two openocd builds do not agree
# about the commands: `reg xPSR` is not a register name on the Windows build (see
# _xpsr_regname), and `reg`/`mdw` answer with their printed text rather than a
# value, so there is a parse either way. One of them, here.
proc _last_word {s} {
    return [lindex [split [string trim $s]] end]
}

proc flashos_boot_check {{settle_ms 1200}} {
    reset run
    sleep $settle_ms
    halt

    if {[catch {
        set pc   [_last_word [reg pc]]
        set xpsr [_last_word [reg [_xpsr_regname]]]
        set cfsr [_last_word [mdw 0xE000ED28 1]]
        set vtor [_last_word [mdw 0xE000ED08 1]]
    } msg]} {
        error "boot: could not read the target after reset ($msg)"
    }
    # VTOR comes from mdw, which prints a bare word, so the 0x is put back here
    # rather than in the entry point. pc and xpsr come out of reg with their own.
    puts "boot   : pc=$pc vtor=0x$vtor cfsr=0x$cfsr xpsr=$xpsr"

    if {$cfsr ne "00000000"} {
        error "boot: FAILED -- a fault is latched in CFSR (0x$cfsr); the image is on the flash byte-perfect but the board does not run it"
    }
    set ipsr [expr {$xpsr & 0x1FF}]
    if {$ipsr != 0} {
        error "boot: FAILED -- the core is in exception $ipsr $settle_ms ms after reset"
    }
    puts "boot   : reset, ran, thread mode, no fault latched"
}

proc _erase_sector_count {len sector_size} {
    return [expr {($len + $sector_size - 1) / $sector_size}]
}

proc flash_program_bin {bin_file {start_addr ""}} {
    if {$start_addr eq ""} {
        set start_addr $::FLASH_BASE
    }
    if {$::FLASH_SIZE <= 0} {
        error "flash_program_bin: FLASH_SIZE is not set -- call flashos_geometry"
    }
    set _t0 [clock milliseconds]
    set sector_size $::FLASH_SECTOR_SIZE
    set chunk_size $::FOS_BUF_SIZE
    puts "=========================================="
    puts "File: $bin_file"

    if {![file exists $bin_file]} {
        error "File not found: $bin_file"
    }
    if {[expr {$start_addr & ($sector_size - 1)}] != 0} {
        puts "start_addr must be $sector_size-byte aligned, got [format 0x%08X $start_addr]"
        return
    }
    # Bytes, not characters: this is the length of the region to erase and the
    # divisor in every progress and rate figure below, and it has to agree with
    # the byte offsets the chunks are read at. See _sram_load_chunk.
    set len [file size $bin_file]
    if {$len == 0} {
        puts "File is empty: $bin_file"
        return
    }

    puts "Binary size: $len bytes"
    puts "Start address: [format 0x%08X $start_addr]"
    puts "Sector size: [format 0x%X $sector_size]"
    puts "=========================================="

    set end_addr [expr {$start_addr + $len}]
    if {$end_addr > [expr {$::FLASH_BASE + $::FLASH_SIZE}]} {
        puts "Binary too large: $len bytes (flash max $::FLASH_SIZE)"
        return
    }

    flashos_load
    flashos_init

    # ---- Phase 1: erase every affected sector ----
    #
    # One EraseRange call rather than one EraseSector per sector. The loop is the
    # same either way; what changes is where it runs, and a call is expensive in a
    # way that has nothing to do with erasing: a breakpoint write, five register
    # writes, a resume, a halt wait and a read-back, ~18 ms against ~1.8 ms of
    # actual erase. This image is 129 sectors, 2536 ms as 129 calls and 245 ms as
    # one. flashos_erase_sector is still here for erasing a single sector.
    #
    # len is a byte count and start_addr is sector aligned (checked above), so
    # this is exactly the count the old loop iterated.
    set sector_count [_erase_sector_count $len $sector_size]
    puts "Erasing $sector_count sector(s) from [format 0x%08X $start_addr]..."
    set _ms_erase 0

    # Posted and not waited for, with the first chunk staged over it -- the same
    # trick, and the same measured premise, as the program calls in phase 2 below:
    # the erase runs in the target's XSPI controller and never touches SRAM, so
    # the link is idle while it works. 129 sectors is ~246 ms of erase against
    # ~460 ms of staging a 62 KB chunk, so the call is hidden whole.
    #
    # This is also why chunk 0 is staged here rather than just before the loop: it
    # is the one chunk with nothing to be staged over, and giving it the erase to
    # hide behind costs nothing. So `_ms_stage` starts accumulating before the
    # first program call, or the largest and least overlapped chunk of the run
    # would be the one missing from the accounting.
    _flashos_call_enter $::FOS_EraseRange $start_addr $sector_count $::FOS_PARAM_ADDR
    set _ms_stage 0
    set first_sz [expr {$chunk_size < $len ? $chunk_size : $len}]
    set _t [clock milliseconds]
    _sram_load_chunk $bin_file 0 $first_sz $::FOS_BUF_ADDR
    incr _ms_stage [expr {[clock milliseconds] - $_t}]

    # What is left of the erase that the staging did not cover, on the same
    # convention as `program` below: by the time the host collects the call's
    # result the core has already halted, so the interval it can measure is the
    # few milliseconds it takes to notice, not the 246 ms the controller spent.
    # It goes to zero when the overlap is working, which is what to watch.
    set _t [clock milliseconds]
    set ret [_flashos_call_leave]
    incr _ms_erase [expr {[clock milliseconds] - $_t}]
    if {$ret != 0} {
        error "FlashOS EraseRange @ [format 0x%08X $start_addr] x$sector_count returned $ret"
    }
    puts "$sector_count sector(s) erased."

    # ---- Phase 2: program + verify, with the staging run ahead ----
    #
    # Staging a chunk is the SWD link carrying the image into SRAM (~120 KiB/s,
    # the probe); programming it is the target's own XSPI controller (~260 KiB/s,
    # the flash). The link is the slower of the two by 2.1x, and the two are not
    # competing for anything -- one is the debug port writing SRAM, the other is
    # the core reading SRAM and driving the controller. So chunk k+1 is staged
    # into the other buffer while chunk k is programmed, and the flash write
    # disappears inside the staging. Measured that the AP can write while the core
    # runs and that the write does not disturb it: ap-while-running.tcl in
    # H787-Test is that measurement and nothing else.
    #
    # The loop is uniform, which costs no overlap: the last chunk has nothing to
    # stage over it, so that end is serial however it is written. The other end is
    # taken care of above, where chunk 0 is staged over the erase.
    #
    # `program` in the summary is the part of the flash write the staging did not
    # cover, not the flash write itself -- see the erase above for the same
    # convention, and for why the number is small rather than absent.
    puts "Programming and Verify..."
    set _ms_prog  0
    set _ms_ver   0

    set nchunks [expr {($len + $chunk_size - 1) / $chunk_size}]

    # ::FLASHOS_GATE is the entry point's switch, and it is one shell word with no
    # path in it -- `-c "set ::FLASHOS_GATE 1"` -- so nothing here has to survive
    # shell quoting on the way in. The image the sums are computed over is this
    # call's own bin_file. A caller that wants the sums of a *different* file --
    # which is how the gate is tested for being able to fail -- calls flashos_gate
    # itself before this runs, and the block below leaves an existing ::CHUNK_SUMS
    # alone.
    if {[info exists ::FLASHOS_GATE] && $::FLASHOS_GATE && ![info exists ::CHUNK_SUMS]} {
        flashos_gate $bin_file $chunk_size
    }

    # The checksum gate. If the host set ::CHUNK_SUMS -- a list of one Fletcher
    # sum per chunk, over the image bytes that chunk holds -- then every chunk is
    # checked against the FLASH after it is written, and the run stops loudly on
    # a mismatch instead of completing and reporting success.
    #
    # Without it there is no such check anywhere in the pipeline, and that is not
    # an oversight in Verify: Verify compares the flash against the buffer it was
    # written from, so a buffer that was damaged on the way in programs the
    # damage and then certifies it. The same is true of every progress figure,
    # which divides by the file's length whatever happened to the bytes. Only the
    # host holds the original, so only the host can ask the question -- and this
    # asks it of the flash, which is the only place the answer means anything.
    #
    # Off by default: the readback compare at the end of flash.sh already catches
    # everything this does, 13 s later and without saying which chunk. This makes
    # a failure local and immediate, and costs a call per chunk to do it.
    set _gate 0
    if {[info exists ::CHUNK_SUMS]} {
        if {[llength $::CHUNK_SUMS] != $nchunks} {
            error "flash_program_bin: ::CHUNK_SUMS has [llength $::CHUNK_SUMS] entries, the image makes $nchunks chunks"
        }
        if {![info exists ::CHUNK_SIZE] || $::CHUNK_SIZE != $chunk_size} {
            error "flash_program_bin: ::CHUNK_SIZE ([expr {[info exists ::CHUNK_SIZE] ? $::CHUNK_SIZE : "unset"}]) does not match FOS_BUF_SIZE ($chunk_size) -- the sums were computed for a different chunking"
        }
        set _gate 1
        puts "Chunk checksums: enabled ($nchunks chunks, target reads the flash)"
    }

    # Chunk 0 is already staged into buf(0), over the erase.
    for {set k 0} {$k < $nchunks} {incr k} {
        # Chunk k is in buf(k), staged here or by the previous iteration.
        set cur  [expr {$k % 2 ? $::FOS_BUF2_ADDR : $::FOS_BUF_ADDR}]
        set pa   [expr {$start_addr + $k * $chunk_size}]
        set chunk_sz [expr {$pa + $chunk_size <= $end_addr ? $chunk_size : $end_addr - $pa}]

        _flashos_call_enter $::FOS_ProgramPage $pa $chunk_sz $cur $::FOS_PARAM_ADDR

        set _t [clock milliseconds]
        if {$k + 1 < $nchunks} {
            set next_pa  [expr {$pa + $chunk_size}]
            set next_sz  [expr {$next_pa + $chunk_size <= $end_addr ? $chunk_size : $end_addr - $next_pa}]
            set next_buf [expr {$k % 2 ? $::FOS_BUF_ADDR : $::FOS_BUF2_ADDR}]
            _sram_load_chunk $bin_file [expr {$next_pa - $start_addr}] $next_sz $next_buf
        }
        incr _ms_stage [expr {[clock milliseconds] - $_t}]

        # Timed from after the staging, not from before it: what this should
        # measure is how much of the flash write outlasted the staging of the next
        # chunk, which is the part that did not get hidden. Timing from before the
        # staging instead counts the staging itself, and then `program` comes out
        # larger than `stage` -- which is how this was first written.
        set _t [clock milliseconds]
        set ret [_flashos_call_leave]
        incr _ms_prog [expr {[clock milliseconds] - $_t}]
        if {$ret != 0} {
            puts "ProgramPage @ [format 0x%08X $pa] returned $ret"
            return
        }

        if {$_gate} {
            set want [lindex $::CHUNK_SUMS $k]
            set got  [flashos_flash_sum $pa $chunk_sz]
            if {$got != $want} {
                # Both sides, on disk, before anything is retried over them: the
                # buffer is what the flash was written from and the flash is what
                # came out, and the difference between the two is what says which
                # one is wrong. Nothing else in the run can answer that.
                set dir "/tmp/flashop-gate"
                file mkdir $dir
                dump_image [format "%s/chunk%02d-buf.bin"   $dir $k] $cur $chunk_sz
                dump_image [format "%s/chunk%02d-flash.bin" $dir $k] $pa  $chunk_sz
                puts "  checksum MISMATCH on chunk $k @ [format 0x%08X $pa]:"
                puts [format "    flash 0x%08X  image 0x%08X" $got $want]
                puts "    both dumped to $dir/chunk[format %02d $k]-{buf,flash}.bin"

                # Once more, in case it was the flash write rather than the
                # buffer that went wrong -- the flash write is retried cheaply
                # from the same bytes. If the buffer is the damaged one this
                # changes nothing and the error below is the right answer.
                set got [flashos_flash_sum $pa $chunk_sz]
                if {$got != $want} {
                    puts "    re-programming chunk $k..."
                    set ret [flashos_call $::FOS_ProgramPage $pa $chunk_sz $cur $::FOS_PARAM_ADDR]
                    if {$ret != 0} {
                        error "ProgramPage @ [format 0x%08X $pa] returned $ret on retry"
                    }
                    set got [flashos_flash_sum $pa $chunk_sz]
                }
                if {$got != $want} {
                    error "chunk $k @ [format 0x%08X $pa] does not match the image after two writes (flash 0x[format %08X $got], image 0x[format %08X $want]) -- see $dir"
                }
                puts "    retry cleared it: the flash write, not the staging."
            }
        }

        set _t [clock milliseconds]
        set ret [flashos_call $::FOS_Verify $pa $chunk_sz $cur $::FOS_PARAM_ADDR]
        incr _ms_ver [expr {[clock milliseconds] - $_t}]
        if {$ret != 0} {
            puts "Verify failed @ [format 0x%08X $pa] (returned $ret)"
            return
        }

        set progress [expr {($pa + $chunk_sz - $start_addr) * 100 / $len}]
        puts "  $progress% (program+verify @ [format 0x%08X $pa])"
    }

    flashos_uninit

    set _el [expr {[clock milliseconds] - $_t0}]
    puts "Flash programming complete in [format %.1f [expr {$_el / 1000.0}]] s."

    # Rate helper. Every one of these is wall clock around the calls themselves,
    # so the per-chunk `puts` above is not inside any of them -- the progress
    # lines are not slowing down the number they report.
    #
    # `erase` and `program` are not the erase and the flash write: they are the
    # parts of each that the staging did not cover, and they should sit near zero.
    # They get no rate column, because a rate taken over the uncovered remainder of
    # an overlapped operation is a number about this measurement rather than about
    # the flash -- 14 ms of leftover erase divided into 526596 bytes is 36 MB/s of
    # nonsense. `stage` and `verify` are what they say they are.
    set _kb [expr {$len / 1024.0}]
    foreach {name ms rate} [list "erase   " $_ms_erase 0  "stage   " $_ms_stage 1 \
                                 "program " $_ms_prog  0  "verify  " $_ms_ver   1 \
                                 "total   " $_el       1] {
        if {$rate && $ms > 0} {
            puts "  $name : [format %6d $ms] ms  [format %7.2f [expr {$_kb * 1000.0 / $ms}]] KiB/s"
        } else {
            puts "  $name : [format %6d $ms] ms        --"
        }
    }
    puts "  (erase and program are the parts staging did not cover; the rest of"
    puts "   each is inside stage. The flash write itself is ~260 KiB/s.)"
    puts "  ($sector_count sectors, [format %d $len] bytes)"
}
