# SPDX-License-Identifier: MIT
idf_component_register(SRCS main.c PRIV_REQUIRES wamr pthread EMBED_FILES "$ENV{MOD}")
