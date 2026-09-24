# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

# Nations N32H78x/H76x NRP-1A, Cortex-M7, CMSIS-DAP NSLink (HID).
# Validated board: N32H787XIB7. This is not a universal N32H7 Flash driver.
# The flash path is flashos/flashos.tcl + flashop.bin, imported from the ns-link
# tree -- keep all three files in flashos/ beside this configuration. flash_op/ and
# its vendor blob are left in place but are no longer used by anything here.

adapter driver cmsis-dap
cmsis_dap_vid_pid 0x19f5 0x3106
# cmsis_dap_backend hid
transport select swd
adapter speed 8000
source [find mem_helper.tcl]

swd newdap n32h7x cpu -irlen 4 -ircapture 0x1 -irmask 0xf -expected-id 0x2ba01477
dap create n32h7x.dap -chain-position n32h7x.cpu
target create n32h7x.cpu0 cortex_m -endian little -dap n32h7x.dap -ap-num 0
# BOOT may temporarily disable AP0. Call n32h7x_connect before core access.
n32h7x.cpu0 configure -defer-examine
reset_config none connect_deassert_srst
cortex_m reset_config sysresetreq
adapter srst pulse_width 100
adapter srst delay 1000
gdb_memory_map enable
gdb_flash_program enable
gdb_breakpoint_override hard
n32h7x.cpu0 configure -event gdb-attach {n32h7x_connect; halt 3000}
n32h7x.cpu0 configure -event gdb-detach {resume}

# OpenOCD 0.12.0 releases SRST even for software reset (reset_config none).
# Keep SELECT=0 immediately before that native deassert path as well.
# This complements the hardware assertion sequence below.
n32h7x.cpu0 configure -event reset-deassert-pre {n32h7x.dap dpreg 8 0}

# The NSLink/N32H787 hardware-reset sequence requires SELECT=0 immediately
# before NRST assertion. Their native core reset handling is preserved.
proc n32h7x_assert_run {} {
    set ::n32h7x_assert_result [catch {
        mww 0xe000edf8 0
        mww 0xe000edf0 0xa05f0003
        set dfsr [mrw 0xe000ed30]
        mww 0xe000ed30 $dfsr
        mww 0xe000edf0 0xa05f0001
        n32h7x.dap dpreg 8 0
        adapter assert srst
        sleep 50
    } ::n32h7x_assert_error]
}

# Preserve OpenOCD's outer recursion guard, deassert and reconnect handling.
if {[llength [info procs n32h7x_original_reset_inner]] == 0} {
    rename ocd_process_reset_inner n32h7x_original_reset_inner
}
proc ocd_process_reset_inner {mode} {
    set cfg [reset_config]
    set has_srst [expr {[lsearch -exact $cfg srst_only] >= 0 ||
                       [lsearch -exact $cfg trst_and_srst] >= 0}]
    if {$mode ne "run" || !$has_srst || [transport select] ne "swd" ||
        [target names] ne "n32h7x.cpu0"} {
        return [n32h7x_original_reset_inner $mode]
    }
    set old_assert [n32h7x.cpu0 cget -event reset-assert]
    if {$old_assert ne ""} {return [n32h7x_original_reset_inner $mode]}
    set ::n32h7x_assert_result -1
    set ::n32h7x_assert_error "reset-assert was not reached"
    set rc [catch {
        n32h7x.cpu0 configure -event reset-assert {n32h7x_assert_run}
        n32h7x_original_reset_inner $mode
        # The native event dispatcher does not propagate Tcl callback errors.
        if {$::n32h7x_assert_result != 0} {error $::n32h7x_assert_error}
    } result]
    n32h7x.cpu0 configure -event reset-assert $old_assert
    unset ::n32h7x_assert_result
    unset ::n32h7x_assert_error
    if {$rc != 0} {error $result}
    return $result
}

# BOOT holds AP0 disabled for the first ~120 ms after the debugger's nRESET
# pulse, so the old flat 2000 ms wait was mostly margin. Keep a short floor for
# that measured window, then poll for AP0 instead of sleeping the rest.
set N32H7X_AP0_SETTLE_MS 150
set N32H7X_AP0_TIMEOUT_MS 3000
set N32H7X_AP0_POLL_MS 10

proc n32h7x_wait_ap0 {} {
    set waited 0
    set csw 0
    set dap_error ""
    while {1} {
        if {$waited >= $::N32H7X_AP0_SETTLE_MS} {
            set dap_error ""
            if {[catch {set csw [n32h7x.dap apreg 0 0]} err]} {
                set csw 0
                set dap_error $err
            } elseif {($csw & 0x40) != 0} {
                return $csw
            }
        }
        if {$waited >= $::N32H7X_AP0_TIMEOUT_MS} {break}
        sleep $::N32H7X_AP0_POLL_MS
        incr waited $::N32H7X_AP0_POLL_MS
    }
    set detail "CSW=[format 0x%08x $csw]"
    if {$dap_error ne ""} {append detail ", last DAP error: $dap_error"}
    error "AP0 is disabled after BOOT wait ($detail); power-cycle the board before reconnecting"
}

proc n32h7x_connect {} {
    init
    n32h7x_wait_ap0
    n32h7x.cpu0 arp_examine
}

# This firmware is built -DUSING_TCM with TCM_SIZE_VALUE 0x15 (256 KiB ITCM,
# 512 KiB DTCM, 256 KiB AXI SRAM2); see ConfigTcmSize() in system_n32h7xx.c.
# Parts fresh from the factory (or booting an image built without USING_TCM)
# keep the reset-default partition where no DTCM exists at all: DTCMCR reads
# back 0 and every DAP access to 0x20000000.. fails while writes vanish into
# the posted-buffer abyss. FlashOS is staged at 0x20050000 in DTCM, so the
# first flash over such an image dies in load_image before anything is erased.
#
# The fix is exactly what the new firmware's own SystemInit would do on its
# first boot: request partition 0x15 through the SDK mailbox (0x51105280) and
# issue one system reset, after which the boot ROM remaps the TCM banks. The
# mailbox survives the reset; no option bytes are changed and flash is never
# touched. Probe DTCM by readback rather than trusting any partition number.
set N32H7X_TCM_PARTITION_VALUE 0x15
set N32H7X_TCM_PARTITION_MBOX  0x51105280
set N32H7X_TCM_PROBE_ADDR      0x20050000

proc n32h7x_dtcm_present {} {
    # DTCMCR (SCB offset 0x294) bit 0 is the authoritative DTCM enable.
    # The old write/read probe at 0x20050000 passes on AXI SRAM which
    # aliases that address when TCM is absent, masking the problem.
    if {[catch {set v [mrw 0xe000ed94]}]} { return 0 }
    return [expr {($v & 1) != 0}]
}

proc n32h7x_ensure_tcm_partition {} {
    if {[n32h7x_dtcm_present]} { return }
    echo "DTCMCR.EN is 0; enabling DTCM/ITCM directly"
    mww 0xe000ed94 0x1
    mww 0xe000ed90 0x1
    if {![n32h7x_dtcm_present]} {
        error "DTCM could not be enabled via DTCMCR; power-cycle the board and retry"
    }
    echo "DTCM enabled via DTCMCR"
}

# Ask the bootloader to stay in USB MSC mode after the next reset. This is a
# magic/complement pair in reserved AHB SRAM, outside the bootloader's BSS.
proc n32h7x_request_usb {} {
    mww 0x30015f00 0x544f4f42
    mww 0x30015f04 0xabb0b0bd
}

# Stop the application and enter the bootloader by installing its reset vector,
# without using OpenOCD's reset path. On this board a system reset can perform a
# second reset after the bootloader has already consumed the one-shot flag.
proc n32h7x_boot_usb {} {
    n32h7x_connect
    halt 3000
    wait_halt 3000
    reg primask 1
    mww 0xe000e010 0
    n32h7x_hold_m4_in_reset
    mww 0x3000039c 0
    n32h7x_request_usb
    mww 0xe000ed08 0x15000000
    reg msp [mrw 0x15000000]
    reg pc [expr {[mrw 0x15000004] & ~1}]
    reg primask 0
    resume
}

set FLASH_START 0x15000000
set FLASH_SIZE 0x001e0000
set FLASH_PAGE 0x1000

# Flash algorithm and driver, from the ns-link tree's flashop/ and carrying its own
# copy here so this config needs no second checkout. Emptying flashos/README.md's
# list of files breaks this: flashos.tcl loads flashop_sym.tcl and flashop.bin from
# beside itself, and flashos_geometry below is the only place the part's shape is
# declared. The blob is position-independent -- its only absolute literals are
# peripheral and system-memory addresses, never RAM -- so the base is ours to choose.
set N32H7X_FLASHOS [file join [file dirname [info script]] flashos]
source [file join $N32H7X_FLASHOS flashos.tcl]

# CM7 DTCM, and the third address this layout has had. The first two are worth
# recording because each looked fine and each failed the same silent way:
#
#   0x24000000  CM7 AXI SRAM, the natural home for a CM7 image. Fills wall to wall
#               with .data/.bss (and, in the dual-core build, the CM4's camera DMA
#               buffer), and staging over them corrupted ~250 bytes a chunk.
#   0x30010000  AHB SRAM1 -- what this config used when it drove the vendor blob,
#               with a 16 KiB buffer stopping at 0x30015000. That fits; the 62 KiB
#               buffers this driver uses do not. Per LinkFile/n32h787_kws_demo_CM7.ld
#               that window held the dual-core SNAPSHOT at 0x30016000 and then the
#               CM4's own RW_SRAM, and planting a pattern over the latter changed
#               2341 bytes of it. 24 KiB is all 0x30010000 has before 0x30016000.
#
# Neither failure was visible from inside the pipeline: Verify compares the buffer
# against the flash it was just programmed from, and a readback would have compared
# the flash against that same buffer. Both agree, because both hold the same wrong
# bytes. Only a checksum against the host's copy of the image sees it.
#
# DTCM cannot fail that way. LinkFile/n32h787_kws_demo_CM7.ld declares it as
# `DTCM (rw) : ORIGIN = 0x20020000, LENGTH = 0x60000`, and this layout takes the
# upper 0x30000 of that. It is private to the CM7: no DMA engine and no other core
# has an address for it, so nothing writes it behind the flasher's back, and it is
# not cacheable, so the D-cache maintenance the other two layouts needed is a no-op
# here. The window below it (0x20020000-0x2004ffff) holds the keyword spotter's
# TFLM tensor arena, which the application rebuilds from the model on every boot,
# and this flow ends in `reset run`.
#
# The layout inside it, from flashos_setup at the base above:
#   0x20050000-0x20050293 : algorithm code (612 bytes) + parameter block
#   0x20050294-0x20050fdf : FlashOS stack, growing down from the top
#   0x20050ff0            : BKPT instruction for function return
#   0x20051000-0x200607ff : 62 KiB staging buffer 0
#   0x20060800-0x2006ffff : 62 KiB staging buffer 1
# Two buffers, because ProgramPage is posted without waiting so the next chunk is
# staged over the flash write; that overlap is where most of the speed is.
set ::FOS_SRAM_BYTES 0x30000
flashos_setup 0x20050000
flashos_geometry 0x15000000 0x1E0000 0x1000

proc n32h7x_hold_m4_in_reset {} {
    # The application is single-core and never releases the second core, but a
    # firmware flashed over by an older dual-core build still can have it
    # running, and the vendor FlashOS Init asserts this same bit.  Claiming it
    # here means flashing works regardless of what was in the part before, and
    # it is the precondition wm8978_n32.c's bus-ownership check and
    # board_sdram.c both rely on.
    #
    # The camera, its DVP2/JPEG bus masters and the mailbox handshake the old
    # M4 used to acknowledge are all gone; only the reset bit remains.
    mww 0x58030174 0
    if {([mrw 0x58030174] & 1) != 0} {error "M4 could not be held in reset"}
}

proc n32h7x_quiesce_cached_m7 {} {
    if {([mrw 0xe000ed14] & 0x30000) == 0} {return}
    # Only resume firmware explicitly implementing this non-cacheable ABI.
    # Never run injected cache maintenance against arbitrary/faulted firmware.
    if {[mrw 0x30000000] != 0x4d344951 || [mrw 0x300002b4] != 0x4d374331} {
        error "Enabled caches without M7 flash-pause ABI; stop/clean caches in firmware before flashing"
    }
    mww 0x300002b0 0
    mww 0x300002a8 0x4d344951
    set paused 0
    set rc [catch {
        resume
        for {set attempt 0} {$attempt < 400} {incr attempt} {
            if {[mrw 0x300002b0] == 0x4d344951} {set paused 1; break}
            sleep 20
        }
    } result]
    halt 3000
    wait_halt 3000
    if {$rc != 0} {error $result}
    if {!$paused || ([mrw 0xe000ed14] & 0x30000) != 0} {
        error "M7 cache shutdown was not acknowledged; Flash untouched"
    }
    echo "M7 paused with caches cleaned and disabled"
}

proc n32h7x_get_core_regs {{names {pc msp xPSR}}} {
    # Linux OpenOCD names this xPSR; N32Studio's Windows build uses xpsr.
    set compatible {}
    foreach name $names {
        lappend compatible [expr {$name eq "xPSR" ? "xpsr" : $name}]
    }
    if {[catch {get_reg -force $names} result]} {
        set result [get_reg -force $compatible]
    }
    return $result
}

proc n32h7x_xpsr {registers} {
    if {[dict exists $registers xPSR]} {return [dict get $registers xPSR]}
    return [dict get $registers xpsr]
}

# ---- nothing below is called any more; kept as the record of what it did ----
#
# flash_program_bin in flashos/flashos.tcl is the whole flash path now, and its own
# timing summary replaces the profiler these three drive. n32h7x_flashos_call is the
# vendor BKPT call protocol written out in full -- worth reading against
# _flashos_call_enter/_leave in flashos.tcl, which does the same register writes and
# then returns before waiting, so the caller can stage the next chunk over the call.
# The two switches it read, N32H7X_FLASH_PROFILE and N32H7X_FLASH_FAST_HALT, were
# never set anywhere, which is why the old path never printed a breakdown.
proc n32h7x_flash_profile_enabled {} {
    return [expr {[info exists ::N32H7X_FLASH_PROFILE] &&
                  $::N32H7X_FLASH_PROFILE}]
}

proc n32h7x_flash_profile_begin {} {
    if {![n32h7x_flash_profile_enabled]} {return ""}
    mww 0xe000edfc [expr {[mrw 0xe000edfc] | 0x01000000}]
    mww 0xe0001fb0 0xc5acce55
    mww 0xe0001000 [expr {[mrw 0xe0001000] | 1}]
    mww 0xe0001004 0
    return [clock milliseconds]
}

proc n32h7x_flash_profile_end {func started} {
    if {$started eq ""} {return}
    set elapsed [expr {[clock milliseconds] - $started}]
    set cycles [mrw 0xe0001004]
    set name unknown
    set func_key [format 0x%08x $func]
    foreach pair [list [list init $::FOS_Init] \
                       [list erase $::FOS_EraseSector] \
                       [list program $::FOS_ProgramPage] \
                       [list verify $::FOS_Verify] \
                       [list uninit $::FOS_UnInit]] {
        if {$func_key eq [format 0x%08x [lindex $pair 1]]} {
            set name [lindex $pair 0]
            break
        }
    }
    if {![info exists ::N32H7X_FLASH_STATS]} {set ::N32H7X_FLASH_STATS [dict create]}
    dict incr ::N32H7X_FLASH_STATS ${name}_calls
    dict incr ::N32H7X_FLASH_STATS ${name}_ms $elapsed
    dict incr ::N32H7X_FLASH_STATS ${name}_cycles $cycles
}

proc n32h7x_wait_flashos_halt {} {
    if {![info exists ::N32H7X_FLASH_FAST_HALT] || !$::N32H7X_FLASH_FAST_HALT} {
        sleep 10
        wait_halt 30000
        return
    }
    # OpenOCD's wait_halt polls on a coarse interval. FlashOS calls are short,
    # so poll frequently when the caller explicitly enables this path.
    set deadline [expr {[clock milliseconds] + 30000}]
    while {1} {
        poll
        if {[n32h7x.cpu0 curstate] eq "halted"} {return}
        if {[clock milliseconds] >= $deadline} {error "FlashOS halt timeout"}
        sleep 1
    }
}

proc n32h7x_flashos_call {func args} {
    set profile_started [n32h7x_flash_profile_begin]
    mww $::FOS_BKPT_ADDR 0x0000be00
    set i 0
    foreach name {r0 r1 r2 r3} {
        if {$i < [llength $args]} {
            reg $name [lindex $args $i]
        } else {
            reg $name 0
        }
        incr i
    }
    reg sp $::FOS_STACK_TOP
    reg lr [expr {$::FOS_BKPT_ADDR | 1}]
    reg pc $func
    reg primask 1
    resume
    if {[catch {n32h7x_wait_flashos_halt} result]} {
        catch {halt 3000}
        error "FlashOS timeout at [format 0x%08x $func]: $result"
    }
    set returned [n32h7x_get_core_regs {r0 pc xPSR}]
    set pc [dict get $returned pc]
    if {($pc != $::FOS_BKPT_ADDR && $pc != $::FOS_BKPT_ADDR + 2) ||
        ([n32h7x_xpsr $returned] & 0x010001ff) != 0x01000000} {
        error "FlashOS stopped outside its return breakpoint: $returned"
    }
    set value [dict get $returned r0]
    if {$value != 0} {
        error "FlashOS failed at [format 0x%08x $func], status=$value"
    }
    n32h7x_flash_profile_end $func $profile_started
}

# High-level entry point: prepare, program, verify, then software-reset to run.
# Only the image's occupied sectors are erased. The start must be page-aligned.
proc n32h7x_flash {image {transfer_khz 1000} {start_addr 0x15000000}} {
    if {![file isfile $image]} {error "Image not found: $image"}
    if {![file isfile $::FLASHPRG_ALGO_BIN]} {error "Flash algorithm not found: $::FLASHPRG_ALGO_BIN"}
    set length [file size $image]
    if {$length == 0 || $start_addr < $::FLASH_START ||
        $start_addr + $length > $::FLASH_START + $::FLASH_SIZE ||
        ($start_addr % $::FLASH_PAGE) != 0} {
        error "Image must fit in Flash and start on a 4096-byte boundary"
    }
    if {![string is integer -strict $transfer_khz] || $transfer_khz <= 0} {
        error "Transfer speed must be a positive integer in kHz"
    }
    set stage "connect"
    set rc [catch {
        n32h7x_connect
        set stage "halt"
        halt 3000
        wait_halt 3000
        set stage "TCM partition"
        n32h7x_ensure_tcm_partition
        set stage "halted CPU validation"
        set cpuid [mrw 0xe000ed00]
        set dhcsr [mrw 0xe000edf0]
        set core [n32h7x_get_core_regs]
        if {($cpuid & 0xff0ffff0) != 0x410fc270 ||
            ($dhcsr & 0x30000) != 0x30000 ||
            ([n32h7x_xpsr $core] & 0x01000000) == 0 ||
            [dict get $core msp] == 0} {
            error "Invalid halted CPU state: CPUID=[format 0x%08x $cpuid], DHCSR=[format 0x%08x $dhcsr], $core"
        }
        set stage "cache check"
        n32h7x_quiesce_cached_m7
        if {([mrw 0xe000ed14] & 0x30000) != 0} {
            error "FlashOS preparation with enabled instruction/data caches is not supported"
        }
        set stage "M4 reset"
        n32h7x_hold_m4_in_reset
        set stage "stop SysTick"
        mww 0xe000e010 0
        # Debug-only VECTCLRACTIVE clears Handler state without restarting BOOT.
        # VECTRESET/reset halt can disable AP0 on this chip during secure BOOT.
        set stage "clear active exceptions"
        set aircr [mrw 0xe000ed0c]
        mww 0xe000ed0c [expr {0x05fa0002 | ($aircr & 0x700)}]
        set stage "post-clear poll"
        poll
        set stage "Thread-mode check"
        if {([mrw 0xe000ed04] & 0x1ff) != 0} {
            error "Refusing to execute FlashOS outside Thread mode"
        }
        set stage "interrupt and MPU preparation"
        set banks [expr {([mrw 0xe000e004] & 0xf) + 1}]
        for {set i 0} {$i < $banks} {incr i} {
            mww [expr {0xe000e180 + 4 * $i}] 0xffffffff
            mww [expr {0xe000e280 + 4 * $i}] 0xffffffff
        }
        mww 0xe000ed04 0x0a000000
        mww 0xe000ed28 0xffffffff
        mww 0xe000ed2c 0xffffffff
        mww 0xe000ed94 0
        set stage "core register preparation"
        # Register 16 is xPSR; its spelling differs between OpenOCD versions.
        reg 16 0x01000000
        reg control 0
        reg msp $::FOS_STACK_TOP
        reg sp $::FOS_STACK_TOP
        reg basepri 0
        reg faultmask 0
        reg primask 1
    } result]
    set restore_rc [catch {cortex_m reset_config sysresetreq} restore_result]
    if {$rc != 0} {
        if {$result eq ""} {set result "OpenOCD returned an error without details"}
        error "Flash preparation failed at $stage: $result. Flash was not erased or programmed."
    }
    if {$restore_rc != 0} {
        error "Cannot restore SYSRESETREQ mode: $restore_result. Flash was not erased or programmed."
    }
    adapter speed $transfer_khz
    # A separate AHB scratch area lets OpenOCD verify with its CRC routine.
    # It does not overlap the DTCM the algorithm borrows, and it sits below the
    # 0x30016000 boundary that partitions AHB SRAM1: .m4_shared occupies the
    # first 1 KiB and nothing else in this firmware claims the rest, but the
    # OpenOCD work area is deliberately kept out of the region above it.
    n32h7x.cpu0 configure -work-area-phys 0x30015000 -work-area-size 0x400 -work-area-backup 0
    # flash_program_bin is the whole run from here: flashos_load stages the blob and
    # verifies it in SRAM before anything executes it -- a successful SWD write is
    # not proof that SRAM stored the algorithm, and Init is the first call that
    # could erase anything -- then one EraseRange call instead of one EraseSector
    # per page, then ProgramPage posted without waiting so the next chunk is staged
    # over the flash write, then Verify and UnInit. It calls flashos_load and
    # flashos_init itself; the ~18 ms a call costs against ~1.8 ms of actual erase
    # is what the single EraseRange and the overlap exist to remove.
    flash_program_bin $image $start_addr
    adapter speed 8000
    reset run
    echo "Flash complete; reset released"
}
