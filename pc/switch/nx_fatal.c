/* host_fatal on Switch: there are no message boxes, so the system's error dialog (the error
   applet) shows the message until the player closes it. Its own file: <switch.h> clashes with
   the PS2 shim's names and SDL's. */
#include <switch.h>
#include <stdio.h>

void sdl3c_fatal_screen(const char* msg)
{
    ErrorApplicationConfig c;
    char text[1800];
    snprintf(text, sizeof(text), "Code: Veronica X: %s\n\nLog: sdmc:/switch/recvx/recvx.log", msg);
    if (R_SUCCEEDED(errorApplicationCreate(&c, text, NULL))) errorApplicationShow(&c);
}
