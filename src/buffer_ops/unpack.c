/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2012      Los Alamos National Security, Inc.  All rights reserved. 
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "pmix_config.h"
#include "src/include/types.h"
#include "src/util/argv.h"
#include "src/util/error.h"
#include "src/util/output.h"
#include "src/buffer_ops/types.h"
#include "src/buffer_ops/internal.h"

int pmix_bfrop_unpack(pmix_buffer_t *buffer, void *dst, int32_t *num_vals,
                      pmix_data_type_t type)
{
    int rc, ret;
    int32_t local_num, n=1;
    pmix_data_type_t local_type;

    /* check for error */
    if (NULL == buffer || NULL == dst || NULL == num_vals) {
        return PMIX_ERR_BAD_PARAM;
    }

    /* if user provides a zero for num_vals, then there is no storage allocated
     * so return an appropriate error
     */
    if (0 == *num_vals) {
        PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack: inadequate space ( %p, %p, %lu, %d )\n",
                       (void*)buffer, dst, (long unsigned int)*num_vals, (int)type ) );
        return PMIX_ERR_UNPACK_INADEQUATE_SPACE;
    }

    /** Unpack the declared number of values
     * REMINDER: it is possible that the buffer is corrupted and that
     * the BFROP will *think* there is a proper int32_t variable at the
     * beginning of the unpack region - but that the value is bogus (e.g., just
     * a byte field in a string array that so happens to have a value that
     * matches the int32_t data type flag). Therefore, this error check is
     * NOT completely safe. This is true for ALL unpack functions, not just
     * int32_t as used here.
     */
    if (PMIX_BFROP_BUFFER_FULLY_DESC == buffer->type) {
        if (PMIX_SUCCESS != (
                             rc = pmix_bfrop_get_data_type(buffer, &local_type))) {
            *num_vals = 0;
            return rc;
        }
        if (PMIX_INT32 != local_type) { /* if the length wasn't first, then error */
            *num_vals = 0;
            return PMIX_ERR_UNPACK_FAILURE;
        }
    }

    n=1;
    if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_int32(buffer, &local_num, &n, PMIX_INT32))) {
        *num_vals = 0;
        return rc;
    }

    /** if the storage provided is inadequate, set things up
     * to unpack as much as we can and to return an error code
     * indicating that everything was not unpacked - the buffer
     * is left in a state where it can not be further unpacked.
     */
    if (local_num > *num_vals) {
        local_num = *num_vals;
        PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack: inadequate space ( %p, %p, %lu, %d )\n",
                       (void*)buffer, dst, (long unsigned int)*num_vals, (int)type ) );
        ret = PMIX_ERR_UNPACK_INADEQUATE_SPACE;
    } else {  /** enough or more than enough storage */
        *num_vals = local_num;  /** let the user know how many we actually unpacked */
        ret = PMIX_SUCCESS;
    }

    /** Unpack the value(s) */
    if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_buffer(buffer, dst, &local_num, type))) {
        *num_vals = 0;
        ret = rc;
    }

    return ret;
}

int pmix_bfrop_unpack_buffer(pmix_buffer_t *buffer, void *dst, int32_t *num_vals,
                             pmix_data_type_t type)
{
    int rc;
    pmix_data_type_t local_type;
    pmix_bfrop_type_info_t *info;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_buffer( %p, %p, %lu, %d )\n",
                   (void*)buffer, dst, (long unsigned int)*num_vals, (int)type ) );

    /** Unpack the declared data type */
    if (PMIX_BFROP_BUFFER_FULLY_DESC == buffer->type) {
        if (PMIX_SUCCESS != (rc = pmix_bfrop_get_data_type(buffer, &local_type))) {
            return rc;
        }
        /* if the data types don't match, then return an error */
        if (type != local_type) {
            pmix_output(0, "PMIX bfrop:unpack: got type %d when expecting type %d", local_type, type);
            return PMIX_ERR_PACK_MISMATCH;
        }
    }

    /* Lookup the unpack function for this type and call it */

    if (NULL == (info = (pmix_bfrop_type_info_t*)pmix_pointer_array_get_item(&pmix_bfrop_types, type))) {
        return PMIX_ERR_UNPACK_FAILURE;
    }

    return info->odti_unpack_fn(buffer, dst, num_vals, type);
}


/* UNPACK GENERIC SYSTEM TYPES */

/*
 * INT
 */
int pmix_bfrop_unpack_int(pmix_buffer_t *buffer, void *dest,
                          int32_t *num_vals, pmix_data_type_t type)
{
    int ret;
    pmix_data_type_t remote_type;

    if (PMIX_SUCCESS != (ret = pmix_bfrop_get_data_type(buffer, &remote_type))) {
        return ret;
    }

    if (remote_type == BFROP_TYPE_INT) {
        /* fast path it if the sizes are the same */
        /* Turn around and unpack the real type */
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, dest, num_vals, BFROP_TYPE_INT))) {
        }
    } else {
        /* slow path - types are different sizes */
        UNPACK_SIZE_MISMATCH(int, remote_type, ret);
    }
    
    return ret;
}

/*
 * SIZE_T
 */
int pmix_bfrop_unpack_sizet(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    int ret;
    pmix_data_type_t remote_type;

    if (PMIX_SUCCESS != (ret = pmix_bfrop_get_data_type(buffer, &remote_type))) {
        return ret;
    }

    if (remote_type == BFROP_TYPE_SIZE_T) {
        /* fast path it if the sizes are the same */
        /* Turn around and unpack the real type */
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, dest, num_vals, BFROP_TYPE_SIZE_T))) {
        }
    } else {
        /* slow path - types are different sizes */
        UNPACK_SIZE_MISMATCH(size_t, remote_type, ret);
    }
    
    return ret;
}

/*
 * PID_T
 */
int pmix_bfrop_unpack_pid(pmix_buffer_t *buffer, void *dest,
                          int32_t *num_vals, pmix_data_type_t type)
{
    int ret;
    pmix_data_type_t remote_type;

    if (PMIX_SUCCESS != (ret = pmix_bfrop_get_data_type(buffer, &remote_type))) {
        return ret;
    }

    if (remote_type == BFROP_TYPE_PID_T) {
        /* fast path it if the sizes are the same */
        /* Turn around and unpack the real type */
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, dest, num_vals, BFROP_TYPE_PID_T))) {
        }
    } else {
        /* slow path - types are different sizes */
        UNPACK_SIZE_MISMATCH(pid_t, remote_type, ret);
    }
    
    return ret;
}


/* UNPACK FUNCTIONS FOR NON-GENERIC SYSTEM TYPES */

/*
 * BYTE, CHAR, INT8
 */
int pmix_bfrop_unpack_byte(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals, pmix_data_type_t type)
{
    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_byte * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, *num_vals)) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    memcpy(dest, buffer->unpack_ptr, *num_vals);

    /* update buffer pointer */
    buffer->unpack_ptr += *num_vals;

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_int16(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i;
    uint16_t tmp, *desttmp = (uint16_t*) dest;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_int16 * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(tmp))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        memcpy( &(tmp), buffer->unpack_ptr, sizeof(tmp) );
        tmp = ntohs(tmp);
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
        buffer->unpack_ptr += sizeof(tmp);
    }

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_int32(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i;
    uint32_t tmp, *desttmp = (uint32_t*) dest;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_int32 * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(tmp))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        memcpy( &(tmp), buffer->unpack_ptr, sizeof(tmp) );
        tmp = ntohl(tmp);
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
        buffer->unpack_ptr += sizeof(tmp);
    }

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_datatype(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    return pmix_bfrop_unpack_int32(buffer, dest, num_vals, type);
}

int pmix_bfrop_unpack_int64(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i;
    uint64_t tmp, *desttmp = (uint64_t*) dest;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_int64 * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(tmp))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        memcpy( &(tmp), buffer->unpack_ptr, sizeof(tmp) );
        tmp = ntoh64(tmp);
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
        buffer->unpack_ptr += sizeof(tmp);
    }

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_string(pmix_buffer_t *buffer, void *dest,
                             int32_t *num_vals, pmix_data_type_t type)
{
    int ret;
    int32_t i, len, n=1;
    char **sdest = (char**) dest;

    for (i = 0; i < (*num_vals); ++i) {
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int32(buffer, &len, &n, PMIX_INT32))) {
            return ret;
        }
        if (0 ==  len) {   /* zero-length string - unpack the NULL */
            sdest[i] = NULL;
        } else {
            sdest[i] = (char*)malloc(len);
            if (NULL == sdest[i]) {
                return PMIX_ERR_OUT_OF_RESOURCE;
            }
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_byte(buffer, sdest[i], &len, PMIX_BYTE))) {
                return ret;
            }
        }
    }

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_float(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i, n;
    float *desttmp = (float*) dest, tmp;
    int ret;
    char *convert;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_float * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(float))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        n=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &convert, &n, PMIX_STRING))) {
            return ret;
        }
        tmp = strtof(convert, NULL);
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
        free(convert);
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_double(pmix_buffer_t *buffer, void *dest,
                             int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i, n;
    double *desttmp = (double*) dest, tmp;
    int ret;
    char *convert;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_double * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(double))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        n=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &convert, &n, PMIX_STRING))) {
            return ret;
        }
        tmp = strtod(convert, NULL);
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
        free(convert);
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_timeval(pmix_buffer_t *buffer, void *dest,
                              int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i, n;
    int64_t tmp[2];
    struct timeval *desttmp = (struct timeval *) dest, tt;
    int ret;

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_timeval * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*sizeof(struct timeval))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        n=2;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int64(buffer, tmp, &n, PMIX_INT64))) {
            return ret;
        }
        tt.tv_sec = tmp[0];
        tt.tv_usec = tmp[1];
        memcpy(&desttmp[i], &tt, sizeof(tt));
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_time(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals, pmix_data_type_t type)
{
    int32_t i, n;
    time_t *desttmp = (time_t *) dest, tmp;
    int ret;
    uint64_t ui64;

    /* time_t is a system-dependent size, so cast it
     * to uint64_t as a generic safe size
     */

    PMIX_OUTPUT( ( pmix_bfrop_verbose, "pmix_bfrop_unpack_time * %d\n", (int)*num_vals ) );
    /* check to see if there's enough data in buffer */
    if (pmix_bfrop_too_small(buffer, (*num_vals)*(sizeof(uint64_t)))) {
        return PMIX_ERR_UNPACK_READ_PAST_END_OF_BUFFER;
    }

    /* unpack the data */
    for (i = 0; i < (*num_vals); ++i) {
        n=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int64(buffer, &ui64, &n, PMIX_UINT64))) {
            return ret;
        }
        tmp = (time_t)ui64;
        memcpy(&desttmp[i], &tmp, sizeof(tmp));
    }
    return PMIX_SUCCESS;
}


/* UNPACK FUNCTIONS FOR GENERIC PMIX TYPES */

/*
 * PMIX_VALUE
 */
static int unpack_val(pmix_buffer_t *buffer, pmix_value_t *val)
{
    int ret, m;

    m = 1;
    switch (val->type) {
    case PMIX_BYTE:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.byte, &m, PMIX_BYTE))) {
            return ret;
        }
        break;
    case PMIX_STRING:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.string, &m, PMIX_STRING))) {
            return ret;
        }
        break;
    case PMIX_SIZE:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.size, &m, PMIX_SIZE))) {
            return ret;
        }
        break;
    case PMIX_PID:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.pid, &m, PMIX_PID))) {
            return ret;
        }
        break;
    case PMIX_INT:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.integer, &m, PMIX_INT))) {
            return ret;
        }
        break;
    case PMIX_INT8:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.int8, &m, PMIX_INT8))) {
            return ret;
        }
        break;
    case PMIX_INT16:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.int16, &m, PMIX_INT16))) {
            return ret;
        }
        break;
    case PMIX_INT32:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.int32, &m, PMIX_INT32))) {
            return ret;
        }
        break;
    case PMIX_INT64:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.int64, &m, PMIX_INT64))) {
            return ret;
        }
        break;
    case PMIX_UINT:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.uint, &m, PMIX_UINT))) {
            return ret;
        }
        break;
    case PMIX_UINT8:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.uint8, &m, PMIX_UINT8))) {
            return ret;
        }
        break;
    case PMIX_UINT16:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.uint16, &m, PMIX_UINT16))) {
            return ret;
        }
        break;
    case PMIX_UINT32:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.uint32, &m, PMIX_UINT32))) {
            return ret;
        }
        break;
    case PMIX_UINT64:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.uint64, &m, PMIX_UINT64))) {
            return ret;
        }
        break;
    case PMIX_FLOAT:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.fval, &m, PMIX_FLOAT))) {
            return ret;
        }
        break;
    case PMIX_DOUBLE:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.dval, &m, PMIX_DOUBLE))) {
            return ret;
        }
        break;
    case PMIX_TIMEVAL:
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.tv, &m, PMIX_TIMEVAL))) {
            return ret;
        }
        break;
    case PMIX_ARRAY:
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.array.type, &m, PMIX_INT))) {
            return ret;
        }
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_buffer(buffer, &val->data.array.size, &m, PMIX_SIZE))) {
            return ret;
        }
    default:
        pmix_output(0, "PACK-PMIX-VALUE: UNSUPPORTED TYPE");
        return PMIX_ERROR;
    }

    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_value(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals, pmix_data_type_t type)
{
    pmix_info_t **ptr;
    int32_t i, n;
    int ret;

    ptr = (pmix_info_t **) dest;
    n = *num_vals;

    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = (pmix_info_t*)malloc(sizeof(pmix_value_t));
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack value */
        if (PMIX_SUCCESS != (ret = unpack_val(buffer,&ptr[i]->value)) ) {
            return ret;
        }
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_info(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals, pmix_data_type_t type)
{
    pmix_info_t **ptr;
    int32_t i, n, m;
    int ret;
    char *tmp;
    
    ptr = (pmix_info_t **) dest;
    n = *num_vals;
    
    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = (pmix_info_t*)malloc(sizeof(pmix_info_t));
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack key */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
            return ret;
        }
        (void)strncpy(ptr[i]->key, tmp, PMIX_MAX_VALLEN);
        free(tmp);
        /* unpack value */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_value(buffer, &ptr[i]->value, &m, PMIX_VALUE))) {
            return ret;
        }
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_buf(pmix_buffer_t *buffer, void *dest,
                          int32_t *num_vals, pmix_data_type_t type)
{
    pmix_buffer_t **ptr;
    int32_t i, n, m;
    int ret;
    size_t nbytes;

    ptr = (pmix_buffer_t **) dest;
    n = *num_vals;
    
    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = OBJ_NEW(pmix_buffer_t);
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack the number of bytes */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_sizet(buffer, &nbytes, &m, PMIX_SIZE))) {
            return ret;
        }
        m = nbytes;
        /* setup the buffer's data region */
        if (0 < nbytes) {
            ptr[i]->base_ptr = (char*)malloc(nbytes);
            /* unpack the bytes */
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_byte(buffer, ptr[i]->base_ptr, &m, PMIX_BYTE))) {
                return ret;
            }
        }
        ptr[i]->pack_ptr = ptr[i]->base_ptr + m;
        ptr[i]->unpack_ptr = ptr[i]->base_ptr;
        ptr[i]->bytes_allocated = nbytes;
        ptr[i]->bytes_used = m;
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_range(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    pmix_range_t **ptr;
    int32_t i, n, m;
    int ret;
    char *tmp;
    
    ptr = (pmix_range_t **) dest;
    n = *num_vals;
    
    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = (pmix_range_t*)malloc(sizeof(pmix_range_t));
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack namespace */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
            return ret;
        }
        (void)strncpy(ptr[i]->namespace, tmp, PMIX_MAX_NSLEN);
        free(tmp);
        /* unpack the number of ranks */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_sizet(buffer, &ptr[i]->nranks, &m, PMIX_SIZE))) {
            return ret;
        }
        if (0 < ptr[i]->nranks) {
            ptr[i]->ranks = (int*)malloc(ptr[i]->nranks * sizeof(int));
            m=ptr[i]->nranks;
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_sizet(buffer, &ptr[i]->ranks, &m, PMIX_INT))) {
                return ret;
            }
        }
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_app(pmix_buffer_t *buffer, void *dest,
                          int32_t *num_vals, pmix_data_type_t type)
{
    pmix_app_t **ptr;
    int32_t i, k, n, m;
    int ret;
    int32_t nval;
    char *tmp;
    
    ptr = (pmix_app_t **) dest;
    n = *num_vals;
    
    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = (pmix_app_t*)malloc(sizeof(pmix_app_t));
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack cmd */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &ptr[i]->cmd, &m, PMIX_STRING))) {
            return ret;
        }
        /* unpack argc */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int(buffer, &ptr[i]->argc, &m, PMIX_INT))) {
            return ret;
        }
        /* unpack argv */
        for (k=0; k < ptr[i]->argc; k++) {
            m=1;
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
                return ret;
            }
            pmix_argv_append_nosize(&ptr[i]->argv, tmp);
            free(tmp);
        }
        /* unpack env */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int(buffer, &nval, &m, PMIX_INT))) {
            return ret;
        }
        for (k=0; k < nval; k++) {
            m=1;
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
                return ret;
            }
            pmix_argv_append_nosize(&ptr[i]->argv, tmp);
            free(tmp);
        }
        /* unpack maxprocs */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int(buffer, &ptr[i]->maxprocs, &m, PMIX_INT))) {
            return ret;
        }
        /* unpack info array */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_sizet(buffer, &ptr[i]->ninfo, &m, PMIX_SIZE))) {
            return ret;
        }
        if (0 < ptr[i]->ninfo) {
            ptr[i]->info = (pmix_info_t*)malloc(ptr[i]->ninfo * sizeof(pmix_info_t));
            for (k=0; k < (int32_t)ptr[i]->ninfo; k++) {
                m=1;
                if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
                    return ret;
                }
                (void)strncpy(ptr[i]->info[k].key, tmp, PMIX_MAX_VALLEN);
                free(tmp);
                m=1;
                if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_value(buffer, &(ptr[i]->info[k].value), &m, PMIX_VALUE))) {
                    return ret;
                }
            }
        }
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_kval(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals, pmix_data_type_t type)
{
    pmix_kval_t **ptr;
    int32_t i, n, m;
    int ret;

    ptr = (pmix_kval_t **) dest;
    n = *num_vals;

    for (i = 0; i < n; ++i) {
        ptr[i] = OBJ_NEW(pmix_kval_t);
        /* pack the key */
        m = 1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &ptr[i]->key, &m, PMIX_STRING))) {
            return ret;
        }

        ptr[i]->value = malloc(sizeof(pmix_value_t));
        /* pack the type */
        m = 1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int(buffer, &ptr[i]->value->type, &m, PMIX_INT))) {
            return ret;
        }
        /* now pack the right field */
        if (PMIX_SUCCESS != (ret = unpack_val(buffer, ptr[i]->value))) {
            return ret;
        }
    }
    return PMIX_SUCCESS;
}

int pmix_bfrop_unpack_array(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    // No functional yet.
    abort();
}

#if PMIX_HAVE_HWLOC
int pmix_bfrop_unpack_topo(pmix_buffer_t *buffer, void *dest,
                           int32_t *num_vals,
                           pmix_data_type_t type)
{
    /* NOTE: hwloc defines topology_t as a pointer to a struct! */
    hwloc_topology_t t, *tarray  = (hwloc_topology_t*)dest;
    int rc=PMIX_SUCCESS, i, cnt, j;
    char *xmlbuffer=NULL;
    struct hwloc_topology_support *support;

    for (i=0, j=0; i < *num_vals; i++) {
        /* unpack the xml string */
        cnt=1;
        if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_string(buffer, &xmlbuffer, &cnt, PMIX_STRING))) {
            goto cleanup;
        }

        /* convert the xml */
        if (0 != hwloc_topology_init(&t)) {
            rc = PMIX_ERROR;
            goto cleanup;
        }
        if (0 != hwloc_topology_set_xmlbuffer(t, xmlbuffer, strlen(xmlbuffer))) {
            rc = PMIX_ERROR;
            free(xmlbuffer);
            hwloc_topology_destroy(t);
            goto cleanup;
        }
        /* since we are loading this from an external source, we have to
         * explicitly set a flag so hwloc sets things up correctly
         */
        if (0 != hwloc_topology_set_flags(t, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM | HWLOC_TOPOLOGY_FLAG_IO_DEVICES)) {
            free(xmlbuffer);
            rc = PMIX_ERROR;
            hwloc_topology_destroy(t);
            goto cleanup;
        }
        /* now load the topology */
        if (0 != hwloc_topology_load(t)) {
            free(xmlbuffer);
            rc = PMIX_ERROR;
            hwloc_topology_destroy(t);
            goto cleanup;
        }
        if (NULL != xmlbuffer) {
            free(xmlbuffer);
        }

        /* get the available support - hwloc unfortunately does
         * not include this info in its xml import!
         */
        support = (struct hwloc_topology_support*)hwloc_topology_get_support(t);
        cnt = sizeof(struct hwloc_topology_discovery_support);
        if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_byte(buffer, support->discovery, &cnt, PMIX_BYTE))) {
            goto cleanup;
        }
        cnt = sizeof(struct hwloc_topology_cpubind_support);
        if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_byte(buffer, support->cpubind, &cnt, PMIX_BYTE))) {
            goto cleanup;
        }
        cnt = sizeof(struct hwloc_topology_membind_support);
        if (PMIX_SUCCESS != (rc = pmix_bfrop_unpack_byte(buffer, support->membind, &cnt, PMIX_BYTE))) {
            goto cleanup;
        }

        /* pass it back */
        tarray[i] = t;

        /* track the number added */
        j++;
    }

 cleanup:
    *num_vals = j;
    return rc;
}
#endif

int pmix_bfrop_unpack_modex(pmix_buffer_t *buffer, void *dest,
                            int32_t *num_vals, pmix_data_type_t type)
{
    pmix_modex_data_t **ptr;
    int32_t i, n, m;
    int ret;
    char *tmp;
    
    ptr = (pmix_modex_data_t **) dest;
    n = *num_vals;
    
    for (i = 0; i < n; ++i) {
        /* allocate the new object */
        ptr[i] = (pmix_modex_data_t*)malloc(sizeof(pmix_modex_data_t));
        if (NULL == ptr[i]) {
            return PMIX_ERR_OUT_OF_RESOURCE;
        }
        /* unpack namespace */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_string(buffer, &tmp, &m, PMIX_STRING))) {
            return ret;
        }
        (void)strncpy(ptr[i]->namespace, tmp, PMIX_MAX_NSLEN);
        free(tmp);
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_int(buffer, &ptr[i]->rank, &m, PMIX_INT))) {
            return ret;
        }
        /* unpack the number of bytes */
        m=1;
        if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_sizet(buffer, &ptr[i]->size, &m, PMIX_SIZE))) {
            return ret;
        }
        if (0 < ptr[i]->size) {
            ptr[i]->blob = (uint8_t*)malloc(ptr[i]->size * sizeof(uint8_t));
            m=ptr[i]->size;
            if (PMIX_SUCCESS != (ret = pmix_bfrop_unpack_byte(buffer, &ptr[i]->blob, &m, PMIX_UINT8))) {
                return ret;
            }
        }
    }
    return PMIX_SUCCESS;
}
