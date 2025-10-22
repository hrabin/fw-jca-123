/*
 * Author: jarda https://github.com/hrabin
 */

#include "common.h"
#include "cfg.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#define BAUDRATE B115200
#define TIMEOUT 1  // Timeout in seconds

static int port_fd;

void setup_serial(int fd) 
{
    struct termios tty;

    // Get current terminal attributes
    if (tcgetattr(fd, &tty) != 0) 
    {
        OS_FATAL("tcgetattr");
    }

    // Set baud rates
    cfsetospeed(&tty, BAUDRATE);
    cfsetispeed(&tty, BAUDRATE);

    // 8 data bits, no parity, 1 stop bit
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Set local mode and non-canonical mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Set raw input
    tty.c_iflag &= ~(INLCR | ICRNL | IGNCR | BRKINT | INPCK | ISTRIP);

    // Set the new attributes
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        OS_FATAL("tcsetattr");
    }
}

void rx_flush(int fd)
{
    tcflush(port_fd, TCIOFLUSH); // rx/tx fifo discard
}

void send_command(int fd, const char *command) 
{
    write(fd, command, strlen(command));
}

void read_response(int fd) 
{
    char buffer[256];
    int n = read(fd, buffer, sizeof(buffer) - 1);

    if (n < 0) {
        OS_FATAL("port read failed");
    }

    buffer[n] = '\0'; // Null-terminate the buffer
    
    if (strncmp(buffer, cfg.response, strlen(cfg.response)) != 0)
    {
        OS_PRINTF("Response: %s" NL, buffer);
        OS_FATAL("Expected: %s" NL, cfg.response);
    }
}

void usage (ascii *filename)
{
    ascii *p = strrchr(filename, '/');
    
    if (p == NULL)
    p = filename;

    if (p[0] == '/')
        p++;

    OS_PRINTF("\nusage: %s [options]\n", p);
    
    cfg_print_help();

    OS_PRINTF("\n");
}

#define MAX_CMD_LEN 1024

static void _rx_feed(char ch)
{
    static int n=0;
    static char command[MAX_CMD_LEN];
    static u32 rx_len = 0;

    if (rx_len >= (sizeof(command) - 1))
        return;

    if ((ch == '\r') || (ch == '\n'))
    {
        if (rx_len == 0)
            return;

        ch = '\n';
    }

    command[rx_len++] = ch;

    if (ch == '\n')
    {
        command[rx_len++] = '\0';

        rx_flush(port_fd);
        send_command(port_fd, command);
        read_response(port_fd);
        OS_PRINTF("line %d OK" NL, ++n);

        rx_len = 0;
    }
}

int main(int argc, char *argv[])
{
    int ch;

    if (! configure (argc, argv))
    {
        usage(argv[0]);
        exit (EXIT_FAILURE);
    }

    // Open the serial port
    port_fd = open(cfg.device, O_RDWR | O_NOCTTY | O_SYNC);

    if (port_fd < 0) 
    {
        OS_FATAL("serial device open failed");
    }
    setup_serial(port_fd);
    rx_flush(port_fd);

    while ((ch = getchar()) != EOF)
    {   // cteme stdin
        _rx_feed(ch);
    }

    // Close the serial port
    close(port_fd);

    return 0;
}
