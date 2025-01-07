set(srcs "main.c"
        "board.c")

idf_component_register(SRCS "beep.c" "adc.c" "rc522.c" "${srcs}"
                    INCLUDE_DIRS  ".")
