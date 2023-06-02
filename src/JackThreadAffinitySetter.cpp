/*
  ==============================================================================

    JackThreadAffinitySetter.h
    Created: 25 May 2023
    Author: Dan Leinir Turthra Jensen <admin@leinir.dk>

  ==============================================================================
*/

#include "JackThreadAffinitySetter.h"

#define DEBUG_JACK_THREAD_AFFINITY_SETTER false

void zl_set_jack_client_affinity(jack_client_t *client) {
    const jack_native_thread_t threadID = jack_client_thread_id(client);
    zl_set_dsp_thread_affinity(threadID);
}

void zl_set_dsp_thread_affinity(const pthread_t& threadID)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    // Our CPU set for the DSP is 0 (where the kernel also lives), 1, and 2. This leaves 4 for the UI application
    CPU_SET(0, &cpuset);
    CPU_SET(2, &cpuset);
    CPU_SET(3, &cpuset);
    int result = pthread_setaffinity_np(threadID, sizeof(cpuset), &cpuset);
    if (result != 0) {
        errno = result;
        perror("pthread_getaffinity_np");
#if DEBUG_JACK_THREAD_AFFINITY_SETTER
    } else {
        QStringList accepted;
        for (size_t j = 0; j < CPU_SETSIZE; j++)
            if (CPU_ISSET(j, &cpuset))
                accepted << QString("CPU %1").arg(QString::number(j));
        qDebug() << "Set returned by pthread_getaffinity_np() contained:" << accepted.join(", ");
#endif
    }
}
