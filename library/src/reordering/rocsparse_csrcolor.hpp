/*! \file */
/* ************************************************************************
 * Copyright (c) 2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ************************************************************************ */

#pragma once
#ifndef ROCSPARSE_CSRCOLOR_HPP
#define ROCSPARSE_CSRCOLOR_HPP

#include "utility.h"

template <typename T, typename I, typename J>
rocsparse_status rocsparse_csrcolor_dispatch(J                         m,
                                             I                         nnz,
                                             const rocsparse_mat_descr descr,
                                             const T*                  csr_val,
                                             const I*                  csr_row_ptr,
                                             const J*                  csr_col_ind,
                                             const floating_data_t<T>* fraction_to_color,
                                             J*                        ncolors,
                                             J*                        colors,
                                             J*                        reordering,
                                             rocsparse_color_info      info);

template <typename T, typename I = rocsparse_int, typename J = rocsparse_int>
rocsparse_status rocsparse_csrcolor_template(rocsparse_handle          handle,
                                             J                         m,
                                             I                         nnz,
                                             const rocsparse_mat_descr descr,
                                             const T*                  csr_val,
                                             const I*                  csr_row_ptr,
                                             const J*                  csr_col_ind,
                                             const floating_data_t<T>* fraction_to_color,
                                             J*                        ncolors,
                                             J*                        coloring,
                                             J*                        reordering,
                                             rocsparse_color_info      info);

#endif // ROCSPARSE_CSRCOLOR_HPP
