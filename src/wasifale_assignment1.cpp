/**
 * @wasifale_assignment1
 * @author  Wasif Aleem <wasifale@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * REFERENCE
 * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/Server.h"
#include "../include/Client.h"

using namespace std;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv) {
    /*Init. Logger*/
    cse4589_init_log(argv[2]);

    /* Clear LOGFILE*/
    fclose(fopen(LOGFILE, "w"));

    /*Start Here*/
    if (argc != 3) {
        cout << "Usage: ./assignment1 c/s port" << endl;
        return 1;
    }
    switch (argv[1][0]) {
        case 's': {
            Server server(argv[2]);
            server.start();
            return 0;
        }
        case 'c': {
            Client client(argv[2]);
            client.start();
            return 0;
        }
        default: {
            cout << "Usage: ./assignment1 c/s port" << endl;
            return 1;
        }
    }
}
