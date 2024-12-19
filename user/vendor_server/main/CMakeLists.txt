set(srcs "main.c"
        "board.c")

idf_component_register(SRCS "rc522.c" "${srcs}"
                    INCLUDE_DIRS  ".")
