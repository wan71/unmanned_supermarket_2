set(srcs "main.c"
        "board.c")

idf_component_register(SRCS "onenet.c" "${srcs}" "smartconfig.c" "onenet.c"
                    INCLUDE_DIRS  "."
                    )
