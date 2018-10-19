#include <stdio.h>
#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>
#include <string.h>
/*
    Internally the person who initialtes the match is called the "HOST" and the other person
    who joins the game is the "GUEST"
*/

#define GAME_MODE_HOST 1
#define GAME_MODE_GUEST 1


struct question
{
    char *question;
    char choices[4][100];
    char *correct_answer;
    char *category;
};


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
    mvprintw(3,4,"----%s----", question->question);
    mvprintw(0, 0, "hello");
    for(int i = 0; i < 4; i++)
    {
        //printf("\n %d. %s", i+1, question->choices[i]);
        mvprintw(5 + i, 4, "%d. %s", i+1, question->choices[i]);
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
    json_t *json_questions_array, *json_question;
    struct question *question;
    question = malloc(sizeof(struct question));
    int current_question_number = 0;

    char mesg[]="Enter an answer( 1 or 2 or 3 or 4) : ", ch;		/* message to be appeared on the screen */
    char str[80];
    int row,col;				/* to store the number of rows and *
                        * the number of colums of the screen */
    initscr();				/* start the curses mode */
    clear();
    nodelay(stdscr, TRUE);
    getmaxyx(stdscr,row,col);		/* get the number of rows and columns */
    //mvprintw(row/2,(col-strlen(mesg))/2,"%s",mesg);
                            /* print the message at the center of the screen */
    refresh();
    json_questions_array = load_questions_array();
    json_array_foreach(json_questions_array, current_question_number, json_question)
    {
        //The answer that will be inputted by the user.
        int user_answer = 0;
        parse_question(json_question, question);
        clear();
        display_question(question);

        //printf("Enter an answer( 1 or 2 or 3 or 4) : ");
        mvprintw(10,(col-strlen(mesg))/2,mesg);
        //scanf("%d", &user_answer);
        for (;;) {
            if ((ch = getch()) == ERR) {

            }
            else {
                if(ch == '1' || ch == '2' || ch == '3' || ch == '4')
                {
                    int choice = ch - 48;
                    mvprintw(11, 4, "Your answer : %s", question->choices[choice - 1]);
                    mvprintw(12, 4, "The answer is %d", check_answer(question, choice));
                    mvprintw(13, 4, "Press any key to continue");
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
    endwin();
    return 0;

}

