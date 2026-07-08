/***************************************************************************
 * Copyright (c) Johan Mabille, Sylvain Corlay and Wolf Vollprecht          *
 * Copyright (c) QuantStack                                                 *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#ifndef XTL_XHALF_FLOAT_HPP
#define XTL_XHALF_FLOAT_HPP

#include "xhalf_float_impl.hpp"
#include "xtype_traits.hpp"

namespace xtl {
using half_float = half_float::half;

template <>
struct is_scalar<half_float> : std::true_type {};

template <>
struct is_arithmetic<half_float> : std::true_type {};

template <>
struct is_signed<half_float> : std::true_type {};

template <>
struct is_floating_point<half_float> : std::true_type {};
}  // namespace xtl

#endif
