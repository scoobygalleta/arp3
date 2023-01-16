#include "./../include/processB_utilities.h"
#include <sys/shm.h>
#include <sys/mman.h>
#include <bmpfile.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#define SEM_PATH_WRITER "/sem_BITMAP_writer"
#define SEM_PATH_READER "/sem_BITMAP_reader"

#define DIAMETER 60
#define WIDTH 1600
#define HEIGHT 600
#define DEPTH 1

FILE *log_file;

struct coordinate {
    int x;
    int y;
};

// Used for keeping track of the whole path
struct coordinate visited_pos[10000];
struct coordinate last_center;
int current = 0;

// Shared memory matrix
static uint8_t (*map)[600];

// Data for shared memory
const char *shm_name = "/BITMAP";
int shm_fd;

sem_t *sem_id_writer;
sem_t *sem_id_reader; 

int SIZE = sizeof(uint8_t[WIDTH][HEIGHT]);

void print_visited() { // Print all the visited postions
    for (int i = 0; i < current ; i++) {
        mvaddch(ceil(visited_pos[i].y / 20), ceil(visited_pos[i].x / 20), '0');
    }
}

void find_circle() { // Find the current position of the center in the shared matrix

    // Get the current time to write on the logfile
    time_t clk = time(NULL);
    char * timestamp = ctime(&clk);
    timestamp[24] = ' ';

    int current_diameter = 0;
    int center_found = FALSE;

    // Find the circle
    for (int y = 0; y < HEIGHT && !center_found; y++) {
        for (int x = 0; x < WIDTH && !center_found; x++) {

            if(current_diameter == DIAMETER-1 && last_center.x != x && last_center.y != y) {
                // If we found the diameter and position changed
                // Write to the logfile
                fprintf(log_file,"%s- Moving circle to position [%i,%i]\n", timestamp, x, y); fflush(log_file);
                // Save current position
                visited_pos[current].x = x - DIAMETER/2; // Current x - circle radius gives the center position
                visited_pos[current].y = y; 
                last_center.x = x - DIAMETER/2;
                last_center.y = y;
                current++;
                // Exit the loop
                center_found = TRUE;

            }
            else if(map[x][y] == 0) { // Pixel corresponds to background
                current_diameter = 0;
            }
            else { // Pixel corresponds to circle
                current_diameter++;
            }        
        }
    }
}

void sig_handler(int signo){ // termination signals
   if(signo == SIGINT || signo == SIGTERM) {
    // Destroy bitmap
    //bmp_destroy(bmp);
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

int main(int argc, char const *argv[]) {

    // Bitmap in process B is NOT needed, we directly check the matrix
    // Data structure for storing the bitmap file
    // bmpfile_t *bmp;

    // Setup to receive SIGINT and SIGTERM
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    
    shm_fd = shm_open(shm_name, O_RDONLY, 0666);
    if (shm_fd == 1) {
        perror("shared memory segment failed\n");
        exit(1);
    }

    map = (uint8_t(*)[HEIGHT])mmap(0, SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (map == MAP_FAILED) {
        perror("map failed\n");
        exit(1);
    } 

    // Semaphores   
    sem_id_writer = sem_open(SEM_PATH_WRITER, 0);
    if(sem_id_writer== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }
    sem_id_reader = sem_open(SEM_PATH_READER, 0);
    if(sem_id_reader== (void*)-1){
        perror("sem_open failure");
        exit(1);
    }

    // Creating the logfile
    log_file = fopen("./log/processB.log", "w");
    if (log_file == NULL) { 
        perror("error on file opening");
        exit(1); 
    }

    last_center.x = 0;
    last_center.y = 0;

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // Infinite loop
    while (TRUE) {

        // Get input in non-blocking mode
        int cmd = getch();

        // Get the current time to write on the logfile
        time_t clk = time(NULL);
        char * timestamp = ctime(&clk);
        timestamp[24] = ' ';

        if(cmd == 'c') {
            memset(visited_pos, 0, 10000);
            current = 0;
            reset_console_ui();
            find_circle();
            print_visited();
            mvprintw(LINES - 1, 1, "Press 'c' to reset the path ");
            refresh();
        } 

        // If user resizes screen, re-draw UI...
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
                print_visited();
            }
        }
        else {

            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Waiting for READER semaphore\n", timestamp); fflush(log_file);
            sem_wait(sem_id_reader);
            fprintf(log_file,"%s- READER semaphore entered\n", timestamp); fflush(log_file);

            find_circle();

            // Write the semaphore status to the logfile
            fprintf(log_file,"%s- Leaving the WRITER semaphore\n", timestamp); fflush(log_file);
            sem_post(sem_id_writer);
            fprintf(log_file,"%s- WRITER semaphore unlocked\n", timestamp); fflush(log_file);


        }

        print_visited();
        mvprintw(LINES - 1, 1, "Press 'c' to reset the path ");
        refresh();
    }

    // Free resources before termination
    
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

    endwin();
    return 0;
}
