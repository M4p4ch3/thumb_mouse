idf_component_register(
    SRCS "utils.c" "controller.c" "trackball.c" "mouse.c" "logger.c" "thumb_mouse.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver "esp_timer"
)
