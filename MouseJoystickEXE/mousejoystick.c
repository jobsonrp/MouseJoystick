/*
 * MouseJoystick
 */

#include <errno.h>
#include <fcntl.h> /* create, O_CREAT, O_CLOEXEC */
#include <poll.h> /* poll */
#include <stdbool.h> /* type bool */
#include <stdio.h> /* printf, puts, snprintf */
#include <stdlib.h> /* EXIT_FAILURE, EXIT_SUCCESS */
#include <string.h>
#include <unistd.h> /* read */
#include <linux/uhid.h>
#include <linux/input.h>

//***********FUNCTIONS JOYSTICK******************

#define JS_EVENT_BUTTON		0x01	/* button pressed/released */
#define JS_EVENT_AXIS		0x02	/* joystick moved */

struct js_event {
	__u32 time;		/* event timestamp in milliseconds */
	__s16 value;	/* value */
	__u8 type;		/* event type */
	__u8 number;	/* axis/button number */
};

// Variables used for Joystick control
static bool btn1_down;
static bool btn2_down;
static signed char abs_hor;
static signed char abs_ver;
static signed char wheel;

int js_read_event(int fd, struct js_event *event)
{
	ssize_t bytes;
    bytes = read(fd, event, sizeof(*event));
    if (bytes == sizeof(*event)) {
    	return 0;
      }
    // Error, could not read full event.
    return -1;
}

// Current state of an axis.
struct js_axis_state {
    short x, y;
};

// Returns the axis that the event indicated.
size_t js_get_axis_state(struct js_event *event, struct js_axis_state axes[3])
{
   size_t axis = event->number / 2;

   /*printf("event->number = %u\n", event->number);
   printf("axis = %u\n", axis); */

   if (axis < 3)
   {
       if (event->number % 2 == 0)
           axes[axis].x = event->value;
       else
           axes[axis].y = event->value;
   }
   return axis;
}

// ***********FUNCTIONS UHID******************

/* Some information about the device can be verified by reading the text file
 * in /sys/kernel/debug/hid/<dev>/rdesc
 */
static unsigned char rdesc[] = {
	0x05, 0x01,	/* USAGE_PAGE (Generic Desktop) */
	0x09, 0x02,	/* USAGE (Mouse) */
	0xa1, 0x01,	/* COLLECTION (Application) */
	0x09, 0x01,		/* USAGE (Pointer) */
	0xa1, 0x00,		/* COLLECTION (Physical) */
	0x85, 0x01,			/* REPORT_ID (1) */
	0x05, 0x09,			/* USAGE_PAGE (Button) */
	0x19, 0x01,			/* USAGE_MINIMUM (Button 1) */
	0x29, 0x03,			/* USAGE_MAXIMUM (Button 3) */
	0x15, 0x00,			/* LOGICAL_MINIMUM (0) */
	0x25, 0x01,			/* LOGICAL_MAXIMUM (1) */
	0x95, 0x03,			/* REPORT_COUNT (3) */
	0x75, 0x01,			/* REPORT_SIZE (1) */
	0x81, 0x02,			/* INPUT (Data,Var,Abs) */
	0x95, 0x01,			/* REPORT_COUNT (1) */
	0x75, 0x05,			/* REPORT_SIZE (5) */
	0x81, 0x01,			/* INPUT (Cnst,Var,Abs) */
	0x05, 0x01,			/* USAGE_PAGE (Generic Desktop) */
	0x09, 0x30,			/* USAGE (X) */
	0x09, 0x31,			/* USAGE (Y) */
	0x09, 0x38,			/* USAGE (WHEEL) */
	0x15, 0x81,			/* LOGICAL_MINIMUM (-127) */
	0x25, 0x7f,			/* LOGICAL_MAXIMUM (127) */
	0x75, 0x08,			/* REPORT_SIZE (8) */
	0x95, 0x03,			/* REPORT_COUNT (3) */
	0x81, 0x06,			/* INPUT (Data,Var,Rel) */
	0xc0,			/* END_COLLECTION */
	0xc0,		/* END_COLLECTION */
	0x05, 0x01,	/* USAGE_PAGE (Generic Desktop) */
	0x09, 0x06,	/* USAGE (Keyboard) */
	0xa1, 0x01,	/* COLLECTION (Application) */
	0x85, 0x02,		/* REPORT_ID (2) */
	0x05, 0x08,		/* USAGE_PAGE (Led) */
	0x19, 0x01,		/* USAGE_MINIMUM (1) */
	0x29, 0x03,		/* USAGE_MAXIMUM (3) */
	0x15, 0x00,		/* LOGICAL_MINIMUM (0) */
	0x25, 0x01,		/* LOGICAL_MAXIMUM (1) */
	0x95, 0x03,		/* REPORT_COUNT (3) */
	0x75, 0x01,		/* REPORT_SIZE (1) */
	0x91, 0x02,		/* Output (Data,Var,Abs) */
	0x95, 0x01,		/* REPORT_COUNT (1) */
	0x75, 0x05,		/* REPORT_SIZE (5) */
	0x91, 0x01,		/* Output (Cnst,Var,Abs) */
	0xc0,		/* END_COLLECTION */
};

static int uhid_write(int fd, const struct uhid_event *ev)
{
	ssize_t ret;
	ret = write(fd, ev, sizeof(*ev));
	if (ret < 0) {
		fprintf(stderr, "Cannot write to uhid: %m\n");
		return -errno;
	} else if (ret != sizeof(*ev)) {
		fprintf(stderr, "Wrong size written to uhid: %ld != %lu\n",
			ret, sizeof(ev));
		return -EFAULT;
	} else {
		return 0;
	}
}

static int uhid_create(int fd)
{
	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, "test-uhid-device");
	ev.u.create.rd_data = rdesc;
	ev.u.create.rd_size = sizeof(rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;
	return uhid_write(fd, &ev);
}

static void uhid_destroy(int fd)
{
	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;
	uhid_write(fd, &ev);
}

static int uhid_send_event(int fd)
{
	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = 5;
	ev.u.input.data[0] = 0x1;
	if (btn1_down)
		ev.u.input.data[1] |= 0x1;
	if (btn2_down)
		ev.u.input.data[1] |= 0x2;
	ev.u.input.data[2] = abs_hor;
	ev.u.input.data[3] = abs_ver;
	ev.u.input.data[4] = wheel;
	return uhid_write(fd, &ev);
}

// *********** MAIN ******************

int main()
{
	// ***********DEVICE JOYSTICK******************
    const char *device;
    int js, closeJoy;
    struct js_event event;
    struct js_axis_state axes[3] = {0};
    size_t axis;

    device = "/dev/input/js0";

    js = open(device, O_RDONLY);
    //printf("Js = %u \n", js);
    fprintf(stderr, "\nUSB Joystick %s OPEN.\n", device);

    if (js == -1)
        perror("Device USB Joystick could not be opened.");

    fprintf(stderr, "Device USB Joystick created.\n");

	// ***********DEVICE UHID******************
	int fd, ret;
	const char *path = "/dev/uhid";

	//Abrir o UHID-CDEV
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open uhid-cdev %s: %m\n", path);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "\nUHID-CDEV %s OPEN.\n", path);

	//**********Criação do UHID Device**********
	ret = uhid_create(fd);
	if (ret) {
		close(fd);
		return EXIT_FAILURE;
	}
	fprintf(stderr, "Device UHID criated.\n");

	//**********USER PROGRAM***********
	fprintf(stderr, "\nPress the SELECT button to EXIT.\n");
	closeJoy = 0;

    while (js_read_event(js, &event) == 0 && closeJoy == 0)
    {
        switch (event.type)
        {
            case JS_EVENT_BUTTON:
                //printf("Button %u %s\n", event.number, event.value ? "On" : "Off");
                if (event.number == 0){ // TRIANGLE - PAGE UP
                	fprintf(stderr, "PAGE UP %s |", event.value ? "On" : "Off");
        			wheel = 1;
        			uhid_send_event(fd);
        			wheel = 0;
                }
                else if (event.number == 1){ // BALL - BUTTON MOUSE RIGHT
                	fprintf(stderr, "MOUSE RIGHT %s |", event.value ? "On" : "Off");
                	btn2_down = !btn2_down;
        			uhid_send_event(fd);
                }
                else if (event.number == 2){ // X (CROSS) - PAGE DOWN
                	fprintf(stderr, "PAGE DOWN %s |", event.value ? "On" : "Off");
        			wheel = -1;
        			uhid_send_event(fd);
        			wheel = 0;
                }
                else if (event.number == 3){ // SQUARE - BUTTON MOUSE LEFT
                	fprintf(stderr, "MOUSE LEFT %s |", event.value ? "On" : "Off");
        			btn1_down = !btn1_down;
        			uhid_send_event(fd);
                }
                else if (event.number == 8){ // Button SELECT - EXIT
                	fprintf(stderr, "SELECT %s ->\nPROGRAM FINISHED\n", event.value ? "On" : "Off");
                	closeJoy = 1;
                	break;
                }
                break;
            case JS_EVENT_AXIS:
                axis = js_get_axis_state(&event, axes);
                /*if (axis < 3)
                    printf("Axis %u at (%6d, %6d)\n", axis, axes[axis].x, axes[axis].y); */
                if (axes[axis].y < 0){
                	fprintf(stderr, "UP |");
        			abs_ver = -20;
        			uhid_send_event(fd);
        			abs_ver = 0;
                }
                else if (axes[axis].x < 0){
                	fprintf(stderr, "LEFT |");
        			abs_hor = -20;
        			ret = uhid_send_event(fd);
        			abs_hor = 0;
                }
                else if (axes[axis].y > 0){
                	fprintf(stderr, "DOWN |");
        			abs_ver = 20;
        			ret = uhid_send_event(fd);
        			abs_ver = 0;
                }
                else if (axes[axis].x > 0){
                	fprintf(stderr, "RIGHT |");
        			abs_hor = 20;
        			ret = uhid_send_event(fd);
        			abs_hor = 0;
                }
                break;
            default:
            	break;
        }
    }
    fprintf(stderr, "\nClose USB Joystick device\n");
    close(js);

	fprintf(stderr, "Destroy uhid device\n\n");
	uhid_destroy(fd);
	return EXIT_SUCCESS;
}

