/*
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * Author: Angus Salkeld <asalkeld@redhat.com>
 *
 * This file is part of libqb.
 *
 * libqb is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * libqb is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libqb.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef QB_RB_DEFINED
#define QB_RB_DEFINED

#include <sys/types.h>
#include <stdint.h>

/**
 * @file qbrb.h
 * @author Angus Salkeld <asalkeld@redhat.com>
 *
 * This implements a ring buffer that works in "chunks" not bytes.
 * So you write/read a complete chunk or not at all.
 * There are two types of ring buffer normal and overwrite.
 * Overwrite will reclaim the oldest chunks inorder to make way for new ones,
 * the normal version will refuse to write a new chunk if the ring buffer
 * is full.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * create a ring buffer (rather than open and existing one)
 * @see qb_rb_open()
 */
#define QB_RB_FLAG_CREATE		0x01
/**
 * New calls to qb_rb_chunk_write() will call qb_rb_chunk_reclaim()
 * if there is not enough space.
 * If this is not set then new writes will be refused.
 * @see qb_rb_open()
 */
#define QB_RB_FLAG_OVERWRITE		0x02
/**
 * The ringbuffer will be shared between pthreads not processes.
 * This effects the type of locks/semaphores that are used.
 * @see qb_rb_open()
 */
#define QB_RB_FLAG_SHARED_THREAD	0x03
/**
 * The ringbuffer will be shared between processes.
 * This effects the type of locks/semaphores that are used.
 * @see qb_rb_open()
 */
#define QB_RB_FLAG_SHARED_PROCESS	0x04
/*
PTHREAD_LOCK_SHARED
PROCESS_LOCK_SHARED
SEMAPHORE_SIGNAL
FD_SIGNAL
*/
struct qb_ringbuffer_s;
typedef struct qb_ringbuffer_s qb_ringbuffer_t;

/**
 * Create the ring buffer with the given type.
 *
 * This creates allocates a ring buffer in shared memory.
 *
 * @param name the unique name of this ringbuffer.
 * @param size the requested size.
 * @param flags or'ed flags
 * @note the actual size will be rounded up to the next page size.
 * @return a new ring buffer or NULL if there was a problem.
 * @see QB_RB_FLAG_CREATE, QB_RB_FLAG_OVERWRITE, QB_RB_FLAG_SHARED_THREAD, QB_RB_FLAG_SHARED_PROCESS
 */
qb_ringbuffer_t* qb_rb_open (const char *name, size_t size, uint32_t flags);

/**
 * Dereference the ringbuffer and if we are the last user destroy it.
 *
 * All files, mmaped memory, semaphores and locks will be destroyed.
 *
 * @param rb ringbuffer instance
 */
void qb_rb_close (qb_ringbuffer_t *rb);

/**
 * Write a chunk to the ring buffer.
 *
 * This simply calls qb_rb_chunk_writable_alloc() and then
 * qb_rb_chunk_writable_commit().
 *
 * @param rb ringbuffer instance
 * @param data (in) the data to write
 * @param len (in) the size of the chunk.
 * @return the amount of bytes actually buffered (either len or -1).
 *
 * @see qb_rb_chunk_writable_alloc()
 * @see qb_rb_chunk_writable_commit()
 */
ssize_t qb_rb_chunk_write (qb_ringbuffer_t *rb, const void* data, size_t len);

/**
 * Allocate space for a chunk of the given size.
 *
 * If type == QB_RB_OVERWRITE then this will always return non-null
 * but if it's type is QB_RB_NORMAL then when there is not enough space then
 * it will return NULL.
 *
 * @param rb ringbuffer instance
 * @param len (in) the size to allocate.
 * @return pointer to chunk to write to, or NULL (if no space).
 *
 * @see qb_rb_chunk_writable_alloc()
 */
void* qb_rb_chunk_writable_alloc (qb_ringbuffer_t *rb, size_t len);

/**
 * finalize the chunk.
 * @param rb ringbuffer instance
 * @param len (in) the size of the chunk.
 */
int32_t qb_rb_chunk_writable_commit (qb_ringbuffer_t *rb, size_t len);

/**
 * Read (without reclaiming) the last chunk.
 *
 * This function is a way of accessing the next chunk without a memcpy().
 * You can read the chunk data in place.
 *
 * @note This function will not "pop" the chunk, you will need to call
 * qb_rb_chunk_reclaim().
 * @param rb ringbuffer instance
 * @param data_out (out) a pointer to the next chunk to read (not copied).
 *
 * @return the size of the chunk (0 if buffer empty).
 */
ssize_t qb_rb_chunk_peek (qb_ringbuffer_t *rb, void **data_out);

/**
 * Reclaim the oldest chunk.
 * You will need to call this if using qb_rb_chunk_peek(). 
 * @param rb ringbuffer instance
 */
void qb_rb_chunk_reclaim (qb_ringbuffer_t *rb);

/**
 * Read the oldest chunk into data_out.
 *
 * This is the same as qb_rb_chunk_peek() memcpy() and qb_rb_chunk_reclaim().
 *
 * @param rb ringbuffer instance
 * @param data_out (in/out) the chunk will be memcpy'ed into this.
 * @param len (in) the size of data_out.
 * @param ms_timeout the amount od time to wait for new data.
 * @return the size of the chunk, or error.
 */
ssize_t qb_rb_chunk_read (qb_ringbuffer_t *rb, void *data_out, size_t len, int32_t ms_timeout);

/**
 * The amount of free space in the ring buffer.
 *
 * @note Some of this space will be consumed by the chunk headers.
 * @param rb ringbuffer instance
 */
ssize_t qb_rb_space_free (qb_ringbuffer_t *rb);

/**
 * The total amount of data in the buffer.
 *
 * @note This includes the chunk headers (8 bytes per chunk).
 * @param rb ringbuffer instance
 */
ssize_t qb_rb_space_used (qb_ringbuffer_t *rb);

/**
 * Write the contents of the Ring Buffer to file.
 * @param fd open file to write the ringbuffer data to.
 * @param rb ringbuffer instance
 * @see qb_rb_create_from_file()
 */
ssize_t qb_rb_write_to_file (qb_ringbuffer_t *rb, int32_t fd);

/**
 * Load the saved ring buffer from file into tempory memory.
 * @param fd file with saved ringbuffer data.
 * @param flags same flags as passed into qb_rb_open()
 * @return new ringbuffer instance
 * @see qb_rb_write_to_file()
 */
qb_ringbuffer_t *qb_rb_create_from_file (int32_t fd, uint32_t flags);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* QB_RB_DEFINED */

