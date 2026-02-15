# Dreamcast Platform Configuration
# =================================

# Define __DREAMCAST__ for all C/C++ code.
# This is NOT automatically defined by sh-elf-gcc. Without it, all
# #ifdef __DREAMCAST__ blocks (dc_init, dc_video, movie skip, path
# setup, controller input, save system) are dead code.
add_compile_definitions(__DREAMCAST__)

# Build type: MinSizeRel for smallest binary (-Os -DNDEBUG)
# This is critical on Dreamcast - unoptimized code wastes ~1MB+ of RAM
set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "" FORCE)

# Audio settings - sounds load on-demand so only a few are in RAM at a time.
# Disable streaming for sound effects: CD reads via MPQ decompression are
# too slow for real-time audio on Dreamcast, causing stuttering on speech.
# Sound effects and NPC speech load fully into RAM before playback (small files).
# Music must still stream - music files are too large (several MB) to fit in RAM.
set(DISABLE_STREAMING_SOUNDS ON)
set(DEFAULT_AUDIO_BUFFER_SIZE 2048)
set(DEFAULT_AUDIO_SAMPLE_RATE 22050)
set(DEFAULT_AUDIO_RESAMPLING_QUALITY 0)

# Re-enable the 128KB palette transparency LUT for faster transparency blending.
# With NOSOUND freeing audio RAM, we can afford this performance optimization.
set(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT ON CACHE BOOL "" FORCE)

# General build options
set(NONET ON)
set(BUILD_TESTING OFF)
set(USE_SDL1 ON)
set(ASAN OFF)
set(UBSAN OFF)
set(NOEXIT ON)
set(PREFILL_PLAYER_NAME ON)
set(DISABLE_DEMOMODE ON)  # Required for UNPACKED_SAVES without C++17 filesystem

# Resolution: 640x480 with zoom forced ON for performance.
# Zoom renders the game world at half resolution (~24 tiles instead of ~132)
# then upscales 2x. The PS1-style minimal HUD replaces the full panel.
set(DEFAULT_WIDTH 640)
set(DEFAULT_HEIGHT 480)

# Disable system library finders that don't work for cross-compilation
set(DEVILUTIONX_SYSTEM_LIBSODIUM OFF)
set(DEVILUTIONX_SYSTEM_SDL_IMAGE OFF)

# Use standard MPQ archives (packed) for game assets
# This avoids the need to extract assets on the host.
# DevilutionX already supports reading MPQs via libmpq.
set(UNPACKED_MPQS OFF)

# =================================
# Dreamcast Save System (VMU)
# =================================
#
# VMU (Visual Memory Unit) is the Dreamcast's memory card with ~100KB usable space.
# It does NOT support directories - only flat files at /vmu/[port][unit]/
#
# We use UNPACKED_SAVES to write individual files instead of MPQ archives.
# Combined with modifications to CreateDir() and save path handling, this
# allows saves to work on VMU with flat file names like:
#   /vmu/a1/dvx_s0_hero   (single player slot 0, hero file)
#   /vmu/a1/dvx_s0_game   (single player slot 0, game state)
#   /vmu/a1/dvx_m1h_hero  (multiplayer hellfire slot 1, hero)
#
# Naming convention: dvx_[mode][slot][h]_[filename]
#   mode: s=single, m=multi, sp=spawn, sh=share
#   h: present for Hellfire
#
# LZO Compression (like dca3-game):
# All save files are automatically compressed using miniLZO to maximize
# the number of saves that fit in VMU's limited space.
# Format: [4-byte header with size/compression flag][compressed or raw data]
# Typical compression ratio: 30-50% size reduction
#
# This approach is similar to how dca3-game (GTA III port) handles VMU saves.
set(UNPACKED_SAVES ON)

# =================================
# Dreamcast Controller Mapping
# =================================
#
# The Dreamcast controller has:
#   - 4 face buttons: A (bottom), B (right), X (left), Y (top)
#   - Start button
#   - D-pad (mapped as SDL hat)
#   - Analog stick (axes 0, 1)
#   - L/R analog triggers (axes 3, 2)
#
# SDL button indices from KOS SDL_sysjoystick.c:
#   Button 0: A (CONT_A)    - bottom position (like Xbox A)
#   Button 1: B (CONT_B)    - right position  (like Xbox B)
#   Button 2: X (CONT_X)    - left position   (like Xbox X)
#   Button 3: Y (CONT_Y)    - top position    (like Xbox Y)
#   Button 4: Start (CONT_START)
#   Button 5: C (not on standard controller)
#   Button 6: D (not on standard controller)
#   Button 7: Z (not on standard controller)
#
# The Dreamcast ABXY layout matches Xbox positions, NOT Nintendo.
# (Nintendo has A/B and X/Y swapped relative to Xbox)

# Joystick button mapping (ABXY matches Xbox layout)
set(JOY_BUTTON_A 0)      # A = bottom button (primary action)
set(JOY_BUTTON_B 1)      # B = right button (secondary action)
set(JOY_BUTTON_X 2)      # X = left button (spell)
set(JOY_BUTTON_Y 3)      # Y = top button (cancel/back)
set(JOY_BUTTON_START 4)  # Start button

# D-pad is mapped as SDL hat 0
# Hat values: UP=1, RIGHT=2, DOWN=4, LEFT=8
set(JOY_HAT_DPAD_UP_HAT 0)
set(JOY_HAT_DPAD_DOWN_HAT 0)
set(JOY_HAT_DPAD_LEFT_HAT 0)
set(JOY_HAT_DPAD_RIGHT_HAT 0)
set(JOY_HAT_DPAD_UP 1)
set(JOY_HAT_DPAD_DOWN 4)
set(JOY_HAT_DPAD_LEFT 8)
set(JOY_HAT_DPAD_RIGHT 2)

# Analog stick axes
set(JOY_AXIS_LEFTX 0)   # Analog stick X
set(JOY_AXIS_LEFTY 1)   # Analog stick Y

# Triggers are analog axes on Dreamcast (no shoulder buttons)
# We map them as TRIGGERLEFT/TRIGGERRIGHT for inventory/character screens
# Note: The triggers can also be read as analog values, but for button
# detection we treat axis threshold crossings as button presses
#
# Since Dreamcast triggers are axes (not buttons), we need to handle them
# specially. For now, we use the "back" button concept differently.
# Users can press Start + Y for menu/back functionality.

# Gamepad layout - use Generic (ABXY naming, Xbox-style positions)
set(DEVILUTIONX_GAMEPAD_TYPE Generic)

# minilzo library for VMU save compression
add_subdirectory(3rdParty/minilzo)

# Dreamcast platform sources (follows standard platform pattern)
list(APPEND DEVILUTIONX_PLATFORM_SUBDIRECTORIES platform/dreamcast)
list(APPEND DEVILUTIONX_PLATFORM_LINK_LIBRARIES libdevilutionx_dreamcast)

# Workaround for libmpq: It expects BZip2::BZip2 target, which FindBZip2 might fail to create
# when variables are forced in the toolchain file.
if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 STATIC IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES
        IMPORTED_LOCATION "${BZIP2_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIR}"
    )
endif()
