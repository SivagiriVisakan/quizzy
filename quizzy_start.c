#include <stdio.h>
#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>

#include <string.h>
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


int time_left = TIME_PER_QUESTION;
int is_timeup = 0;
struct question
{
    char *question;
    char choices[4][100];
    char *correct_answer;
    char *category;
};

struct game_data
{
    int is_game_in_progress;
    int game_mode; //
    int player_role; // Host or guest player.
    int score;
    int id;
}current_game_info = {0, -1, -1, 0, 110};

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

json_t * load_questions_array()
{
    // The root node of the questions data returned by OpenTriviaDb
    json_t *root, *response_code, *questions;
    json_error_t *error;
    root = json_load_file("questions.json", 0, error);
    
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
                    start_singleplayer();
                    clear();
                    display_initial_screen();
                    refresh();
                    reset_game();
                    break;
                case 'q':
                case 'Q':
                    is_quit_signal = 1;
                    break;
                default:
                    mvprintw(11, 12, "Enter a valid option");
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
                mvprintw(13, 4, "Uh-oh , Time's up ! Press any key to continue ");
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
        for (;;) {
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
        if(time_left > 0)
        {
            time_left--;
            is_timeup = 0;
            alarm(1);
            attron(COLOR_PAIR(1));
            mvprintw(2, getmaxx(stdscr) - 20, "%2d", time_left);
            attroff(COLOR_PAIR(1));
            move(y,x);
        }
        else
        {
            move(y,x);
            time_left = TIME_PER_QUESTION;
            is_timeup = 1;
        }

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
    mvprintw(10, 12, "Enter a choice: ");
}


void update_score()
{
    attron(COLOR_PAIR(1));
    mvprintw(2, getmaxx(stdscr) - 40, "Score : %d", current_game_info.score);
    attroff(COLOR_PAIR(1));

}

void reset_game()
{
    current_game_info.score = 0;
    current_game_info.game_mode = -1;
    current_game_info.is_game_in_progress = 0;
    current_game_info.player_role = -1;
}


void start_multiplayer()
{
    // Host

    
}