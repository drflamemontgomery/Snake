#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

typedef struct pos {
    int x;
    int y;
} pos_t;

bool colors = false;

void init_ncurses(void) {
    initscr();
    clear();
    keypad(stdscr, TRUE);
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    if(has_colors()) {
        start_color(); // Initialize terminal colors
        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_RED);
        init_pair(3, COLOR_BLACK, COLOR_GREEN);
        init_pair(4, COLOR_BLACK, COLOR_YELLOW);
        init_pair(5, COLOR_BLACK, COLOR_BLUE);
        init_pair(6, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(7, COLOR_BLACK, COLOR_CYAN);
    }
}

void end_ncurses(void) { // Give back cursor and visual feedback to the terminal
    curs_set(1);
    echo();
    endwin();
}

void gameLoop(void);
void gameSetup(void);
void scoreboard(void);

bool app_should_close = false;
int** board;
int* __board;
int delay = 50000;
int highscore = 0;
int color = 0;

int main(void) {

    // Open the highscore file to get current high score
    int fd = open("highscore.txt", O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "Highscore file unreadable\n");
    }
    else {
        char buffer[256] = {0x00};
        read(fd, buffer, 256);
        highscore = atoi(buffer);
        close(fd);
    }


    init_ncurses();
    while(!app_should_close) {
        gameSetup();
        gameLoop();
        scoreboard();
    }
    // Free up memory before exiting
    free(__board);
    free(board);
    end_ncurses();
    return 0;
}

int win_width = 0;
int win_height = 0;

pos_t apple;
struct {
    pos_t pos;
    pos_t tail;
    int dir;
    int cur_dir;
    int score;
} snake;

void create_border();
int prior_time = 0;

#define APPLE -1
void unpause_game() {
    erase();
    // Draw Border
    create_border();

    // Draw current apple
    if(has_colors()) {attron(COLOR_PAIR(1));}
    mvprintw(apple.y+1, apple.x*2+1, "{}");
    if(has_colors()) {attroff(COLOR_PAIR(1));}

    // Draw Snake Segments
    for(int y = 0; y < win_height; y++) {
        for(int x = 0; x < win_width; x++) {
            if(board[y][x] != 0 && board[y][x] != APPLE) {
                if(has_colors()) {attron(COLOR_PAIR(color+2));}
                mvprintw(y+1, x*2+1, "[]");
                if(has_colors()) {attroff(COLOR_PAIR(color+2));}
                color += 1;
                color %= 6;
            }
        }
    }

    //Draw Snake Head
    if(has_colors()) {attron(COLOR_PAIR(color+2));}
    mvprintw(snake.pos.y+1, snake.pos.x*2+1, "[]");
    if(has_colors()) {attroff(COLOR_PAIR(color+2));}
    refresh(); // Display buffer to screen
    prior_time = clock() + 1000000; // add one second delay to continuing
}

void pause_game() {
    erase();
    create_border();
    mvprintw(win_height/2, win_width - 5, "-- Paused --");
    refresh();
    while(getch() != 'p'); // Wait until 'p' pressed to resume
    unpause_game();
}


void scoreboard() {
    erase();
    if(snake.score > highscore) { // If we beat the previous highscore, notify the player
        mvprintw(3, 3, "NEW HIGHSCORE: %d", snake.score);
        highscore = snake.score;

        // Create highscore.txt if it doesn't exist with permission of read and write
        int fd = open("highscore.txt", O_CREAT|O_WRONLY, 0666);
        if(fd < 0) {
            fprintf(stderr, "Unable to create highscore.txt\n");
        }
        else {
            char buffer[64] = {0x00};
            // Write highscore to string buffer so it can be passed to highscore file
            sprintf(buffer, "%d", snake.score);
            write(fd, buffer, strlen(buffer));
            close(fd);
        }
    } else { // Else show comparing scores
        mvprintw(3, 3, "SCORE: %d", snake.score);
        mvprintw(4, 3, "HIGHSCORE: %d", highscore);
    }
    mvprintw(6, 3, "New Game (y/n)");
    refresh();
    while(true) {
        switch(getch()) {
            case 'y':
                return;
            case 'n':
                app_should_close = true;
                return;
        }
    }
}

// Defining Keys for movement
#define LEFT KEY_LEFT
#define RIGHT KEY_RIGHT
#define UP KEY_UP
#define DOWN KEY_DOWN

bool move_and_collide();

void get_direction();

// Grace press to improve gameplay
static int last_usable_press = 0;

void gameLoop() {
    prior_time = clock() - delay;

    while(true) {
        if(clock() - prior_time >= delay) {
            snake.cur_dir = snake.dir;
            prior_time = clock();
            if(move_and_collide()) { // If we collide we stop the game
                break;
            }
            refresh();
            get_direction(last_usable_press); // Check if last usable press can change our direction
        } // Get last usable direction to go
        get_direction(getch());
    }
}

void get_direction(int key) {


    // Prevents the player from going backwards into itself
    switch(key) {
        case 'p':
            pause_game();
            break;
        case LEFT:
            last_usable_press = LEFT;
            if(snake.cur_dir != RIGHT) {
                snake.dir = LEFT;
            }
            break;
        case RIGHT:
            last_usable_press = RIGHT;
            if(snake.cur_dir != LEFT) {
                snake.dir = RIGHT;
            }
            break;
        case UP:
            last_usable_press = UP;
            if(snake.cur_dir != DOWN) {
                snake.dir = UP;
            }
            break;
        case DOWN:
            last_usable_press = DOWN;
            if(snake.cur_dir != UP) {
                snake.dir = DOWN;
            }
            break;
    }
}

void create_apple();
void move_tail();


bool move_and_collide() {
    color += 1;
    color %= 6;

    // Set the board value as the direction the snake is heading so that the tail can follow the path
    board[snake.pos.y][snake.pos.x] = snake.dir;
    switch(snake.dir) {
        case LEFT:
            snake.pos.x -= 1;
            break;
        case RIGHT:
            snake.pos.x += 1;
            break;
        case DOWN:
            snake.pos.y += 1;
            break;
        case UP:
            snake.pos.y -= 1;
            break;
    }
    if(snake.pos.x >= win_width || snake.pos.x < 0) {
        return true;
    }
    if(snake.pos.y >= win_height || snake.pos.y < 0) {
        return true;
    }

    // Draw Snakes new head position
    if(has_colors()) {attron(COLOR_PAIR(color+2));}
    mvprintw(snake.pos.y+1, snake.pos.x*2+1, "[]");
    if(has_colors()) {attroff(COLOR_PAIR(color+2));}

    // Create New Apple if we ate the last Apple
    if(board[snake.pos.y][snake.pos.x] == APPLE) {
        create_apple();
        snake.score += 1;
        return false; // Return so that we don't erase the tail segment from view
    }
    else if(board[snake.pos.y][snake.pos.x] != 0) {
        return true;
    }

    // Erase Tail segment
    move_tail();
    return false;
}

void move_tail() {
    mvprintw(snake.tail.y+1, snake.tail.x*2+1, "  ");
    int dir = board[snake.tail.y][snake.tail.x];
    board[snake.tail.y][snake.tail.x] = 0;

    // Update Tail position based on the direction given from the board
    switch(dir) {
        case LEFT:
            snake.tail.x -= 1;
            break;
        case RIGHT:
            snake.tail.x += 1;
            break;
        case UP:
            snake.tail.y -= 1;
            break;
        case DOWN:
            snake.tail.y += 1;
            break;
    }
}

void create_snake();

void gameSetup() {
    // Get terminal size
    getmaxyx(stdscr, win_height, win_width);
    // Width is half the screen size so we have a square segment '[]'
    win_width = (int)(win_width/2)-1;
    win_height -= 2;

    // Allocate memory for the board
    __board = calloc(win_width*win_height, sizeof(int));
    board = malloc(win_height*sizeof(int*));
    for(int y = 0; y < win_height; y++) {
        board[y] = (__board + win_width*y);
    }

    // Seed random for the apples
    srand(time(NULL));


    // Create and Draw intial objects
    erase();
    create_border();
    create_snake();
    create_apple();
    refresh();
}

void create_border() {

    if(has_colors()) {attron(COLOR_PAIR(1));}
    for(int i = 0; i < win_width*2; i++) {
        mvprintw(0, i+1, "-");
        mvprintw(win_height+1, i+1, "-");
    }
    for(int i = 0; i < win_height; i++) {
        mvprintw(i+1, 0, "|");
        mvprintw(i+1, win_width*2+1, "|");
    }
    mvprintw(0, 0, "+");
    mvprintw(0, win_width*2+1, "+");
    mvprintw(win_height+1, 0, "+");
    mvprintw(win_height+1, win_width*2+1, "+");
    if(has_colors()) {attroff(COLOR_PAIR(1));}
}

void create_snake() {
    snake.score = 1;
    snake.pos.x = 3;
    snake.pos.y = 3;
    snake.dir = RIGHT;
    snake.cur_dir = RIGHT;

    snake.tail.x = snake.pos.x;
    snake.tail.y = snake.pos.y;
    last_usable_press = 0x00;

    board[3][3] = RIGHT;
    if(colors) {attron(COLOR_PAIR(color+2));}
    // 4 and 7 are the starting characters on the screen
    mvprintw(4, 7, "[]");
    if(colors) {attroff(COLOR_PAIR(color+2));}
}

void create_apple() {
    apple.x = rand() % win_width;
    apple.y = rand() % win_height;
    int direction = (rand() % 2) * 2 - 1; // Returns -1 or 1

    // Apple must find a position in (n) time
    while(board[apple.y][apple.x] != 0) {
        apple.x += direction;
        if(apple.x < 0) {
            apple.x = win_width-1;
            apple.y -= 1;
        } else if(apple.x >= win_width) {
            apple.x = 0;
            apple.y += 1;
        }
        if(apple.y < 0) {
            apple.y = win_height-1;
        } else if(apple.y >= win_height) {
            apple.y = 0;
        }
    }

    board[apple.y][apple.x] = APPLE;
    if(has_colors()) {attron(COLOR_PAIR(1));}
    mvprintw(apple.y+1, apple.x*2+1, "{}");
    if(has_colors()) {attroff(COLOR_PAIR(1));}
}
