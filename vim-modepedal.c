#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <hidapi.h>
#include <fcntl.h>
#include <linux/input.h>
#include "footswitch/common.h"
#include "footswitch/debug.h"
#include "footswitch/footswitch.c"

#define EV_PRESSED 1
#define EV_RELEASED 0
#define EV_REPEAT 2

static const char *const evval[3] = {
    "RELEASED",
    "PRESSED ",
    "REPEATED"
};

// pedalbytes needs to be array[40]. returns length of pedalstring
int print2_string(unsigned char data[]) {
    int r = 0, ind = 2;
    int len = data[0] - 2;

    while (len > 0) {
        if (ind == 8) {
            r = hid_read(dev, data, 8);
            if (r < 0) {
                fatal("error reading data (%ls)", hid_error(dev));
            }
            if (r != 8) {
                fatal("expected 8 bytes, received: %d", r);
            }
            ind = 0;
        }
        len--;
        ind++;
    }
    return ind-2;
}

void init_daemon(int* pedalbytelength) {
    int r = 0, i = 0;
    unsigned char query[8] = {0x01, 0x82, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char response[8];

	init();
    for (i = 0 ; i < 3 ; i++) {
        query[3] = i + 1;
        usb_write(query);
        r = hid_read(dev, response, 8);
        if (r < 0) {
            fatal("error reading data (%ls)", hid_error(dev));
        }
        printf("[switch %d]: ", i + 1);
        switch (response[1]) {
            case 0:
                printf("unconfigured\n");
                break;
			case 4:
                *pedalbytelength = print2_string(response);
				break;
            case 1:
            case 0x81:
            case 2:
            case 3:
				fprintf(stderr, "Only strings can be used in daemon mode.\n");
				break;
            default:
                fprintf(stderr, "Unknown response:\n");
                debug_arr(response, 8);
                deinit();
                return;
        }
	}
	deinit();
}

int read_pedalevent(int fd_in, struct input_event* inevent, size_t eventsize ) {
	ssize_t n = read(fd_in, *&inevent, eventsize);
	if (n == (ssize_t)-1) {
		fprintf(stderr, "Error code %i while reading pedal event.\n", errno);
		return errno;
	}
	if (n != eventsize) {
		fatal("Input/Output error");
		return EIO; /* i/o error */
	}
	return 0;
}

int is_delim_event(struct input_event* event) {
	if (event->type  == EV_MSC &&
		event->code  == MSC_SCAN &&
		event->value == 0x700e1) 
		 return 1;
	return 0;
}

int daemon_run(int fd_in, int fd_out, int pedalkeycount) {
	if (pedalkeycount < 1) return -1;

    struct input_event inevent;
	struct input_event sentence[80] = { 0 };
	int current_word				= 0;

	/* every pedalpress sends one or more of
	 * - a EV_MSC message (the scancode)
	 * - a EV_KEY (or the like)
	 * e.g. "i" is 1 EV_MSC and 1 EV_KEY while
	 * 0xff is EV_MSC, EV_KEY, EV_MSC, EV_KEY.
	 * An EV_SYN is the end of every pedalpress
	 * event series.
	 * We read all events, split by EV_SYNs.
	 * Then we print out the nth "word" and set the
	 * counter one up. */
    while (1) {	
		/* read sentence. */
		int sentencelength 	= 0;
		int eventcount 		= 0; /* how many EV_SYNs do we have in the sentence*/
		int wordcount 		= 0; /* how many delimiter do we have in the sentence */
		
		for (sentencelength = 0; eventcount < pedalkeycount*2; sentencelength++) { /* every key press has a pressed and released event -> pedalkeycount*2 */
			int ret = read_pedalevent(fd_in, &inevent, sizeof(inevent));
			if (ret != 0) continue;
			sentence[sentencelength] = inevent;
			if 		(inevent.type == EV_SYN) 		eventcount++;
			else if (is_delim_event(&inevent) == 1) wordcount++;
		}
		
		if (wordcount == 0) wordcount++;
		
		/* Move through the sentence and print the "current_word"s word. */
		int x = 0;
		for (int i = 0; i < sentencelength; i++) {
			if (x == current_word) {
				while (i < sentencelength && is_delim_event(&sentence[i]) == 0) {
					/*fprintf(stderr, "Print event %i with code %i\n", i, sentence[i].code);*/
						int status = write(fd_out, &sentence[i], sizeof(sentence[i]));
						if (status < 0)
							fprintf(stderr, "Could not simulate key press: %s\n", strerror(status));
					i++;
				}
				break;
			}

			if (wordcount > 1 && is_delim_event(&sentence[i]) == 1) {
				x++;
				i += 9; /* skip all events of the delimiter press */
			}
			else if (wordcount == 1) { /* only 1 word, no delimiters */
				x++;
			}
		} 
		
		current_word = (current_word +1) % wordcount;
	}
    fflush(NULL);
    fprintf(stderr, "[ERR] %s.\n", strerror(errno));
    return EXIT_FAILURE;
}

int getInputDeviceName(char *indev) {
    int rd;
    const char* pdevsName = "/proc/bus/input/devices";

    int devsFile = open(pdevsName, O_RDONLY);
    if (devsFile == -1) {
        printf("[ERR] Open input devices file: '%s' FAILED\n", pdevsName);
        return 1;
    }

	char devs[4096];

	if ((rd = read(devsFile, devs, sizeof(devs) - 1)) < 6) {
		printf("[ERR] Wrong size was read from devs file\n");
		return 1;
	}
	else {
		devs[rd] = 0;

		char *pVendor, *pEV = devs;

		/* The USB pedal comes with three different Vendor/Product IDs */
		pVendor = strstr(pEV, "Vendor=0c45 Product=7404");
		if (pVendor == NULL)
			pVendor = strstr(pEV, "Vendor=0c45 Product=7403");
		if (pVendor == NULL)
			pVendor = strstr(pEV, "Vendor=413d Product=2107");
		pEV = strstr(pVendor, "event");

		if (pVendor && pEV) {
			if (strlen(pEV) >= 6) {
				indev = strcat(indev, "/dev/input/event");
				char *number = strtok(pEV, " ");
				number = strncpy(number, number + 5, 3); 
				indev = strcat(indev, &number[5]);
				return 0;
			}
			else {
				printf("[ERR] Could not determine USB pedal event device\n");
				return 1;
			}
		}
		else {
			printf("[ERR] USB pedal event device not found\n");
			return 1;
		}
	}
	return 1;
}

int getOutputDeviceName(char *outdev) {
    int rd;
    const char* pdevsName = "/proc/bus/input/devices";

    int devsFile = open(pdevsName, O_RDONLY);
    if (devsFile == -1) {
        printf("[ERR] Open input devices file: '%s' FAILED\n", pdevsName);
        return 1;
    }

	char devs[4096];

	if ((rd = read(devsFile, devs, sizeof(devs) - 1)) < 6) {
		printf("[ERR] Wrong size was read from devs file\n");
		return 1;
	}
	else {
		devs[rd] = 0;

		char *pHandlers, *pEV = devs;
		do {
			pHandlers = strstr(pEV, "Handlers=");
			pEV = strstr(pHandlers, "EV=");
		}
		while (pHandlers && pEV && 0 != strncmp(pEV + 3, "120013", 6));

		if (pHandlers && pEV) {
			char* pevent = strstr(pHandlers, "event");
			if (pevent && strlen(pevent) >= 6) {
				outdev = strcat(outdev, "/dev/input/event");
				char *number = strtok(pevent, " ");
				outdev = strcat(outdev, &number[5]);
				return 0;
			}
			else {
				printf("[ERR] Abnormal keyboard event device\n");
				return 1;
			}
		}
		else {
			printf("[ERR] Keyboard event device not found\n");
			return 1;
		}
	}
	return 1;
}

int daemon_mode(char* input_device, char* output_device) {
    int pedalbytelength = -1;

	init_daemon(&pedalbytelength);
	
	char indev[2048] = {0};
	char outdev[2048] = {0};
	
	if (!input_device) {
		fprintf(stderr, "No USB pedal device file specified. Trying to find it…\n");
		if (getInputDeviceName(indev) != 0) {
			fprintf(stderr, "Could not find USB pedal device.");
			return EXIT_FAILURE;
		}
		input_device = indev;
		fprintf(stderr, "Found USB pedal device file at %s\n", input_device);
	}
	
	if (!output_device) {
		fprintf(stderr, "No output keyboard device file specified. Trying to find it…\n");
		if (getOutputDeviceName(outdev) != 0) {
			fprintf(stderr, "Could not find keyboard device to send simulated keypresses to.");
			return EXIT_FAILURE;
		}
		output_device = outdev;
		fprintf(stderr, "Found output keyboard device file at %s\n", output_device);
	}
	
    int fd_in = open(input_device, O_RDONLY);
    if (fd_in == -1) {
        fprintf(stderr, "Cannot open %s: %s.\n", input_device, strerror(errno));
        return EXIT_FAILURE;
    }
    
    int fd_out = open(output_device, O_WRONLY);
    if (fd_out == -1) {
        fprintf(stderr, "Cannot open %s: %s.\n", output_device, strerror(errno));
        return EXIT_FAILURE;
    }
    
    int status;
	if (ioctl(fd_in, EVIOCGRAB, &status) == -1)
		fprintf(stderr, "Could not claim the device exclusively: %s\n",
			strerror(errno));
	
	return daemon_run(fd_in, fd_out, pedalbytelength);
}

int main(int argc, char *argv[]) {
    int opt;

    init_pedals();
    char *input_device = NULL, *output_device = NULL;
    while ((opt = getopt(argc, argv, "i:o:h")) != -1) {
        switch (opt) {
			case 'i': 
				input_device = optarg; 
				break;
			case 'o':
				output_device = optarg; 
				break;
			case 'h':
                usage();
				break;
        }
    }
	return daemon_mode(input_device, output_device);
}

