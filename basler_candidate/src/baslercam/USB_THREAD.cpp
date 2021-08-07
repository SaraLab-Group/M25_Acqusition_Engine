/* This is where I define my function pointer for the USB thread */

#include "USB_THREAD.h"


usb_dev_handle* open_dev(void)
{
    struct usb_bus* bus;
    struct usb_device* dev;

    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if (dev->descriptor.idVendor == MY_VID
                && dev->descriptor.idProduct == MY_PID)
            {
                return usb_open(dev);
            }
        }
    }
    return NULL;
}



void* USB_THREAD(void* data)
{
    usb_dev_handle* dev = NULL; /* the device handle */
    char tmp[BUF_SIZE];
    int ret;
    void* async_read_context = NULL;
    void* async_write_context = NULL;

    // The Magic of Pointers
    USB_THD_DATA* thd_data = (USB_THD_DATA*)data;
    usb_data incoming, outgoing;

    usb_init(); /* initialize the library */
    usb_find_busses(); /* find all busses */
    usb_find_devices(); /* find all connected devices */


    if (!(dev = open_dev()))
    {
        printf("error opening device: \n%s\n", usb_strerror());
        return 0;
    }
    else
    {
        printf("success: device %04X:%04X opened\n", MY_VID, MY_PID);
    }

#ifdef TEST_SET_CONFIGURATION
    if (usb_set_configuration(dev, MY_CONFIG) < 0)
    {
        printf("error setting config #%d: %s\n", MY_CONFIG, usb_strerror());
        usb_close(dev);
        return 0;
    }
    else
    {
        printf("success: set configuration #%d\n", MY_CONFIG);
    }
#endif

#ifdef TEST_CLAIM_INTERFACE
    if (usb_claim_interface(dev, 0) < 0)
    {
        printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
        usb_close(dev);
        return 0;
    }
    else
    {
        printf("success: claim_interface #%d\n", MY_INTF);
    }
#endif

    //#ifdef TEST_BULK_WRITE

    //#ifdef BENCHMARK_DEVICE
    //    ret = usb_control_msg(dev, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
    //        14, /* set/get test */
    //        2,  /* test type    */
    //        MY_INTF,  /* interface id */
    //        tmp, 1, 1000);
    //#endif

    //#ifdef TEST_ASYNC
    //    // Running an async write test
    //    ret = transfer_bulk_async(dev, EP_OUT, tmp, sizeof(tmp), 5000);
    //#else

    /* This is just a hard coded write test for sending frame rate change instruction to PSOC */
    outgoing.flags = 0;
    //outgoing.flags |= CHANGE_FPS;
    //outgoing.fps = thd_data->fps;
    uint8_t send_data = 1;
    uint8_t running = 1;


    /*****************************************************************************************/
    while (running) {
        // Signaling
        //printf("top of the mornin to yah\n");
        std::unique_lock flg(*thd_data->crit);
        if (thd_data->incoming_data->flags & CHANGE_CONFIG && !(thd_data->outgoing_data->flags & ACK_CMD)) {          
            outgoing.fps = thd_data->incoming_data->fps;
            outgoing.flags |= CHANGE_CONFIG;           
            thd_data->outgoing_data->flags |= ACK_CMD;            
            send_data = 1;
        }

        if (thd_data->incoming_data->flags & START_COUNT) {
            outgoing.flags |= START_COUNT;
            send_data = 1;
        }
        
        if (thd_data->incoming_data->flags & STOP_COUNT) {
            outgoing.flags |= STOP_COUNT;

            send_data = 1;
        }
        flg.unlock();

        if (thd_data->incoming_data->flags & EXIT_THREAD) {
            running = 0;
        }

        if (send_data) {
            ret = usb_bulk_write(dev, EP_OUT, (char*)&outgoing, sizeof(usb_data), 5000);
            //#endif
            if (ret < 0)
            {
                printf("error writing:\n%s\n", usb_strerror());
            }
            else
            {
                printf("success: bulk write %d bytes\n", ret);
            }
            outgoing.flags = 0; //~(CHANGE_FPS);
            //flg.lock();
            //thd_data->incoming_data->flags &= ~CHANGE_CONFIG;
            //flg.unlock();
            send_data = 0;
        }
        //#endif

        //#ifdef TEST_BULK_READ

        //#ifdef BENCHMARK_DEVICE
        //        ret = usb_control_msg(dev, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
        //            14, /* set/get test */
        //            1,  /* test type    */
        //            MY_INTF,  /* interface id */
        //            tmp, 1, 1000);
        //#endif

        //#ifdef TEST_ASYNC
        //        // Running an async read test
        //        ret = transfer_bulk_async(dev, EP_IN, tmp, sizeof(tmp), 5000);
        //#else
                // Running a sync read test
        //printf("before read\n");
        ret = usb_bulk_read(dev, EP_IN | 0x80, (char*)&incoming, sizeof(usb_data), 0);
        //#endif
        if (ret < 0)
        {
            printf("error reading:\n%s\n", usb_strerror());
        }
        else if (ret > 0)
        {
            std::unique_lock<std::mutex> crit(*thd_data->crit);
            thd_data->outgoing_data->flags |= USB_HERE;
            crit.unlock();

            if (incoming.flags & START_COUNT) {
                //printf("flags: %u\n", incoming.flags);
                printf("fps: %u\n", incoming.fps);
                //printf("time_waiting: %u\n", incoming.time_waiting);
                printf("counter: %zu, period: %zu\n", incoming.count, incoming.time_waiting);
            }
            //printf("success: bulk read %d bytes\n", ret);
            //printf("flags: %u\n", incoming.flags);
            //printf("fps: %u\n", incoming.fps);
            //printf("time_waiting: %u\n", incoming.time_waiting);
            //printf("counter: %zu, period: %zu\n", incoming.count, incoming.time_waiting);
        }
        //printf("After_After\n");
        //#endif
    }
    printf("Exiting USB LOOP.");
#ifdef TEST_CLAIM_INTERFACE
    usb_release_interface(dev, 0);
#endif

    if (dev)
    {
        usb_close(dev);
    }
    printf("Done.\n");

    //return 0;
}