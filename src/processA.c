#include "./../include/processA_utilities.h"
#include <sys/shm.h>
#include <sys/mman.h>
#include <bmpfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#define SEM_PATH_WRITER "/sem_BITMAP_writer"
#define SEM_PATH_READER "/sem_BITMAP_reader"

#define DIAMETER 60
#define WIDTH 1600
#define HEIGHT 600
#define DEPTH 1

#define MAX 256

FILE *log_file;

sem_t *sem_id_writer;
sem_t *sem_id_reader;

// Data structure for storing the bitmap file
bmpfile_t *bmp;

// Shared memory matrix
static uint8_t (*map)[600];

// Data for shared memory
const char *shm_name = "/BITMAP";
int shm_fd;

int SIZE = sizeof(uint8_t[WIDTH][HEIGHT]);

void erase_bitmap() { // Erase the whole bitmap (all black)

    rgb_pixel_t black = {0, 0, 0, 0};

    // We erase the existing bitmap
    for (int x = 0; x <= WIDTH; x++) {
        for (int y = 0; y <= HEIGHT; y++) {
            bmp_set_pixel(bmp, x, y, black);
        }
    }
}

void draw_cicle_bitmap() { // Draw a circle on the bitmap

    int radius = DIAMETER/2;
    rgb_pixel_t white = {255, 255, 255, 0};

    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            // If distance is smaller, point is within the circle
            if(sqrt(x*x + y*y) < radius) {                       
                bmp_set_pixel(bmp, circle.x*20 + x, circle.y*20 + y, white); 
            }
        }
    } 
}

void copy_bitmap_to_map() { // Copy values from the bitmap into the shared map

    // Copy values from the bitmap into the shared map
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            rgb_pixel_t *current_pixel = bmp_get_pixel(bmp, x, y);
            map[x][y] = current_pixel->red;
        }
    }
}

//Function for 2. Use in a pasive way:SERVER.
void connection2(int connfd) {

    char key2[MAX];
    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;
    int first_launch = TRUE;
    int n;
    init_console_ui();
    //cmd2 is needed to going back to the menu by pressing q in the keyboard
    int cmd2 = getch();
    
    // infinite loop for chat
    while (cmd2!='q') {

        cmd2 = getch();
        mvprintw(LINES - 1, 1, "Press q to go out...");

        // Get the current time to write on the logfile
        time_t clk = time(NULL);
        char * timestamp = ctime(&clk);
        timestamp[24] = ' ';
        
        // read the message from client and copy it in buffer
        if ((n = read(connfd, key2, sizeof(key2))) < 0) {
            perror("Error reading from socket");
        }
        fflush(stdout);

        //Convert to integer
        int cmd=atoi(key2);

        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
        }
        // Else, if user presses print button...
        else if(cmd == KEY_MOUSE) {
            mvprintw(LINES - 1, 1, "Print button pressed");
            refresh();
            sleep(1);
                        
        }
        
        else if(cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN || first_launch) {
            
            //Write to the logfile according to the direction of the circle
            if (cmd == KEY_LEFT) {
                fprintf(log_file,"%s- Moving circle with the keyboard to LEFT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_RIGHT) {
                fprintf(log_file,"%s- Moving circle with the keyboard to RIGHT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_UP) {
                fprintf(log_file,"%s- Moving circle with the keyboard to UP\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_DOWN) {
                fprintf(log_file,"%s- Moving circle with the keyboard to DOWN\n", timestamp); fflush(log_file);
            }
            
            move_circle(cmd);
            draw_circle();

            erase_bitmap();
            
            draw_cicle_bitmap();

            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Waiting for WRITER semaphore\n", timestamp); fflush(log_file);
            sem_wait(sem_id_writer);
            fprintf(log_file,"%s- WRITER semaphore entered\n", timestamp); fflush(log_file);
            
            copy_bitmap_to_map();
        
            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Leaving the READER semaphore\n", timestamp); fflush(log_file);
            sem_post(sem_id_reader);
            fprintf(log_file,"%s- READER semaphore unlocked\n", timestamp); fflush(log_file);
        }
    }
    reset_console_ui();

}
//Function for 3. Use in an active way:CLIENT.
void connection3(int sockfd) {
    
    char key[MAX];
    int n;
    init_console_ui();
    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;
    int first_launch = TRUE;
    int cmd = getch();

    while (cmd!='q') {

        mvprintw(LINES - 1, 1, "Press q to go out...");
        cmd = getch();
        // Get the current time to write on the logfile
        time_t clk = time(NULL);
        char * timestamp = ctime(&clk);
        timestamp[24] = ' ';
            
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
        }
        // Else, if user presses print button...
        else if(cmd == KEY_MOUSE) {
            if(getmouse(&event) == OK) {
                if(check_button_pressed(print_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Print button pressed");
                    refresh();
                    sleep(1);
                    sprintf(key, "%d", cmd);
                    if ((n = write(sockfd, key, sizeof(key))) < 0) {
                        perror("ERROR writing to socket");
                    sleep(1);
                    }
                }
            }
        }
        else if(cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN||first_launch) {
            
            if (first_launch) {
                first_launch = FALSE;
            }

            sprintf(key, "%d", cmd);

            if ((n = write(sockfd, key, sizeof(key))) < 0) {
                perror("ERROR writing to socket");
            }
            
            //Write to the logfile according to the direction of the circle
            if (cmd == KEY_LEFT) {
                fprintf(log_file,"%s- Moving circle with the keyboard to LEFT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_RIGHT) {
                fprintf(log_file,"%s- Moving circle with the keyboard to RIGHT\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_UP) {
                fprintf(log_file,"%s- Moving circle with the keyboard to UP\n", timestamp); fflush(log_file);
            }
            else if (cmd == KEY_DOWN) {
                fprintf(log_file,"%s- Moving circle with the keyboard to DOWN\n", timestamp); fflush(log_file);
            }
            
            move_circle(cmd);
            draw_circle();

            erase_bitmap();
            
            draw_cicle_bitmap();

            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Waiting for WRITER semaphore\n", timestamp); fflush(log_file);
            sem_wait(sem_id_writer);
            fprintf(log_file,"%s- WRITER semaphore entered\n", timestamp); fflush(log_file);
            
            copy_bitmap_to_map();
        
            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Leaving the READER semaphore\n", timestamp); fflush(log_file);
            sem_post(sem_id_reader);
            fprintf(log_file,"%s- READER semaphore unlocked\n", timestamp); fflush(log_file);
        }
    }
    reset_console_ui();
}

void sig_handler(int signo){ // termination signals
    if(signo == SIGINT || signo == SIGTERM) {
        // Destroy bitmap
        bmp_destroy(bmp);
        // Unmapping the memory segment
        munmap(map, SIZE);
        // Unlinking the shared memory
        shm_unlink(shm_name);
        close(shm_fd);
        // Cose and unlink semaphores
        sem_close(sem_id_reader);
        sem_close(sem_id_writer);
        sem_unlink(SEM_PATH_READER);
        sem_unlink(SEM_PATH_WRITER);
        // Close logfile
        fclose(log_file);
    }
}

int main(int argc, char *argv[]) {

    int general_first = TRUE;

    // Setup to receive SIGINT and SIGTERM
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // Creating the logfiles
    log_file = fopen("./log/processA.log", "w");
    if (log_file == NULL) { 
        perror("error on file opening");
        exit(1); 
    }

    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == 1) {
        printf("Shared memory segment failed\n"); fflush(stdout);
        exit(1);
    }

    sem_id_writer = sem_open(SEM_PATH_WRITER, O_CREAT, 0644, 1);
    if(sem_id_writer== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }
    sem_id_reader = sem_open(SEM_PATH_READER, O_CREAT, 0644, 1);
    if(sem_id_reader== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }

    sem_init(sem_id_writer, 1, 1);
    sem_init(sem_id_reader, 1, 0);



    while(TRUE) {

        //Instantiate local bitmap
        bmp = bmp_create(WIDTH, HEIGHT, DEPTH);

        shm_fd = shm_open(shm_name, O_RDWR, 0666);
        if (shm_fd == 1) {
            printf("Shared memory segment failed\n"); fflush(stdout);
            exit(1);
        }

        ftruncate(shm_fd, SIZE);
        map = (uint8_t(*)[HEIGHT])mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (map == MAP_FAILED) {
            printf("Map failed\n"); fflush(stdout);
            exit(1);
        }
 
        // Semaphores
        sem_id_writer = sem_open(SEM_PATH_WRITER, 0644, 1);
        if(sem_id_writer== (void*)-1) {
            perror("sem_open failure");
            exit(1);
        }
        sem_id_reader = sem_open(SEM_PATH_READER, 0644, 1);;
        if(sem_id_reader== (void*)-1) {
            perror("sem_open failure");
            exit(1);
        }

        // Get the current time to write on the logfile
        time_t clk = time(NULL);
        char * timestamp = ctime(&clk);
        timestamp[24] = ' ';


        // Utility variable to avoid trigger resize event on launch
        int first_resize = TRUE;
        int first_launch = TRUE;

        char input[80];
        //Var. for the socket:
        int sockfd, connfd, len;
        struct sockaddr_in serv_addr, cli_addr;
        char address[80];
        char port[80];

        printf("Please, select the execution modality:\n   1-NORMAL EXECUTION\n   2-SERVER. Receives input from other application.\n   3-CLIENT. Sends input to another application\nPress 4 to skip menu\n");
        fgets(input,80,stdin);

        if (input[0] == 49) {
            printf("\n\t\tEXECUTION IN NORMAL MODALITY");fflush(stdout);
            // Write option in logfile
            fprintf(log_file,"%s- EXECUTION IN NORMAL MODALITY\n", timestamp); fflush(log_file);
            sleep(3);
            // Initialize UI
            init_console_ui();

            // Get input in non-blocking mode
            int cmd = getch();

            // Infinite loop
            while (cmd!='q') {

                cmd = getch();
                mvprintw(LINES - 1, 1, "Press q to go out...");

                // If user resizes screen, re-draw UI...
                if(cmd == KEY_RESIZE) {
                    if(first_resize) {
                        first_resize = FALSE;
                    }
                    else {
                        reset_console_ui();
                    }
                }
                // Else, if user presses print button...
                else if(cmd == KEY_MOUSE) {
                    if(getmouse(&event) == OK) {
                        if(check_button_pressed(print_btn, &event)) {
                            mvprintw(LINES - 1, 1, "Print button pressed");
                            refresh();
                            sleep(1);

                            for(int j = 0; j < COLS - BTN_SIZE_X - 2; j++) {
                                mvaddch(LINES - 1, j, ' ');
                            }

                            // Save image as .bmp file
                            bmp_save(bmp, "./out/snapshot.bmp");
                            fprintf(log_file,"%s- Print button pressed\n", timestamp); fflush(log_file);
                            
                        }
                    }
                }
                // If input is an arrow key, move circle accordingly...
                else if(cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN || first_launch) {
                    
                    if (first_launch) {
                        first_launch = FALSE;
                    }

                    // Write to the logfile according to the direction of the circle
                    if (cmd == KEY_LEFT) {
                    fprintf(log_file,"%s- Moving circle with the keyboard to LEFT\n", timestamp); fflush(log_file);
                    }
                    else if (cmd == KEY_RIGHT) {
                    fprintf(log_file,"%s- Moving circle with the keyboard to RIGHT\n", timestamp); fflush(log_file);
                    }
                    else if (cmd == KEY_UP) {
                    fprintf(log_file,"%s- Moving circle with the keyboard to UP\n", timestamp); fflush(log_file);
                    }
                    else if (cmd == KEY_DOWN) {
                    fprintf(log_file,"%s- Moving circle with the keyboard to DOWN\n", timestamp); fflush(log_file);
                    }
                    
                    move_circle(cmd);
                    draw_circle();

                    erase_bitmap();
                    
                    draw_cicle_bitmap();

                    // Write the semaphore status to the logfile
                    fprintf(log_file,"%s- Waiting for WRITER semaphore\n", timestamp); fflush(log_file);
                    sem_wait(sem_id_writer);
                    fprintf(log_file,"%s- WRITER semaphore entered\n", timestamp); fflush(log_file);
                    
                    copy_bitmap_to_map();
                
                    // Write the semaphore status to the logfile
                    fprintf(log_file,"%s- Leaving the READER semaphore\n", timestamp); fflush(log_file);
                    sem_post(sem_id_reader);
                    fprintf(log_file,"%s- READER semaphore unlocked\n", timestamp); fflush(log_file);

                }

            }

            reset_console_ui();
            // Free resources before termination
            bmp_destroy(bmp);
            close(shm_fd);
            sem_close(sem_id_reader);
            sem_close(sem_id_writer);

            endwin();
            system("clear");
        }

        else if (input[0] == 50) { // SERVER
            printf("\n\n\t\tEXECUTION IN SERVER MODALITY"); fflush(stdout);
            // Write option in logfile
            fprintf(log_file,"%s- EXECUTION IN SERVER MODALITY\n", timestamp); fflush(log_file);

            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("\nSocket creation failed...\n");
                exit(1);
            }
            else
                printf("\nSocket successfully created..\n");fflush(stdout);
            
            sleep(1);

            bzero((char *)&serv_addr, sizeof(serv_addr));

            printf("Include the Client address\n");fflush(stdout);
            fgets(address,80,stdin);
            int addressint=atoi(address);

            printf("Include the Client Port\n");fflush(stdout);
            fgets(port,80,stdin);
            int portint=atoi(port);

            // assign IP, PORT
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(portint);
            serv_addr.sin_addr.s_addr = htonl(addressint); 

            // Binding newly created socket to given IP and verification
            if ((bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
                perror("Socket bind failed...\n");
                exit(1);
            }
            else
                printf("Socket successfully binded..\n");

            // Now server is ready to listen and verification
            if ((listen(sockfd, 5)) != 0) {
                perror("Listen failed...\n");
                exit(1);
            }
            else
                printf("Server listening..\n");

            len = sizeof(cli_addr);

            // Accept the data packet from client and verification
            connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &len);
            if (connfd < 0) {
                perror("Server accept failed...\n");
                exit(1);
            }
            else
                printf("Server accept the client...\n");

            // Function for send and receive messages between client and server
            connection2(connfd);

            // After chatting close the socket
            close(sockfd);

            // Free resources before termination
            bmp_destroy(bmp);
            close(shm_fd);
            sem_close(sem_id_reader);
            sem_close(sem_id_writer);

            endwin();
            system("clear");
    
        }
        
        else if (input[0] == 51) { // CLIENT
            char address_S[80];
            char port_S[80];
            printf("\n\n\t\tEXECUTION IN CLIENT MODALITY\n");fflush(stdout);
            sleep(1);

            // Write option in logfile
            fprintf(log_file,"%s- EXECUTION IN CLIENT MODALITY\n", timestamp); fflush(log_file);

            int sockfd, connfd;
            struct sockaddr_in serv_addr, cli_addr;

            // socket create and verification
            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("Socket creation failed...\n");
                exit(1);
            }
            else
                printf("Socket successfully created..\n");

            bzero(&serv_addr, sizeof(serv_addr));

            printf("Include the Server address\n");fflush(stdout);
            fgets(address_S,80,stdin);

            printf("Include the Server Port\n");fflush(stdout);
            fgets(port_S,80,stdin);
            int portint_S=atoi(port_S);

            // assign IP, PORT
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = inet_addr(address_S);
            serv_addr.sin_port = htons(portint_S);

            // connect the client socket to server socket
            if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) != 0) {
                perror("Connection with the server failed...\n");
                exit(1);
            }
            else
                printf("Connected to the server..\n");

            connection3(sockfd);

            // close the socket
            close(sockfd);

            // Free resources before termination
            bmp_destroy(bmp);
            close(shm_fd);
            sem_close(sem_id_reader);
            sem_close(sem_id_writer);

            endwin();
            system("clear");
        
        }

        else if (input[0]=52) {
            return 0;
        }

    }

    sem_unlink(SEM_PATH_READER);
    sem_unlink(SEM_PATH_WRITER);

}
