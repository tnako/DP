#include "mdwrkapi.c"

int main (int argc, char *argv [])
{
    mdwrk_t *session = mdwrk_new("tcp://localhost:12345", "echo", 1);

    zmsg_t *reply = NULL;
    while (true) {
        zmsg_t *request = mdwrk_recv (session, &reply);
        if (request == NULL) {
            break;              //  Worker was interrupted
		}
		
        reply = request;        //  Echo is complex... :-)
    }
    mdwrk_destroy(&session);
    return 0;
}