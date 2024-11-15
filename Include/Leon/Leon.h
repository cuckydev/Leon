/*
 * [ Leon ]
 *   Source/Leon.h
 * Author(s): Regan Green
 * Date: 2024-02-13
 *
 * Copyright (C) 2024 Regan "CKDEV" Green
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#ifdef _LEON_PROC

#define LEON __attribute__((annotate("@leon")))
#define LEON_KV(key, value) __attribute__((annotate("@leonkv " #key " " #value)))
#define LEON_V(value) __attribute__((annotate("@leonkv " #value " \"true\"")))

#else

#define LEON
#define LEON_KV(...)
#define LEON_V(...)

#endif
