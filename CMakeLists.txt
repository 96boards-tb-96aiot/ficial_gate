cmake_minimum_required(VERSION 3.8.2)

project(ficial_gate)

set(SRC
    ui.c
    database.c
    rockface_control.c
    load_feature.c
    shadow_display.c
    play_wav.c
    rkisp_control.c
    rkcif_control.c
    rga_control.c
    video_common.c
    main.c
)

include_directories(${DRM_HEADER_DIR})

add_executable(ficial_gate ${SRC})

target_link_libraries(ficial_gate rkisp rkisp_api rockface rknn_api drm rga pthread ts minigui_ths png12 jpeg freetype sqlite3 asound)

install(TARGETS ficial_gate DESTINATION bin)

install(DIRECTORY wav/cn/ DESTINATION ../etc)

install(FILES minigui/MiniGUI-1280x720.cfg DESTINATION ../etc RENAME MiniGUI.cfg)
install(DIRECTORY minigui DESTINATION local/share)

install(FILES S99_ficial_gate DESTINATION ../etc/init.d
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_WRITE WORLD_EXECUTE)
