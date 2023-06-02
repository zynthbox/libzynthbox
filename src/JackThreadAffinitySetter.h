/*
  ==============================================================================

    JackThreadAffinitySetter.h
    Created: 25 May 2023
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#pragma once

#include <jack/jack.h>
#include <pthread.h>
#include <errno.h>
#include <QDebug>

/**
 * \brief Set the thread affinity of the given jack client to our DSP cores
 * @param client The jack client that wants its affinity set
 */
void zl_set_jack_client_affinity(jack_client_t *client);

/**
 * \brief Set the thread affinity of a given pthread ID to our DSP cores
 * @param threadID The pthread ID of the thread that wants its affinity set
 */
void zl_set_dsp_thread_affinity(const pthread_t &threadID);
