#include "life.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* outboard,
    char* inboard,
    const int nrows,
    const int ncols,
    const int gens_max)
{
  return threaded_gol (outboard, inboard, nrows, ncols, gens_max);
}

/*****************************************************************************
 * Package that we use to pass parameters to gol worker threads
 ****************************************************************************/
typedef struct Package {
  char * inboard;
  char * outboard;
  int nrows;
  int ncols;
  int start_row;
  int end_row;
} Package;

/*****************************************************************************
 * threaded_gol
 * Threaded version of the game of life
 ****************************************************************************/
  char*
threaded_gol (char* outboard,
    char* inboard,
    const int nrows,
    const int ncols,
    const int gens_max)
{
  int curgen, i, j;

  // Set up threading variables
  int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t tid[num_threads];
  Package *packages = malloc(num_threads*sizeof(Package));

  // Set up package for threads
  int start_row = 0;
  int chunk_size = nrows/num_threads;
  int end_row = chunk_size;
  for (i=0; i<num_threads; i++){
    packages[i].start_row = start_row+1;
    packages[i].end_row = end_row-1;

    // Update next start row and end row
    start_row = end_row;
    end_row = end_row + chunk_size;

    // Always need these
    packages[i].nrows = nrows;
    packages[i].ncols = ncols;
  }

  for (curgen = 0; curgen < gens_max; curgen++) {
    // Make sure the outboard and inboard is the same
    memcpy (outboard, inboard, nrows * ncols * sizeof (char));

    // Do the first and last lines of each chunk so that threads don't
    // overlap writing to the outboard
    for (i = 0; i < num_threads; i++) {
      gol_worker_for_row( packages[i].start_row-1, ncols, nrows, inboard, outboard);
      gol_worker_for_row( packages[i].end_row, ncols, nrows, inboard, outboard);
    }

    // Do the rest of the chunk now
    for (i = 0; i < num_threads; i++) {
      packages[i].inboard = inboard;
      packages[i].outboard = outboard;
      pthread_create(&tid[i], NULL, gol_worker, (void*) &packages[i]);
    }

    // Wait for threads to finish
    for (i = 0; i < num_threads; i++) {
      pthread_join(tid[i],NULL);
    }

    // Swap the boards to update the inboard
    SWAP_BOARDS(outboard, inboard);
  }

  // Format output so that it only shows dead or alive
    for (j = 0; j < ncols; j++) {
      int nrowsxj = nrows * j;
  for (i = 0; i < nrows; i++) {
      BOARD(inboard,i, nrowsxj) = BOARD(inboard,i,nrowsxj) >> 4;
    }
  }

  free(packages);
  return inboard;
}

/*****************************************************************************
 * gol_worker
 * Function for threads to run and update the inboard chunk size's states.
 * The cell's state is updated according to how many living neighbours it has.
 *
 * If a cell state changes, its neighbouring cells will be notified of an
 * updated count.
 ****************************************************************************/
void * gol_worker (void *ptr) {
  Package *p = (Package *) ptr;
  int start_row = p->start_row;
  int end_row = p->end_row;
  int ncols = p->ncols;
  int nrows = p->nrows;
  char * inboard = p-> inboard;
  char * outboard = p-> outboard;
  int i, j;

  for (j = 0; j < ncols; j++) {
    const int nrowsxj = nrows * j;
    for (i = start_row; i < end_row; i++) {
      char cell = BOARD(inboard,i, nrowsxj);
      // Check if the cell can come to life
      if (!IS_ALIVE(cell)) {
        if (cell == (char) 0x3) {
          SET_ALIVE(BOARD(outboard, i, nrowsxj));

          // Notify neighbours of spawn
          const int inorth = mod (i-1, nrows);
          const int isouth = mod (i+1, nrows);
          const int jwest = mod (j-1, ncols);
          const int jeast = mod (j+1, ncols);
          const int nrows_jwest = nrows * jwest;
          const int nrows_jeast = nrows * jeast;
          INCREMENT_NEIGHBOURS (outboard, inorth, nrows_jwest);
          INCREMENT_NEIGHBOURS (outboard, inorth, nrowsxj);
          INCREMENT_NEIGHBOURS (outboard, inorth, nrows_jeast);
          INCREMENT_NEIGHBOURS (outboard, i, nrows_jwest);
          INCREMENT_NEIGHBOURS (outboard, i, nrows_jeast);
          INCREMENT_NEIGHBOURS (outboard, isouth, nrows_jwest);
          INCREMENT_NEIGHBOURS (outboard, isouth, nrowsxj);
          INCREMENT_NEIGHBOURS (outboard, isouth, nrows_jeast);
        }
      } else {
        // Check if the cell needs to die
        if (cell <= (char) 0x11 || cell >= (char) 0x14) {
          SET_DEAD(BOARD(outboard, i, nrowsxj));

          // Notify neighbours of death
          const int inorth = mod (i-1, nrows);
          const int isouth = mod (i+1, nrows);
          const int jwest = mod (j-1, ncols);
          const int jeast = mod (j+1, ncols);
          const int nrows_jwest = nrows * jwest;
          const int nrows_jeast = nrows * jeast;
          DECREMENT_NEIGHBOURS (outboard, inorth, nrows_jwest);
          DECREMENT_NEIGHBOURS (outboard, inorth, nrowsxj);
          DECREMENT_NEIGHBOURS (outboard, inorth, nrows_jeast);
          DECREMENT_NEIGHBOURS (outboard, i, nrows_jwest);
          DECREMENT_NEIGHBOURS (outboard, i, nrows_jeast);
          DECREMENT_NEIGHBOURS (outboard, isouth, nrows_jwest);
          DECREMENT_NEIGHBOURS (outboard, isouth, nrowsxj);
          DECREMENT_NEIGHBOURS (outboard, isouth, nrows_jeast);
        }
      }
    }
  }

  pthread_exit(NULL);
}


/*****************************************************************************
 * gol_worker_for_row
 * Special case of the above function in case we need to process a row by itself
 ****************************************************************************/
void gol_worker_for_row (int i, int ncols, int nrows, char * inboard, char * outboard) {
  int j;
  for (j = 0; j < ncols; j++) {
    const int nrowsxj = nrows * j;
    char cell = BOARD(inboard, i, nrowsxj);
    // Check if the cell can come to life
    if (!IS_ALIVE(cell)) {
      if (cell == (char) 0x3) {
        SET_ALIVE(BOARD(outboard, i, nrowsxj));
        // Notify neighbours of spawn
        const int inorth = mod (i-1, nrows);
        const int isouth = mod (i+1, nrows);
        const int jwest = mod (j-1, ncols);
        const int jeast = mod (j+1, ncols);
        const int nrows_jwest = nrows * jwest;
        const int nrows_jeast = nrows * jeast;
        INCREMENT_NEIGHBOURS (outboard, inorth, nrows_jwest);
        INCREMENT_NEIGHBOURS (outboard, inorth, nrowsxj);
        INCREMENT_NEIGHBOURS (outboard, inorth, nrows_jeast);
        INCREMENT_NEIGHBOURS (outboard, i, nrows_jwest);
        INCREMENT_NEIGHBOURS (outboard, i, nrows_jeast);
        INCREMENT_NEIGHBOURS (outboard, isouth, nrows_jwest);
        INCREMENT_NEIGHBOURS (outboard, isouth, nrowsxj);
        INCREMENT_NEIGHBOURS (outboard, isouth, nrows_jeast);
      }
    } else {
      // Check if the cell needs to die
      if (cell <= (char) 0x11 || cell >= (char) 0x14) {
        SET_DEAD(BOARD(outboard, i, nrowsxj));

        // Notify neighbours of death
        const int inorth = mod (i-1, nrows);
        const int isouth = mod (i+1, nrows);
        const int jwest = mod (j-1, ncols);
        const int jeast = mod (j+1, ncols);
        const int nrows_jwest = nrows * jwest;
        const int nrows_jeast = nrows * jeast;
        DECREMENT_NEIGHBOURS (outboard, inorth, nrows_jwest);
        DECREMENT_NEIGHBOURS (outboard, inorth, nrowsxj);
        DECREMENT_NEIGHBOURS (outboard, inorth, nrows_jeast);
        DECREMENT_NEIGHBOURS (outboard, i, nrows_jwest);
        DECREMENT_NEIGHBOURS (outboard, i, nrows_jeast);
        DECREMENT_NEIGHBOURS (outboard, isouth, nrows_jwest);
        DECREMENT_NEIGHBOURS (outboard, isouth, nrowsxj);
        DECREMENT_NEIGHBOURS (outboard, isouth, nrows_jeast);
      }
    }
  }
}
