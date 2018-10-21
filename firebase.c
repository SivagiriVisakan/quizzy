
#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include <curl/curl.h>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */

#define URL_FORMAT   "https://quizzy-4d728.firebaseio.com/games/%s.json"
#define GAME_DATA_URL_FORMAT   "https://quizzy-4d728.firebaseio.com/games/%s.json"
#define URL_SIZE     2560


struct write_result
{
    char *data;
    int pos;
};


static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static int request_and_save_to_file(const char *url, FILE *fp)
{
    CURL *curl = NULL;
    CURLcode status;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if(!data)
        goto error;

    curl_easy_setopt(curl, CURLOPT_URL, url);

    // curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    //curl_slist_free_all(headers);
    curl_global_cleanup();

    /* zero-terminate the result */
    return 1;

error:
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    // if(headers)
    //     curl_slist_free_all(headers);
    curl_global_cleanup();
    return 0;
}

static char *request(const char *url)
{
    CURL *curl = NULL;
    CURLcode status;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if(!data)
        goto error;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* GitHub commits API v3 requires a User-Agent header */
    //headers = curl_slist_append(headers, "User-Agent: Jansson-Tutorial");
    //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    //curl_slist_free_all(headers);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;

error:
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    // if(headers)
    //     curl_slist_free_all(headers);
    curl_global_cleanup();
    return NULL;
}


static char * patch(char *url, json_t *patch_data)
{
    CURL *curl = NULL;
    CURLcode status;
    char *data = NULL;
    char *post_data = NULL;
    long code;
    struct curl_slist *headers;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if(!data)
        goto error;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    post_data = json_dumps(patch_data, 0);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    /* GitHub commits API v3 requires a User-Agent header */
    headers = curl_slist_append(headers, "Content-Type: application/json-patch+json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    
    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        printf("%s", data);
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    //curl_slist_free_all(headers);
    curl_global_cleanup();
    free(post_data);

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;

error:
    free(post_data);
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    // if(headers)
    //     curl_slist_free_all(headers);
    curl_global_cleanup();
    exit(0);
    return NULL;


}

/* 
int main(int argc, char const *argv[])
{
    char *text;
    char url[URL_SIZE];
    sprintf(url, GAME_DATA_URL_FORMAT, "gameid00234");
    text = request(url);
    printf(text);
    json_error_t error;
    json_t *root = json_loads(text, 0, &error);
    printf("%s  %d", json_string_value(json_object_get(root, "last")), json_is_object(root));
    printf("%s",url);

    json_t *test = json_object();
    json_object_set(test, "last", json_string("Stark"));
    printf("\n\n%s\n\n", json_dumps(test, 0));
    text = patch(url, test);
    printf(text);
    return 0;
}


 */