#include <stdio.h>
#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include<pthread.h>
#include<time.h>
#include<stdlib.h>
#include "firebase.c"

#define BASE_URL_FORMAT "https://quizzy-4d728.firebaseio.com/games"
#define GAME_SESSION_URL_FORMAT "https://quizzy-4d728.firebaseio.com/games/%d.json"
/*
    Internally the person who initialtes the match is called the "HOST" and the other person
    who joins the game is the "GUEST"
*/

#define GAMER_ROLE_HOST 1
#define GAMER_ROLE_GUEST 2

#define GAME_MODE_SINGLEPLAYER 1
#define GAME_MODE_MULTIPLAYER 2

#define TIME_PER_QUESTION 22

static void check_timer();
enum Game_State {NOT_IN_MATCH = 0, WAITING_FOR_OPPONENT_TO_JOIN, OPPONENT_JOINED, QUIZ_STARTED, PAUSED_BETWEEN_QUESTION, IN_QUESTION};

int time_left = TIME_PER_QUESTION;
int is_timeup = 0;
struct question
{
    char *question;
    char choices[4][100];
    char *correct_answer;
    char *category;
};


struct multiplayer_data
{
    int session;
    
}multiplayer_data;

struct player
{
    char player_role_text[25]; 
    int score;
    int current_question_number;
}opponent;

struct game_data
{
    int is_game_in_progress;
    int progress; //No of questions viewed
    int game_mode; //
    int player_role; // Host or guest player.
    char player_role_text[25];
    int score;
    int id;
    enum Game_State state;
}current_game_info = {0, 0, -1, -1,"\0", 0, 110, 0};


void start_multiplayer_as_guest(int);
/*
 log_messages to console or display to user
 severity: 1 - warning
           2 - error
           3 - critical(causes exit of program)
           0 or NULL - info
*/
void log_message(char *msg, int severity)
{
    printf("%s", msg);

}

void shuffle(char array[][100], size_t n)
{
  if (n > 1) {
    size_t i;
    for (i = 0; i < n - 1; i++) {
      size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
      char t[100];

      strcpy(t, array[j]);
      strcpy(array[j], array[i]);
      strcpy(array[i], t);
    }
  }
}

void load_questions_into_file()
{
    FILE *fp  = fopen("question.json", "w+");
    int status;
    char url[53] = "https://opentdb.com/api.php?amount=10&type=multiple";
    status  = request_and_save_to_file(url, fp);
    fclose(fp);
}

json_t * load_questions_array()
{
    // The root node of the questions data returned by OpenTriviaDb
    json_t *root, *response_code, *questions;
    json_error_t *error;
    load_questions_into_file();
    root = json_load_file("question.json", 0, error);
    
    if (root == NULL) {
        log_message("\n\nAn error occured while loading questions :( \n\n", 0);
    }
    
    if(!json_is_object(root))
    {
        log_message("Invalid format of questions\n\n", 0);
    }

    response_code = json_object_get(root, "response_code");
    if(json_is_integer(response_code) && json_integer_value(response_code) != 0)
    {
        log_message("There was a problem while fetching the questions", 3);
    }

    questions = json_object_get(root, "results");
    if(!json_is_array(questions))
    {
        log_message("There was a problem while fetching the questions", 3);
    }

    return questions;

}


void parse_question(json_t *question_json, struct question *question)
{
    json_t  *choices_array, *answer;
    char shuffled_choices[4][100] = {"","","",""};

    //printf("%d", question);
    question->question = (char *)json_string_value(json_object_get(question_json, "question"));
    question->category = (char *)json_string_value(json_object_get(question_json, "category"));

    choices_array = json_object_get(question_json, "incorrect_answers");
    
    if (choices_array == NULL) {
        printf("Error with data :( \n\n");
    }

    answer = json_object_get(question_json, "correct_answer");
    question->correct_answer = (char *)json_string_value(answer);

    for(int i = 0; i < 3; i++)
    {
        // strcpy(shuffled_choices[i], json_string_value(json_array_get(choices_array, i)));
//        printf("\n%d. %s", i+1, json_string_value(json_array_get(choices_array, i)));
        strcpy((question->choices[i]), json_string_value(json_array_get(choices_array, i)));
    }
    /* We add the correct answer to the choices too*/
    //strcpy(shuffled_choices[3], json_string_value(answer));
    strcpy((question->choices[3]), json_string_value(answer));

    shuffle(question->choices, 4);

    //question->choices = shuffled_choices;
    for(int i = 0; i < 4; i++)
    {
        //printf("\n%d. %s", i+1, shuffled_choices[i]);
    }

}

void display_question(struct question *question)
{
    mvprintw(10,4,"----%s----", question->question);
    for(int i = 0; i < 4; i++)
    {
        //printf("\n %d. %s", i+1, question->choices[i]);
        mvprintw(13 + i, 4, "%d. %s", i+1, question->choices[i]);
    }
    //printw("\n----\n");

}

int check_answer(struct question *question, int answer)
{
    if(strcmp(question->correct_answer, question->choices[answer-1]) == 0)
        return 1;
    else
        return 0;
}

void start_singleplayer();
void start_multiplayer();
void display_logo();
void display_initial_screen();
void update_score();
void reset_game();
int main(int argc, char const *argv[])
{
    /*
    select_choice();
    host() || join_game(gameid);
    request_questions();
    play_quiz()
    update_db on each question
    done
    */
    char ch;


    signal(SIGALRM, check_timer);
    initscr();				/* start the curses mode */
    start_color();


    init_pair(1, COLOR_BLACK, COLOR_WHITE); // For the timer
    init_pair(2, COLOR_WHITE, COLOR_GREEN); // Correct answer
    init_pair(3, COLOR_RED, COLOR_WHITE); // Incorrect answer
    init_pair(4, COLOR_BLACK, COLOR_YELLOW); // Incorrect answer

    clear();
    display_initial_screen();
    refresh();
    reset_game();
    while(1)
    {

        int is_quit_signal = 0;
        if((ch = getch()) == ERR)
        {
            
        }
        else{
            switch(ch)
            {
                case '1':
                    mvprintw(0,0,"jkhvgcfxcvhjkl"); refresh();
                    start_singleplayer();
                    clear();
                    display_initial_screen();
                    refresh();
                    reset_game();
                    break;
                case '2':
                    start_multiplayer();
                    clear();
                    display_initial_screen();
                    refresh();
                    reset_game();

                case '3':
                    start_multiplayer_as_guest(12);
                    clear();
                    display_initial_screen();
                    refresh();
                    reset_game();

                case 'q':
                case 'Q':
                    is_quit_signal = 1;
                    break;
                default:
                    mvprintw(14, 12, "Enter a valid option");
                    move(10, 28);
                    refresh();
            }   
        }
    if(is_quit_signal)
        break;
    }
    current_game_info.is_game_in_progress = 0;
    endwin();

    return 0;

}

void start_singleplayer()
{
    json_t *json_questions_array, *json_question;
    struct question *question;
    question = malloc(sizeof(struct question));
    int current_question_number = 0;

    current_game_info.game_mode = GAME_MODE_SINGLEPLAYER;
    current_game_info.is_game_in_progress = 1;

    char mesg[]="Enter an answer( 1 or 2 or 3 or 4) : ", ch;		/* message to be appeared on the screen */
    int row,col;				/* to store the number of rows and *
                        * the number of colums of the screen */

    nodelay(stdscr, TRUE);
    getmaxyx(stdscr,row,col);		/* get the number of rows and columns */
                            /* print the message at the center of the screen */
    refresh();

    json_questions_array = load_questions_array();

    json_array_foreach(json_questions_array, current_question_number, json_question)
    {
        //The answer that will be inputted by the user.
        int user_answer = 0;
        time_left = TIME_PER_QUESTION;
        check_timer();
        parse_question(json_question, question);
        clear();
        display_logo();
        display_question(question);
        update_score();
        //printf("Enter an answer( 1 or 2 or 3 or 4) : ");
        mvprintw(18,(col-strlen(mesg))/2,mesg);
        //scanf("%d", &user_answer);
        for (;;) {
            if(is_timeup)
            {
                mvprintw(23, 4, "Uh-oh , Time's up ! Press any key to continue ");
                break;
            }
            if ((ch = getch()) == ERR) {

            }
            else {
                if(ch == '1' || ch == '2' || ch == '3' || ch == '4')
                {
                    int time_taken = TIME_PER_QUESTION - time_left;
                    time_left = -1;


                    int choice = ch - 48;
                    if(check_answer(question, choice))
                    {
                        //THe answer is correct if we get here.
                        current_game_info.score += 20 - time_taken;
                        attron(COLOR_PAIR(2));
                        mvprintw(22, 4, "  %s  ", question->choices[choice - 1]);
                        attroff(COLOR_PAIR(2));
                    }
                    else
                    {
                        attron(COLOR_PAIR(3));
                        mvprintw(22, 4, "  %s  ", question->choices[choice - 1]);
                        attroff(COLOR_PAIR(3));
                        printw("  ");
                        attron(COLOR_PAIR(2));
                        printw("  %s  ", question->correct_answer);
                        attroff(COLOR_PAIR(2));

                    }

                    //mvprintw(12, 4, "The answer is %d", check_answer(question, choice));
                    update_score();
                    mvprintw(23, 4, "Press any key to continue ");
                    break;
                }
            }
        }

        //Wait for user interaction before proceeding.
        for (;;)
        {
            if ((ch = getch()) == ERR) {

            }
            else {
                break;
            }
        }
    }
    getch();
}

void check_timer()
{
    if(current_game_info.is_game_in_progress)
    {
        int x, y;
        getyx(stdscr, y, x);
        if(current_game_info.game_mode == GAME_MODE_MULTIPLAYER && current_game_info.state >= OPPONENT_JOINED)
        {
            attron(COLOR_PAIR(4));
            mvprintw(4, getmaxx(stdscr) - 40, "--Opponent--");
            move(5,  getmaxx(stdscr) - 40);
            clrtoeol();
            move(6,  getmaxx(stdscr) - 40);
            clrtoeol();
            mvprintw(5, getmaxx(stdscr) - 40, "Score : %d", opponent.score);
            mvprintw(6, getmaxx(stdscr) - 40, "Progress : %d/10", opponent.current_question_number);
            attroff(COLOR_PAIR(4));
        }

        if(time_left > 0)
        {
            time_left--;
            is_timeup = 0;
            alarm(1);
            attron(COLOR_PAIR(1));
            mvprintw(2, getmaxx(stdscr) - 80, "TIME LEFT: %2d", time_left);
            attroff(COLOR_PAIR(1));
            //move(y,x);
        }
        else
        {
            // move(y,x);
            time_left = TIME_PER_QUESTION;
            is_timeup = 1;
        }
        move(y,x);
    }
}

void display_logo()
{
    char logo[250] = "\
    ___            _\n \
  / _ \\   _   _  (_)  ____  ____  _   _ \n \
 | | | | | | | | | | |_  / |_  / | | | | \n \
 | |_| | | |_| | | |  / /   / /  | |_| | \n \
  \\__ \\   \\__,_| |_| /___| /___| \\__,  | \n \
                                  |___/ \n\n";

    mvprintw(0,0,logo);

}

void display_initial_screen()
{
    display_logo();
    mvprintw(9, 12, "1. Start Singleplayer quiz");
    mvprintw(10, 12, "2. Host a Multiplayer quiz");
    mvprintw(11, 12, "3. Join a Multiplayer quiz");
    mvprintw(13, 12, "Enter a choice: ");
}

void update_progress_online()
{
        char *text;
        char url[URL_SIZE];
        json_t *data;
        sprintf(url, "%s/%d/%s.json",BASE_URL_FORMAT, current_game_info.id, current_game_info.player_role_text);
        data = json_object();
        json_object_set(data, "score", json_integer(current_game_info.score));
        json_object_set(data, "current_question", json_integer(current_game_info.progress));
        char *d = json_dumps(data, 0);
        text = patch(url, data);

        json_error_t error;
        json_t *root = json_loads(text, 0, &error);
        if(json_is_null(root))
        {
            clear();
            mvprintw(0,0, "Quizzy encountered an error... Exiting...");
            refresh();
            sleep(5);
            exit(0);
        }
}
void update_score()
{
    attron(COLOR_PAIR(1));
    mvprintw(2, getmaxx(stdscr) - 60, "Score : %d", current_game_info.score);
    mvprintw(2, getmaxx(stdscr) - 40, "Progress : %d/10", current_game_info.progress);
    refresh();
    attroff(COLOR_PAIR(1));
    if(current_game_info.game_mode == GAME_MODE_MULTIPLAYER)
    {
        update_progress_online();
    }
}

void reset_game()
{
    current_game_info.score = 0;
    current_game_info.game_mode = -1;
    current_game_info.is_game_in_progress = 0;
    current_game_info.player_role = -1;
}


json_t * build_initial_json(int id)
{
    json_t *root, *player_info, *player;
    json_error_t error;

    char id_str[20];
    char player_name[15];
    if(current_game_info.player_role == GAMER_ROLE_HOST)
    {
        strcpy(player_name, "player_host");
    }
    else
    {
        strcpy(player_name, "player_guest");
    }
    snprintf(id_str, 12, "%d", id);
    root = json_object();
    player = json_object();
    player_info = json_object();
    json_object_set(player_info, "score", json_integer(-1));
    json_object_set(player_info, "current_question", json_integer(-1));

    json_object_set(player,player_name, player_info);
    json_object_set(root, id_str, player);

    //printf("%s", json_dumps(root, 0));

    return root;
}

void update_opponent_display()
{
    int x, y;
    getyx(stdscr, y, x);
    attron(COLOR_PAIR(1));
    mvprintw(3, getmaxx(stdscr) - 40, "--Opponent--");
    mvprintw(4, getmaxx(stdscr) - 40, "Score : %d", opponent.score);
    mvprintw(5, getmaxx(stdscr) - 40, "Progress : %d/10", opponent.current_question_number);
    attroff(COLOR_PAIR(1));
    move(y,x);
    refresh();
}

/*thread function definition*/
void* check_firebase(void* args)
{
    static int count = 0;
    while(1)
    {
        // count++;
        char *text;
        char url[URL_SIZE];
        sprintf(url, "%s/%d/%s.json",BASE_URL_FORMAT, current_game_info.id, opponent.player_role_text);
        text = request(url);
        // mvprintw(0, 5, "%s", text);
        json_error_t error;
        json_t *root = json_loads(text, 0, &error);
        if(!json_is_null(root) && root != NULL)
        {
            current_game_info.state = OPPONENT_JOINED;
            opponent.score = json_integer_value(json_object_get(root, "score"));
            opponent.current_question_number = json_integer_value(json_object_get(root, "current_question"));
            //update_opponent_display();
        }
        // printf("%s  %d", json_string_value(json_object_get(root, "last")), json_is_object(root));
        // printw("\n\n%s  %d",url, count);
        // printw("\n\nopponent: %d %d", opponent.score, opponent.current_question_number);
        // getch();
        count++;
        //mvprintw(LINES-5,5,"%d", count);
        //printw("ss");
        refresh();
        sleep(1);
    }
}

void start_quizzing_multiplayer(int);

void start_multiplayer()
{
    // Host
    json_t *json;
    char *text = NULL;
    
    /*creating thread id*/
    pthread_t id;
    int ret;
    
    current_game_info.game_mode  = GAME_MODE_MULTIPLAYER;
    current_game_info.player_role =  GAMER_ROLE_HOST;
    current_game_info.id = rand()/10000;
    current_game_info.is_game_in_progress = 1;
    strcpy(opponent.player_role_text, "player_guest");
    strcpy(current_game_info.player_role_text, "player_host");
    multiplayer_data.session = 1233;

    char url[URL_SIZE];
    clear(); //clear the contents of the screen
    display_logo();
    mvprintw(10, 4, "Creating a game... Please wait" );
    refresh();


    snprintf(url, URL_SIZE,"%s%s", BASE_URL_FORMAT, ".json");

    json = build_initial_json(current_game_info.id); // TODO: Randomize this text
    json_dumps(json, 0);

    text = patch(url, json);
    move(10,0);
    clrtoeol();
    if(text != NULL)
    {
        mvprintw(10, 4, "Game Created - ID: %d", current_game_info.id);
        mvprintw(11, 4, "Waiting for opponent to join...");
        current_game_info.state = WAITING_FOR_OPPONENT_TO_JOIN;
        /*creating thread for polling the databse*/
        ret=pthread_create(&id,NULL,&check_firebase,NULL);
        if(ret != 0){
            clear();
            mvprintw(3, 4, "An error occurred :( \n\nExiting ...");
            sleep(5);
            return;
        }
    }
    else
    {
        mvprintw(10, 4, "An error occured... Exiting...");
        sleep(4);
        reset_game();
        return;
    }
    //Wait for user interaction before proceeding.
    for (;;)
    {
        if(current_game_info.state == OPPONENT_JOINED)
        {
            start_quizzing_multiplayer(1);
            //TODO: Add a results screen here
            break;
        }
    }
}


void start_quizzing_multiplayer(int is_host)
{
    json_t *json_questions_array, *json_question;
    struct question *question;
    question = malloc(sizeof(struct question));
    int current_question_number = 0;

    current_game_info.state = IN_QUESTION;
    //current_game_info.game_mode = GAME_MODE_SINGLEPLAYER;
    current_game_info.is_game_in_progress = 1;

    char mesg[]="Enter an answer( 1 or 2 or 3 or 4) : ", ch;		/* message to be appeared on the screen */
    int row,col;				/* to store the number of rows and *
                        * the number of colums of the screen */

    nodelay(stdscr, TRUE);
    getmaxyx(stdscr,row,col);		/* get the number of rows and columns */
                            /* print the message at the center of the screen */
    refresh();

    json_questions_array = load_questions_array();

    json_array_foreach(json_questions_array, current_question_number, json_question)
    {
        //The answer that will be inputted by the user.
        int user_answer = 0;

        current_game_info.progress = current_question_number + 1;
        time_left = TIME_PER_QUESTION;
        check_timer();
        parse_question(json_question, question);
        clear();
        display_logo();
        display_question(question);
        update_score();
        //update_progress_online();
        //printf("Enter an answer( 1 or 2 or 3 or 4) : ");
        mvprintw(18,(col-strlen(mesg))/2,mesg);
        //scanf("%d", &user_answer);
        for (;;) {
            if(is_timeup)
            {
                mvprintw(23, 4, "Uh-oh , Time's up ! Press any key to continue ");
                break;
            }
            if ((ch = getch()) == ERR) {

            }
            else {
                if(ch == '1' || ch == '2' || ch == '3' || ch == '4')
                {
                    int time_taken = TIME_PER_QUESTION - time_left;
                    time_left = -1;


                    int choice = ch - 48;
                    if(check_answer(question, choice))
                    {
                        //THe answer is correct if we get here.
                        current_game_info.score += 20 - time_taken;
                        attron(COLOR_PAIR(2));
                        mvprintw(22, 4, "  %s  ", question->choices[choice - 1]);
                        attroff(COLOR_PAIR(2));
                    }
                    else
                    {
                        attron(COLOR_PAIR(3));
                        mvprintw(22, 4, "  %s  ", question->choices[choice - 1]);
                        attroff(COLOR_PAIR(3));
                        printw("  ");
                        attron(COLOR_PAIR(2));
                        printw("  %s  ", question->correct_answer);
                        attroff(COLOR_PAIR(2));

                    }

                    //mvprintw(12, 4, "The answer is %d", check_answer(question, choice));
                    update_score();
                    mvprintw(23, 4, "Press any key to continue ");
                    break;
                }
            }
        }

        //Wait for user interaction before proceeding.
        for (;;)
        {
            if ((ch = getch()) == ERR) {

            }
            else {
                break;
            }
        }
    }
    getch();
}

json_t * build_initial_json_for_guest()
{
    json_t *root, *player_info;
    root = json_object();
    player_info = json_object();
    json_object_set(player_info, "current_question", json_integer(current_game_info.progress));
    json_object_set(player_info, "score", json_integer(current_game_info.score));

    json_object_set(root, "player_guest", player_info);

    return root;
}

void start_multiplayer_as_guest(int game_id)
{
    // Guest
    json_t *json;
    char *text = NULL;

    /*creating thread id*/
    pthread_t id;
    int ret;
    
    current_game_info.game_mode  = GAME_MODE_MULTIPLAYER;
    current_game_info.player_role =  GAMER_ROLE_GUEST;
    // current_game_info.id = 12323;
    current_game_info.is_game_in_progress = 1;
    strcpy(opponent.player_role_text, "player_host");
    multiplayer_data.session = 1233;
    current_game_info.score = 44444;
    strcpy(current_game_info.player_role_text, "player_guest");
    char url[URL_SIZE];
    clear(); //clear the contents of the screen
    display_logo();
    mvprintw(10, 4, "Enter the Game ID (Can be found from the host): " );
    refresh();
    scanw("%d", &current_game_info.id);

    snprintf(url, URL_SIZE,"%s%s%d%s", BASE_URL_FORMAT, "/", current_game_info.id, ".json");
    //mvprintw(20, 10, url);
    refresh();
    json = build_initial_json_for_guest(); // TODO: Randomize this text
    json_dumps(json, 0);

    text = patch(url, json);
    printw(text);
    refresh();
    // move(10,0);
    // clrtoeol();
    if(text != NULL)
    {
        mvprintw(10, 4, "Joined game %d", current_game_info.id);
        mvprintw(11, 4, "Starting game . . .");
        current_game_info.state = OPPONENT_JOINED;
        /*creating thread for polling the databse*/
        ret=pthread_create(&id,NULL,&check_firebase,NULL);
        if(ret != 0){
            clear();
            mvprintw(3, 4, "An error occurred :( \n\nExiting ...");
            sleep(5);
            return;
        }
    }
    else
    {
        mvprintw(10, 4, "An error occured... Exiting...");
        sleep(4);
        reset_game();
        return;
    }
    //Wait for user interaction before proceeding.
    for (;;)
    {
        if(current_game_info.state == OPPONENT_JOINED)
        {
            start_quizzing_multiplayer(0);
            break;
        }
    }
}