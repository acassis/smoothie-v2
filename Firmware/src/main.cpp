#include <nuttx/config.h>
#include <sys/boardctl.h>
#include <nuttx/usb/cdcacm.h>
#include <nuttx/init.h>
#include <nuttx/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <iostream>

static int setup_CDC()
{
    // create the CDC device
    struct boardioc_usbdev_ctrl_s ctrl;
    void *handle;
    int ret;

    /* Initialize the USB serial driver */

    ctrl.usbdev   = BOARDIOC_USBDEV_CDCACM;
    ctrl.action   = BOARDIOC_USBDEV_CONNECT;
    ctrl.instance = 0; // CONFIG_NSH_USBDEV_MINOR;
    ctrl.handle   = &handle;

    ret = boardctl(BOARDIOC_USBDEV_CONTROL, (uintptr_t)&ctrl);
    if(OK != ret) return -1;

    sleep(3);

    /* Try to open the console */
    int fd;
    do {
        fd = open("/dev/ttyACM0", O_RDWR);
        /* ENOTCONN means that the USB device is not yet connected. Anything
         * else is bad.
         */

        if(fd < 0) {
            if(errno == ENOTCONN) {
                sleep(2);

            } else {
                printf("ttyACM0 Got error: %d\n", errno);
                return -1;
            }
        }
    } while (fd < 0);

    // we think we had a connection but due to a bug in Nuttx we may or may not
    // but it doesn't really seem to matter

    return fd;
}

#include "GCode.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "OutputStream.h"
#include "CommandShell.h"

static GCodeProcessor gp;
static bool dispatch_line(int fd, char *line, int cnt)
{
    // see if a command
    if(islower(line[0]) || line[0] == '$') {
        // create an output stream that writes to the already open fd
        OutputStream os(fd);
        if(!THEDISPATCHER.dispatch(line, os)) {
            if(line[0] == '$') {
                os.printf("error:Invalid statement\n");
            } else {
                os.printf("error:Unsupported command - %s\n", line);
            }
        }

        return true;
    }

    // Handle Gcode
    GCodeProcessor::GCodes_t gcodes;

    // Parse gcode
    if(!gp.parse(line, gcodes)) {
        // line failed checksum, send resend request
        char buf[32];
        size_t n = snprintf(buf, 32, "rs N%d\n", gp.get_line_number() + 1);
        write(fd, buf, n);
        return true;

    } else if(gcodes.empty()) {
        // if gcodes is empty then was a M110, just send ok
        write(fd, "ok\n", 3);
        return true;
    }

    // create an output stream that writes to the already open fd
    OutputStream os(fd);
    // dispatch gcode to MotionControl and Planner
    for(auto& i : gcodes) {
        if(!THEDISPATCHER.dispatch(i, os)) {
            // no handler for this gcode, return ok - nohandler
            os.printf("ok - nohandler\n");
        }
    }

    return true;
}

#define USE_PTHREAD
static std::mutex m;
static std::condition_variable cv;
#ifdef USE_PTHREAD
static void *usb_comms(void *)
#else
static int usb_comms(int , char)
#endif
{
    printf("Comms thread running\n");

    int fd = setup_CDC();
    if(fd == -1) {
        printf("CDC setup failed\n");
        return 0;
    }

    {
        // Manual unlocking is done before notifying, to avoid waking up
        // the waiting thread only to block again (see notify_one for details)
        std::unique_lock<std::mutex> lk(m);
        lk.unlock();
        cv.notify_one();
    }

    // on first connect we send a welcome message
    static const char *welcome_message = "Welcome to Smoothie\nok\n";

    size_t n;
    char line[132];
    n = read(fd, line, 1);
    if(n == 1) {
        n = write(fd, welcome_message, strlen(welcome_message));
        if(n < 0) {
            printf("ttyACM0: Error writing welcome: %d\n", errno);
            close(fd);
            fd = -1;
            return 0;
        }

    } else {
        printf("ttyACM0: Error reading: %d\n", errno);
        fd = -1;
        return 0;
    }

    // now read lines and dispatch them
    size_t cnt = 0;
    for(;;) {
        n = read(fd, &line[cnt], 1);
        if(n == 1) {
            if(line[cnt] == '\n' || cnt >= sizeof(line) - 1) {
                line[cnt] = '\0'; // remove the \n  and nul terminate
                dispatch_line(fd, line, cnt - 1);
                cnt = 0;

            } else {
                ++cnt;
            }
        } else {
            printf("ttyACM0: read error: %d\n", errno);
            break;
        }
    }

    printf("Comms thread exiting\n");

    #ifdef USE_PTHREAD
    return (void *)1;
    #else
    return 1;
    #endif
}

extern "C" int smoothie_main(int argc, char *argv[])
{
    // do C++ initialization for static constructors first
    up_cxxinitialize();

    printf("Smoothie V2.0alpha starting up\n");

    // create the commandshell
    CommandShell shell;
    shell.initialize();

    // Launch the comms thread
    //std::thread usb_comms_thread(usb_comms);

    // sched_param sch_params;
    // sch_params.sched_priority = 10;
    // if(pthread_setschedparam(usb_comms_thread.native_handle(), SCHED_RR, &sch_params)) {
    //     printf("Failed to set Thread scheduling : %s\n", std::strerror(errno));
    // }

    #ifdef USE_PTHREAD
    pthread_t usb_comms_thread;
    void *result;
    pthread_attr_t attr;
    struct sched_param sparam;
    int prio_min;
    int prio_max;
    int prio_mid;
    int status;

    prio_min = sched_get_priority_min(SCHED_FIFO);
    prio_max = sched_get_priority_max(SCHED_FIFO);
    prio_mid = (prio_min + prio_max) / 2;

    status = pthread_attr_init(&attr);
    if (status != 0) {
        printf("main: pthread_attr_init failed, status=%d\n", status);
    }

    status = pthread_attr_setstacksize(&attr, 10000);
    if (status != 0) {
        printf("main: pthread_attr_setstacksize failed, status=%d\n", status);
    }

    sparam.sched_priority = (prio_min + prio_mid) / 2;
    status = pthread_attr_setschedparam(&attr, &sparam);
    if (status != OK) {
        printf("main: pthread_attr_setschedparam failed, status=%d\n", status);
    } else {
        printf("main: Set comms thread priority to %d\n", sparam.sched_priority);
    }

    status = pthread_create(&usb_comms_thread, &attr, usb_comms, NULL);
    if (status != 0) {
        printf("main: pthread_create failed, status=%d\n", status);
    }

    // wait for comms thread to start
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk);

    printf("USB Comms thread started\n");

    // Join the comms thread with the main thread
    //usb_comms_thread.join();
    pthread_join(usb_comms_thread, &result);

    #else
    //usb_comms(0);
    task_create("usb_comms_thread", SCHED_PRIORITY_DEFAULT,
                5000,
                (main_t)usb_comms,
                (FAR char * const *)NULL);

    #endif

    printf("Exiting main\n");

    return 1;
}

