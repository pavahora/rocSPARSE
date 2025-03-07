/*! \file */
/* ************************************************************************
 * Copyright (c) 2018-2021 Advanced Micro Devices, Inc.
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

#include "rocsparse_ellmv.hpp"

#include "definitions.h"
#include "ellmv_device.h"
#include "utility.h"

template <unsigned int BLOCKSIZE, typename I, typename T, typename U>
__launch_bounds__(BLOCKSIZE) ROCSPARSE_KERNEL void ellmvn_kernel(I m,
                                                                 I n,
                                                                 I ell_width,
                                                                 U alpha_device_host,
                                                                 const I* __restrict__ ell_col_ind,
                                                                 const T* __restrict__ ell_val,
                                                                 const T* __restrict__ x,
                                                                 U beta_device_host,
                                                                 T* __restrict__ y,
                                                                 rocsparse_index_base idx_base)
{
    auto alpha = load_scalar_device_host(alpha_device_host);
    auto beta  = load_scalar_device_host(beta_device_host);
    if(alpha != static_cast<T>(0) || beta != static_cast<T>(1))
    {
        ellmvn_device<BLOCKSIZE>(
            m, n, ell_width, alpha, ell_col_ind, ell_val, x, beta, y, idx_base);
    }
}

template <unsigned int BLOCKSIZE, typename I, typename T, typename U>
__launch_bounds__(BLOCKSIZE) ROCSPARSE_KERNEL void ellmvt_kernel(rocsparse_operation trans,
                                                                 I                   m,
                                                                 I                   n,
                                                                 I                   ell_width,
                                                                 U alpha_device_host,
                                                                 const I* __restrict__ ell_col_ind,
                                                                 const T* __restrict__ ell_val,
                                                                 const T* __restrict__ x,
                                                                 T* __restrict__ y,
                                                                 rocsparse_index_base idx_base)
{
    auto alpha = load_scalar_device_host(alpha_device_host);
    if(alpha != static_cast<T>(0))
    {
        ellmvt_device<BLOCKSIZE>(
            trans, m, n, ell_width, alpha, ell_col_ind, ell_val, x, y, idx_base);
    }
}

template <unsigned int BLOCKSIZE, typename I, typename T, typename U>
__launch_bounds__(BLOCKSIZE) ROCSPARSE_KERNEL
    void ellmvt_scale_kernel(I size, U scalar_device_host, T* __restrict__ data)
{
    auto scalar = load_scalar_device_host(scalar_device_host);
    if(scalar != static_cast<T>(1))
    {
        ellmvt_scale_device(size, scalar, data);
    }
}

template <typename I, typename T, typename U>
rocsparse_status rocsparse_ellmv_dispatch(rocsparse_handle          handle,
                                          rocsparse_operation       trans,
                                          I                         m,
                                          I                         n,
                                          U                         alpha_device_host,
                                          const rocsparse_mat_descr descr,
                                          const T*                  ell_val,
                                          const I*                  ell_col_ind,
                                          I                         ell_width,
                                          const T*                  x,
                                          U                         beta_device_host,
                                          T*                        y)
{
    // Stream
    hipStream_t stream = handle->stream;

    // Run different ellmv kernels
    if(trans == rocsparse_operation_none)
    {
#define ELLMVN_DIM 512
        ellmvn_kernel<ELLMVN_DIM>
            <<<(m - 1) / ELLMVN_DIM + 1, ELLMVN_DIM, 0, stream>>>(m,
                                                                  n,
                                                                  ell_width,
                                                                  alpha_device_host,
                                                                  ell_col_ind,
                                                                  ell_val,
                                                                  x,
                                                                  beta_device_host,
                                                                  y,
                                                                  descr->base);
#undef ELLMVN_DIM
    }
    else
    {
#define ELLMVT_DIM 1024
        // Scale y with beta
        ellmvt_scale_kernel<ELLMVT_DIM>
            <<<(n - 1) / ELLMVT_DIM + 1, ELLMVT_DIM, 0, stream>>>(n, beta_device_host, y);

        ellmvt_kernel<ELLMVT_DIM><<<(m - 1) / ELLMVT_DIM + 1, ELLMVT_DIM, 0, stream>>>(
            trans, m, n, ell_width, alpha_device_host, ell_col_ind, ell_val, x, y, descr->base);
#undef ELLMVT_DIM
    }

    return rocsparse_status_success;
}

template <typename I, typename T>
rocsparse_status rocsparse_ellmv_template(rocsparse_handle          handle,
                                          rocsparse_operation       trans,
                                          I                         m,
                                          I                         n,
                                          const T*                  alpha_device_host,
                                          const rocsparse_mat_descr descr,
                                          const T*                  ell_val,
                                          const I*                  ell_col_ind,
                                          I                         ell_width,
                                          const T*                  x,
                                          const T*                  beta_device_host,
                                          T*                        y)
{
    // Check for valid handle and matrix descriptor
    if(handle == nullptr)
    {
        return rocsparse_status_invalid_handle;
    }

    if(descr == nullptr)
    {
        return rocsparse_status_invalid_pointer;
    }

    // Logging
    log_trace(handle,
              replaceX<T>("rocsparse_Xellmv"),
              trans,
              m,
              n,
              LOG_TRACE_SCALAR_VALUE(handle, alpha_device_host),
              (const void*&)descr,
              (const void*&)ell_val,
              (const void*&)ell_col_ind,
              ell_width,
              (const void*&)x,
              LOG_TRACE_SCALAR_VALUE(handle, beta_device_host),
              (const void*&)y);

    log_bench(handle,
              "./rocsparse-bench -f ellmv -r",
              replaceX<T>("X"),
              "--mtx <matrix.mtx> ",
              "--alpha",
              LOG_BENCH_SCALAR_VALUE(handle, alpha_device_host),
              "--beta",
              LOG_BENCH_SCALAR_VALUE(handle, beta_device_host));

    if(rocsparse_enum_utils::is_invalid(trans))
    {
        return rocsparse_status_invalid_value;
    }

    if(descr->type != rocsparse_matrix_type_general)
    {
        // TODO
        return rocsparse_status_not_implemented;
    }

    // Check sizes
    if(m < 0 || n < 0 || ell_width < 0)
    {
        return rocsparse_status_invalid_size;
    }

    // Sanity check
    if((m == 0 || n == 0) && ell_width != 0)
    {
        return rocsparse_status_invalid_size;
    }

    // Quick return if possible
    if(m == 0 || n == 0 || ell_width == 0)
    {
        return rocsparse_status_success;
    }

    // Check pointer arguments
    if(alpha_device_host == nullptr || beta_device_host == nullptr)
    {
        return rocsparse_status_invalid_pointer;
    }

    if(handle->pointer_mode == rocsparse_pointer_mode_host
       && *alpha_device_host == static_cast<T>(0) && *beta_device_host == static_cast<T>(1))
    {
        return rocsparse_status_success;
    }

    // Check the rest of the pointer arguments
    if(ell_val == nullptr || ell_col_ind == nullptr || x == nullptr || y == nullptr)
    {
        return rocsparse_status_invalid_pointer;
    }

    if(handle->pointer_mode == rocsparse_pointer_mode_device)
    {
        return rocsparse_ellmv_dispatch(handle,
                                        trans,
                                        m,
                                        n,
                                        alpha_device_host,
                                        descr,
                                        ell_val,
                                        ell_col_ind,
                                        ell_width,
                                        x,
                                        beta_device_host,
                                        y);
    }
    else
    {
        return rocsparse_ellmv_dispatch(handle,
                                        trans,
                                        m,
                                        n,
                                        *alpha_device_host,
                                        descr,
                                        ell_val,
                                        ell_col_ind,
                                        ell_width,
                                        x,
                                        *beta_device_host,
                                        y);
    }
    return rocsparse_status_success;
}

#define INSTANTIATE(ITYPE, TTYPE)                                     \
    template rocsparse_status rocsparse_ellmv_template<ITYPE, TTYPE>( \
        rocsparse_handle          handle,                             \
        rocsparse_operation       trans,                              \
        ITYPE                     m,                                  \
        ITYPE                     n,                                  \
        const TTYPE*              alpha,                              \
        const rocsparse_mat_descr descr,                              \
        const TTYPE*              ell_val,                            \
        const ITYPE*              ell_col_ind,                        \
        ITYPE                     ell_width,                          \
        const TTYPE*              x,                                  \
        const TTYPE*              beta,                               \
        TTYPE*                    y);

INSTANTIATE(int32_t, float)
INSTANTIATE(int32_t, double)
INSTANTIATE(int32_t, rocsparse_float_complex)
INSTANTIATE(int32_t, rocsparse_double_complex)
INSTANTIATE(int64_t, float)
INSTANTIATE(int64_t, double)
INSTANTIATE(int64_t, rocsparse_float_complex)
INSTANTIATE(int64_t, rocsparse_double_complex)
#undef INSTANTIATE

/*
 * ===========================================================================
 *    C wrapper
 * ===========================================================================
 */

#define C_IMPL(NAME, TYPE)                                                                   \
    extern "C" rocsparse_status NAME(rocsparse_handle          handle,                       \
                                     rocsparse_operation       trans,                        \
                                     rocsparse_int             m,                            \
                                     rocsparse_int             n,                            \
                                     const TYPE*               alpha,                        \
                                     const rocsparse_mat_descr descr,                        \
                                     const TYPE*               ell_val,                      \
                                     const rocsparse_int*      ell_col_ind,                  \
                                     rocsparse_int             ell_width,                    \
                                     const TYPE*               x,                            \
                                     const TYPE*               beta,                         \
                                     TYPE*                     y)                            \
    {                                                                                        \
        return rocsparse_ellmv_template(                                                     \
            handle, trans, m, n, alpha, descr, ell_val, ell_col_ind, ell_width, x, beta, y); \
    }

C_IMPL(rocsparse_sellmv, float);
C_IMPL(rocsparse_dellmv, double);
C_IMPL(rocsparse_cellmv, rocsparse_float_complex);
C_IMPL(rocsparse_zellmv, rocsparse_double_complex);
#undef C_IMPL
